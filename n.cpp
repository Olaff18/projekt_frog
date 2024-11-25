#include <ncurses.h>
#include <stdlib.h>
#include <stdio.h>
#define _POSIX_C_SOURCE 199309L
#include <time.h>
#include <string.h>

#define DISAPPEAR_WAIT 2   // cooldown after car disappearing
#define QUIT_TIME	   3							// .. seconds to quit
#define QUIT		  'x'
#define DIS_INT        3   // car disappearing interval in seconds

typedef struct {
    int r;          // number of rows in the playing area
    int c;          // number of columns in the playing area
    int numCars;          // number of cars in the game
    int maxTime;          // time limit for the game
    double *carSpeeds;       // array to store speeds for cars // change to malloc later?
    char frogShape;
    char frogColor[10];
    char carColor[10];
    int frogLives;
} GameConfig;

// function for opening the file and loading the data into the config struct
void loadConfig(GameConfig* config, const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening config file");
        return;
    }


    char line[100];
    while (fgets(line, sizeof(line), file)) {
        // parse each configuration line; 
        if (sscanf(line, "numRows=%d", &config->r) == 1) continue;
        if (sscanf(line, "numCols=%d", &config->c) == 1) continue;
        if (sscanf(line, "numCars=%d", &config->numCars) == 1) continue;
        if (sscanf(line, "maxTime=%d", &config->maxTime) == 1) continue;
        if (sscanf(line, "frogLives=%d", &config->frogLives) == 1) continue;
        // for carSpeeds, parse as a comma-separated lists
        if (strncmp(line, "carSpeeds=", 10) == 0) {
            config->carSpeeds = (double*)malloc(config->numCars * sizeof(double));
            char* speeds = line + 10; // a pointer to the first character after "carSpeeds=" in line to directly access the comma-separated values
            for (int i = 0; i < config->numCars; i++) {
                config->carSpeeds[i] = atoi(strtok(i == 0 ? speeds : NULL, ","));
            }
        }
        
    }
    

    fclose(file);
    
}

typedef struct {
    WINDOW* game_win;   // main game window
    WINDOW* status_win; // status menu window
} Windows;

// initialize windows
Windows initWindows(GameConfig config) {
    Windows windows;
    windows.game_win = newwin(2 * config.r + 1, config.c, 0, 0);
    windows.status_win = newwin(3, config.c, 2 * config.r + 2, 0);
    box(windows.status_win, 0, 0); // draw a border for the status menu
    refresh();
    return windows;
}


// struct for cars, with parameters: row - cars w, cx - cars x coordinate, speed - cars speed
typedef struct {
    int row;
    int cx;
    double speed;
    double lastMove; // time representation; to capture the point of time of cars last move, to update its position
    double disappearTime;
    int disapeared;
    char symbol;
    int prev_pos;
} Car;

typedef struct {
    int x;        // current x-position of the frog
    int y;        // current y-position of the frog
    int lives;    // number of lives the frog has
    int score;    // player's score
    char shape;
} Frog;

// function for generating a random row, with the step = 2, so the values are only odd
int getRandomStep(int max_value, int min_value, int step) {
    int random_value = (rand() % ((++max_value - min_value) / step)) * step + min_value;
    return random_value;
}

// function for generating a random car
int getRandom(int max_value, int min_value) {
    int random_value = rand() % ((++max_value - min_value)) + min_value;
    return random_value;
}

void row(Car* cars, GameConfig config, int *i, int *rrow, int *flag_in, int *crows){
    if (&i == 0) {
            int rrow = getRandomStep(2 * config.r - 1, 1, 2); // if its the first car, we assign it a row (with a odd y coordinate, because road/lane can only have such)
            cars[0].row = rrow;
            crows[0] = rrow;
        }
        else {
            while (*flag_in == 0) { // generates odd rows, until it generates a unique one

                *rrow = getRandomStep(2 * config.r , 1, 2);
                int count = 0;
                for (int j = 0; j < *i; j++) {
                    if (*rrow != crows[j]) {
                        ++count;
                    }
                }
                if (count == *i) {
                    cars[*i].row = *rrow;
                    cars[*i].prev_pos = *rrow;
                    crows[*i] = *rrow;
                    *flag_in = 1;

                }
            }
        }
}

// initializing the cars and its parameters
void initCars(Car* cars, GameConfig config) { 
    
    // dynamic 1D array, for storing the rows of cars that had been already used, so they don't repeat
    int* crows = (int*)malloc(config.numCars * sizeof(int));

    // for loop executing num_cars times to generate an odd row for each car
    for (int i = 0; i < config.numCars; i++) {
        // flag to check if some row was already assigned to some car: 0 - it was; 1 - it wasn't
        int flag_in = 0;
        // int rrow - to store a current row
        int rrow = 0;
        row(cars, config, &i, &rrow, &flag_in, crows);
        struct timespec start_time;
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        double start_time_ms = start_time.tv_sec * 1000 + start_time.tv_nsec / 1000000;
        cars[i].lastMove = start_time_ms;
        cars[i].cx = 1;
        
    }
    free(crows);
}


// setting the initial speed for the cars
void setSpeed(Car *cars, GameConfig config, int MIN_SPEED, int MAX_SPEED){
        for(int i = 0; i<config.numCars; i++){       
        if(config.carSpeeds[i] < MIN_SPEED) cars[i].speed = MIN_SPEED; // if set car speed is below the minimum allowed speed, set it to the minimum allowed speed
        else if(config.carSpeeds[i] > MAX_SPEED) cars[i].speed = MAX_SPEED; // if set car speed is over the maximum allowed speed, set it to the maximum allowed speed
        else cars[i].speed = config.carSpeeds[i];
    }
}

// updating cars position based on its last move
void updateCars(Car* cars, GameConfig config, int dis) {
    // time_t currentTime = time(NULL); // time(NULL) returns current point of time
    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    // convert to a double representing milliseconds
    double current_time = start_time.tv_sec * 1000.0 + start_time.tv_nsec / 1000000.0;
    double elapsedTime, disappearTime;
    for (int i = 0; i < config.numCars; i++) {
        if(cars[i].disapeared == 0){
            elapsedTime = current_time - cars[i].lastMove;
            double interval = 1000/cars[i].speed;
            if (elapsedTime >= interval) { // checking if the interval in ms is <= than elapsedTime, if it is, move the car by 1 column          
                cars[i].cx++;
                cars[i].lastMove = current_time;
                // if car is next to the border, wrap it
                if (cars[i].cx == config.c - 1) {
                    cars[i].cx = 1;
                    if(i == dis){
                        cars[i].symbol=' ';
                        cars[i].disapeared = 1;
                        struct timespec time;
                        clock_gettime(CLOCK_MONOTONIC, &time);
                        // convert to a double representing milliseconds
                        double time_ms = start_time.tv_sec * 1000.0 + start_time.tv_nsec / 1000000.0;
                        cars[i].disappearTime = time_ms;
                    }
                }
            }
        }
        else{
            elapsedTime = current_time - cars[i].disappearTime;
            if(elapsedTime >= DISAPPEAR_WAIT*1000){
                if(config.r > config.numCars){
                    while(1){
                        int newRow = getRandomStep(2 * config.r , 1, 2);
                        if(newRow != cars[i].prev_pos){
                            int counter = 0;
                            for(int j=0; j<config.numCars; j++){
                                if(cars[j].row != newRow){
                                    counter++;
                                }
                            }
                            if(counter==config.numCars){
                                cars[i].disapeared = 0;
                                cars[i].row = newRow;
                                cars[i].symbol='C';
                                break;
                            }
                        }
                    }
                }
                else{
                    cars[i].disapeared = 0;
                }
            }
        }
    }
}

// function for checking if the frog was hit by a car
int frogHit(Car *cars, GameConfig config, Frog* frog, char* buffer, WINDOW* win) {
            
    for (int i = 0; i < config.numCars; i++) {

        if (!cars || !frog) {
        fprintf(stderr, "Invalid pointer: cars or frog is NULL.\n");
        return -1;
        }
        if (i < 0 || i >= config.numCars) {
            fprintf(stderr, "Out of bounds access on cars array: i=%d\n", i);
            return -1;
        }

        if (cars[i].cx == frog->x && cars[i].row == frog->y && cars[i].disapeared == 0){

            frog->lives -= 1;
            if ((frog->lives) == 0) {

                
                free(buffer);
                free(cars); 
                return 1; // other end game logic needed
            }
            return 2; // hit, but lives > 0
        }
    }
    // frog hasn't been hit
    return 0;
}

char* createBuffer(GameConfig config) {
    return (char*)malloc((2 * config.r + 1) * config.c * sizeof(char));
}

void clearBuffer(char* buffer, GameConfig config) {
    for (int y = 0; y < 2 * config.r + 1; y++) {
        for (int x = 0; x < config.c; x++) {
            buffer[y * config.c + x] = ' ';
        }
    }
}

void drawBuffer(WINDOW* win, char* buffer, GameConfig config) {
    for (int y = 0; y < 2 * config.r + 1; y++) {
        for (int x = 0; x < config.c; x++) {
            char ch = buffer[y * config.c + x];
            wmove(win, y, x); // Move to position in window
            if (ch == '@') {
                wattron(win, COLOR_PAIR(1)); // Set color for frog
                waddch(win, ch);
                wattroff(win, COLOR_PAIR(1)); // Reset color
            } else if (ch == 'C') {
                wattron(win, COLOR_PAIR(2)); // Set color for cars
                waddch(win, ch);
                wattroff(win, COLOR_PAIR(2)); // Reset color
            } else {
                waddch(win, ch); // Default character
            }
        }
    }
    wrefresh(win); // Refresh to display changes
}

void drawBoard(GameConfig config, char* buffer) {
    // draw borders
    for (int x = 0; x < config.c; x++) {
        buffer[x] = '#'; // Top border
        buffer[(2 * config.r) * config.c + x] = '#'; // bottom border
    }
    for (int y = 0; y < 2 * config.r + 1; y++) {
        buffer[y * config.c] = '#'; // Left border
        buffer[y * config.c + config.c - 1] = '#'; // right border
    }
    for (int y = 2; y < (2 * config.r); y += 2) {
        for (int x = 1; x < config.c - 1; x++) {
            buffer[y * config.c + x] = '-';        // draw lanes
        }
    }
}

void drawStatus(WINDOW* status_win, GameConfig config, Frog frog, int elapsedTime) {
    werase(status_win);  // clear the status menu
    box(status_win, 0, 0);  // redraw the border
    mvwprintw(status_win, 1, 1, "Timer: %d s | Score: %d | Lives: %d", elapsedTime, frog.score, frog.lives);
    wrefresh(status_win);  // refresh to display updates
}

void frogLost(WINDOW* status_win, GameConfig config, Frog frog, int elapsedTime) {
    werase(status_win);  // clear the status menu
    box(status_win, 0, 0);  // redraw the border
    mvwprintw(status_win, 1, 1, "Game over! Final score: %d", frog.score);
    wrefresh(status_win);  // refresh to display updates
}

void hitStatus(WINDOW* status_win, GameConfig config, Frog frog, int elapsedTime) {
    werase(status_win);  // clear the status menu
    box(status_win, 0, 0);  // redraw the border
    mvwprintw(status_win, 1, 1, "Frog hit! Lives left: %d", frog.lives);
    wrefresh(status_win);  // refresh to display updates
}

void outTime(WINDOW* status_win, GameConfig config, Frog frog, int elapsedTime) {
    werase(status_win);  // clear the status menu
    box(status_win, 0, 0);  // redraw the border
    mvwprintw(status_win, 1, 1, "Out of time! Final score: %d", frog.score);
    wrefresh(status_win);  // refresh to display updates
}

void frogFin(WINDOW* status_win, GameConfig config, Frog frog, int elapsedTime) {
    werase(status_win);  // clear the status menu
    box(status_win, 0, 0);  // redraw the border
    mvwprintw(status_win, 1, 1, "You've reached the finish line! Score: %d", frog.score);
    wrefresh(status_win);  // refresh to display updates
}

void quitGame(WINDOW* status_win, GameConfig config, Frog frog) {
    werase(status_win);  // clear the status menu
    box(status_win, 0, 0);  // redraw the border
    mvwprintw(status_win, 1, 1, "You've decided to quit. Score: %d", frog.score);
    wrefresh(status_win);  // refresh to display updates
}

void drawCars(char* buffer, Car* cars, GameConfig config) {
    for (int i = 0; i < config.numCars; i++) {
        buffer[cars[i].row * config.c + cars[i].cx] = cars[i].symbol; // 'C' represents the car
    }
}

void frogInitPos(GameConfig config, Frog* frog){
        frog->x = config.c / 2;
        frog->y = 2 * config.r - 1;
        frog->shape = '@';
        
}

void initCarParams(Car* cars, GameConfig config){
    // cars init position and disappeared status
    for(int i=0; i<config.numCars; i++){
        cars[i].disapeared=0;
        cars[i].cx = 1;
        cars[i].symbol = 'C';
    }
}

void newPos(GameConfig config, Frog frog, char *buffer){
    buffer[frog.y * config.c + frog.x] = frog.shape;
}

void mainLoop(Windows windows, char *buffer, GameConfig config, Frog* frog, Car *cars){
    time_t startTime = time(NULL); 
    srand(time(NULL)); // ? seeding
    initCars(cars, config);
        // initial frog position at the centre of the bottom line
    frogInitPos(config, frog);

    initCarParams(cars, config); // initial car parameters
    int dis;


    // input handling
    nodelay(windows.game_win, TRUE);
    int action; // get user input
    
    while((action = wgetch(windows.game_win)) != QUIT){
        if (action != ERR) { // if a key is pressed     
            // move the frog based on input
            if ((action == 'w' || action == 'W') && frog->y - 1 != 0) frog->y--;
            if ((action == 's' || action == 'S') && frog->y + 1 != 2 * config.r) frog->y++;
            if ((action == 'a' || action == 'A') && frog->x - 1 != 0) frog->x--;
            if ((action == 'd' || action == 'D') && frog->x + 1 != config.c - 1) frog->x++;
        }
        napms(10);
        clearBuffer(buffer, config);

        time_t currentTime = time(NULL);
        int elapsedTime = difftime(currentTime, startTime);
        
        if(elapsedTime % DIS_INT == 0 && elapsedTime > 0){
            dis = getRandom(config.numCars, 0);
        }
        // if the time reaches the limit - end the game
        if (elapsedTime == config.maxTime) {
            outTime(windows.status_win, config, *frog, elapsedTime);
            napms(2000);
            break;
        }

        drawBoard(config, buffer); // draw the border

        newPos(config, *frog, buffer); // draw the player's new position
        updateCars(cars, config, dis);
        drawCars(buffer, cars, config);

        drawBuffer(windows.game_win, buffer, config); // draw the map to the screen
        
        drawStatus(windows.status_win, config, *frog, elapsedTime); // draw the box status

        if (frog->y - 1 == 0) { // finisl line
            frog->score += 1;
            frogFin(windows.status_win, config, *frog, elapsedTime);
            napms(2000);
            startTime += 2;
            frogInitPos(config, frog);
            initCars(cars, config); 
        }

        int hit_result = frogHit(cars, config, frog, buffer, windows.game_win);
        if(hit_result == 1){
            frogLost(windows.status_win, config, *frog, elapsedTime);   
            napms(2000); // wait for 2 seconds
            
            break;            
        }
        else if(hit_result == 2){
            hitStatus(windows.status_win, config, *frog, elapsedTime);   
            napms(2000);
            startTime += 2;
            frogInitPos(config, frog);
            initCars(cars, config);
        }
    }
    quitGame(windows.status_win, config, *frog);
}

int main() {
    Frog frog;
    GameConfig config;
    loadConfig(&config, "config.txt");
    frog.lives = config.frogLives;

    char* buffer = createBuffer(config);

    initscr(); // initialize ncurses
    cbreak();
    noecho();
    start_color(); // enable colors
    init_pair(1, COLOR_GREEN, COLOR_BLACK); // frog color
    init_pair(2, COLOR_RED, COLOR_BLACK);   // car color
    
    Windows windows = initWindows(config);    
    keypad(windows.game_win,TRUE);
    refresh();
    if (!windows.game_win) {
    fprintf(stderr, "Failed to create window.\n");
    endwin();
    exit(1);
    }   
   
    const int MIN_SPEED = 1;
    const int MAX_SPEED = config.c - 2;


    Car* cars = (Car*)malloc(config.numCars * sizeof(Car));

    setSpeed(cars, config, MIN_SPEED, MAX_SPEED); // setting initial car speed
    
    mainLoop(windows, buffer, config, &frog, cars);

    free(buffer);
    free(cars);

    endwin(); // end ncurses session

    return 0;
}
