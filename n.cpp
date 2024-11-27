#include <ncurses.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#define DISAPPEAR_WAIT 2   // cooldown after car disappearing
#define QUIT_TIME	   3   // .. seconds to quit
#define WAIT_TIME      2   // time to wait during the message
#define QUIT		  'x'  // quit symbol
#define DIS_INT        3   // car disappearing interval in seconds
#define SPEED_INT      5   // speed changing interval in seconds
#define SPEED_RANGE    2   // speed changing range
#define STOP_NUM       2   // amount of cars that will stop near the frog

typedef struct {
    int r;          // number of rows in the playing area
    int c;          // number of columns in the playing area
    int numCars;          // number of cars in the game
    int maxTime;          // time limit for the game
    double *carSpeeds;       // array to store speeds for cars // change to malloc later?
    char frogSymbol;
    char carSymbol;
    char obsSymbol;
    char frogColor[10];
    char carColor[10];
    int frogLives;
    int dis_am;
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
        if (sscanf(line, "frogSymbol=%c", &config->frogSymbol) == 1) continue;
        if (sscanf(line, "carSymbol=%c", &config->carSymbol) == 1) continue;
        if (sscanf(line, "obsSymbol=%c", &config->obsSymbol) == 1) continue;
        // for carSpeeds, parse as a comma-separated lists
        if (strncmp(line, "carSpeeds=", 10) == 0) {
            config->carSpeeds = (double*)malloc(config->numCars * sizeof(double));
            char* speeds = line + 10; // a pointer to the first character after "carSpeeds=" in line to directly access the comma-separated values
            for (int i = 0; i < config->numCars; i++) {
                config->carSpeeds[i] = atoi(strtok(i == 0 ? speeds : NULL, ","));
            }
        }     
    }
    config->dis_am = 0;
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
    int prev_speed;
    int carStopped;
} Car;

typedef struct {
    int x;        // current x-position of the frog
    int y;        // current y-position of the frog
    int lives;    // number of lives the frog has
    int score;    // player's score
    char symbol;
} Frog;

typedef struct {
    char symbol;
    int x;
    int y;
    int numObs;
} Obstacle;

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
    int attempts = 0;
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
                // attempts++;
                // if (attempts > config.r) {
                //     fprintf(stderr, "Failed to find a row\n");
                //     break;
                //     return; 
                // }
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
    }
    free(crows);
}


// setting the initial speed for the cars
void setSpeed(Car *cars, GameConfig config, int MIN_SPEED, int MAX_SPEED){
    for(int i = 0; i<config.numCars; i++){       
        if(config.carSpeeds[i] < MIN_SPEED) cars[i].speed = MIN_SPEED; // if set car speed is below the minimum allowed speed, set it to the minimum allowed speed
        else if(config.carSpeeds[i] > MAX_SPEED) cars[i].speed = MAX_SPEED; // if set car speed is over the maximum allowed speed, set it to the maximum allowed speed
        else {
            cars[i].speed = config.carSpeeds[i]; 
            cars[i].prev_speed = config.carSpeeds[i];
        }
    }
}

// updating cars position based on its last move
void updateCars(Car* cars, GameConfig *config, int dis) {
    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    // convert to a double representing milliseconds
    double current_time = start_time.tv_sec * 1000.0 + start_time.tv_nsec / 1000000.0;
    double elapsedTime, disappearTime;
    for (int i = 0; i < config->numCars; i++) {
        if(cars[i].disapeared == 0){
            elapsedTime = current_time - cars[i].lastMove;
            double interval = 1000/cars[i].speed;
            if (elapsedTime >= interval) { // checking if the interval in ms is <= than elapsedTime, if it is, move the car by 1 column          
                cars[i].cx++;
                cars[i].lastMove = current_time;
                // if car is next to the border, wrap it
                if (cars[i].cx == config->c - 1) {
                    cars[i].cx = 1;
                    // if(config->r > config->numCars){ // do only if there is at least one free row
                        // if(config->dis_am < (config->r - config->numCars)) // car disappears only if there is enough free rows at the moment
                        if(i == dis){
                            cars[i].symbol=' ';
                            cars[i].disapeared = 1;
                            config->dis_am+=1;
                            struct timespec time;
                            clock_gettime(CLOCK_MONOTONIC, &time);
                            // convert to a double representing milliseconds
                            double time_ms = start_time.tv_sec * 1000.0 + start_time.tv_nsec / 1000000.0;
                            cars[i].disappearTime = time_ms;
                        }
                    // }
                }
            }
        }
        else{
            int attempts = 0;
            elapsedTime = current_time - cars[i].disappearTime;
            if(elapsedTime >= DISAPPEAR_WAIT*1000){
                if(config->r > config->numCars){   
                    while(1){
                        int newRow = getRandomStep(2 * config->r , 1, 2);
                        if(newRow != cars[i].prev_pos){ // condition so the car appears in the new row
                            int counter = 0;
                            for(int j=0; j<config->numCars; j++){
                                if(cars[j].row != newRow){
                                    counter++;
                                }
                            }
                            if(counter==config->numCars){ 
                                cars[i].disapeared = 0;
                                config->dis_am-=1;
                                cars[i].row = newRow;
                                cars[i].symbol = config->carSymbol;
                                break;
                            }
                        }
                        attempts++;
                        if (attempts > config->r) { // if amount of attempts to generate a new row for a disappeared car surpasses amount of rows, just keep the previous position
                            cars[i].disapeared = 0;
                            cars[i].row = cars[i].prev_pos;
                            cars[i].symbol = config->carSymbol;
                            break;
                            
                        }
                    }
                } else{
                    cars[i].disapeared = 0;
                    cars[i].row = cars[i].prev_pos;
                    cars[i].symbol = config->carSymbol;
                }
            }
        }
    }
}

void speedChange(Car *cars, GameConfig config, int MIN_SPEED, int MAX_SPEED){
    int car;
    int attempts = 0;
    car = getRandom(config.numCars-1, 0);
    // srand(time(NULL));
    do{
        car = getRandom(config.numCars-1, 0);

        attempts++;
        if (attempts > config.numCars) {
            fprintf(stderr, "Failed to find a car with non-zero speed.\n");
            return; // Exit the function or handle gracefully
        }
    }while(cars[car].speed == 0); // to ensure that we dont move the car that stopped in front of the frog
    int speed_ch = 0;
    attempts = 0;
    
    do{
        speed_ch = getRandom(SPEED_RANGE, -SPEED_RANGE);
        attempts++;
        if (attempts > SPEED_RANGE * 2) {
            speed_ch = 1; // Default to a valid value if takes too long
            break;
        }
    }while(speed_ch == 0); // to ensure that the speed change won't be = 0
    cars[car].speed += speed_ch;
    if(cars[car].speed < MIN_SPEED) cars[car].speed = MIN_SPEED; // if set car speed is below the minimum allowed speed, set it to the minimum allowed speed
    if(cars[car].speed > MAX_SPEED) cars[car].speed = MAX_SPEED; // if set car speed is over the maximum allowed speed, set it to the maximum allowed speed
}

void stopCarGen(Car *cars, Frog frog, GameConfig config){ // generate STOP_NUM amount of stop cars
    int car;
    // srand(time(NULL));
    for(int i=0; i<STOP_NUM; i++){
        while(1){
            
            car = getRandom(config.numCars-1, 0);
            if(cars[car].carStopped == 0) break;
        }
        cars[car].prev_speed = cars[car].speed;
        cars[car].carStopped = 1;
    }
}

void stopCar(Car *cars, Frog frog, GameConfig config){
    for(int i=0; i<config.numCars; i++){
        if(cars[i].carStopped == 1){
            if(frog.x - cars[i].cx <= 2 && frog.x - cars[i].cx >= 1 && cars[i].row == frog.y){
                cars[i].speed = 0;
            }
            else{
                cars[i].speed = cars[i].prev_speed;
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

void drawBuffer(WINDOW* win, char* buffer, GameConfig config, Car *cars) {
    for (int y = 0; y < 2 * config.r + 1; y++) {
        for (int x = 0; x < config.c; x++) {
            char ch = buffer[y * config.c + x];
            wmove(win, y, x); // move to position in window
            if (ch == config.frogSymbol) {
                wattron(win, COLOR_PAIR(1)); // set color for frog
                waddch(win, ch);
                wattroff(win, COLOR_PAIR(1)); // reset color
            } else if (ch == config.carSymbol) {
                for(int i=0; i<config.numCars; i++){
                    if(cars[i].cx == x && cars[i].row == y){
                        if(cars[i].carStopped == 0){
                            wattron(win, COLOR_PAIR(2)); // set color for cars
                            waddch(win, ch);
                            wattroff(win, COLOR_PAIR(2)); // reset color
                        }
                        else{
                            wattron(win, COLOR_PAIR(3)); // set color for cars
                            waddch(win, ch);
                            wattroff(win, COLOR_PAIR(3)); // reset color
                        }
                    }

                }
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
    mvwprintw(status_win, 1, 1, "Timer: %d/%d s | Score: %d | Lives: %d", elapsedTime, config.maxTime, frog.score, frog.lives);
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
    mvwprintw(status_win, 1, 1, "Finish line! Score: %d", frog.score);
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
        buffer[cars[i].row * config.c + cars[i].cx] = cars[i].symbol; 
    }
}

void frogInitPos(GameConfig config, Frog* frog){
        frog->x = config.c / 2;
        frog->y = 2 * config.r - 1;
        frog->symbol = config.frogSymbol;
        
}

void initObs(GameConfig config, Obstacle* obs, int num_obs){
    for(int i=0; i<num_obs; i++){
        obs[i].x = getRandom(config.c-2, 1); // x coordinate of an obstacle ranges between first place beside the border to the last one
        obs[i].y = getRandomStep(2*config.r-1, 2, 2); // y coordinate of an obstacle ranges between the 3rd x of the map, which is a first lane, and the last lane
        obs[i].symbol = config.obsSymbol;
        obs[i].numObs = num_obs;
    }
}

void drawObs(char *buffer, GameConfig config, Obstacle *obs){
    for(int i=0; i<obs[0].numObs; i++){
        buffer[obs[i].y*config.c + obs[i].x] = obs[i].symbol;
    }
}

int frogObs(Frog* frog, Obstacle *obs, int newX, int newY){
    for (int i = 0; i < obs[0].numObs; i++) {
        if (obs[i].x == newX && obs[i].y == newY) {
            return 0; // invalid move
        }
    }
    return 1; // valid move
}

void initCarParams(Car* cars, GameConfig config){
    // cars init position and disappeared status
    for(int i=0; i<config.numCars; i++){
        cars[i].disapeared = 0;
        cars[i].carStopped = 0;
        cars[i].cx = 1;
        cars[i].symbol = config.carSymbol;
    }
}

void newPos(GameConfig config, Frog frog, char *buffer){
    buffer[frog.y * config.c + frog.x] = frog.symbol;
}

void moveFrog(Frog *frog, GameConfig config, int action, Obstacle *obs){
    if (action != ERR) { // if a key is pressed     
            // move the frog based on input
            if ((action == 'w' || action == 'W') && frog->y - 1 != 0 && frogObs(frog, obs, frog->x, frog->y - 1)) frog->y--;
            if ((action == 's' || action == 'S') && frog->y + 1 != 2 * config.r && frogObs(frog, obs, frog->x, frog->y + 1)) frog->y++;
            if ((action == 'a' || action == 'A') && frog->x - 1 != 0 && frogObs(frog, obs, frog->x - 1, frog->y)) frog->x--;
            if ((action == 'd' || action == 'D') && frog->x + 1 != config.c - 1 && frogObs(frog, obs, frog->x + 1, frog->y)) frog->x++;
        }
        napms(10);
}

void mainLoop(Windows windows, char *buffer, GameConfig config, Frog* frog, Car *cars, Obstacle *obs, int num_obs){
    const int MIN_SPEED = 1;
    const int MAX_SPEED = config.c - 2;
    time_t startTime = time(NULL); 
    srand(time(NULL)); // seeding
    setSpeed(cars, config, MIN_SPEED, MAX_SPEED); // setting initial car speed
    initCars(cars, config);
    // initial frog position at the centre of the bottom line
    frogInitPos(config, frog);

    initCarParams(cars, config); // initial car parameters
    int dis;
    initObs(config, obs, num_obs);

    stopCarGen(cars, *frog, config);
    // mvwprintw(windows.game_win, 2, 2, "%d", obs[0].numObs);
    // getchar();
    // input handling
    nodelay(windows.game_win, TRUE);
    int action; // get user input
    int lost = 0; // a flag for checking if the frog lost
    
    while((action = wgetch(windows.game_win)) != QUIT){
        moveFrog(frog, config, action, obs);
        clearBuffer(buffer, config);

        time_t currentTime = time(NULL);
        int elapsedTime = difftime(currentTime, startTime);
        
        // every DIS_INT seconds, a random car will be generated to disappear
        if(elapsedTime % DIS_INT == 0 && elapsedTime > 0){
            dis = getRandom(config.numCars, 0);
        }

        // every SPEED_INT seconds, a random car will be generated to change speed
        if(elapsedTime % SPEED_INT == 0 && elapsedTime > 0){
            speedChange(cars, config, MIN_SPEED, MAX_SPEED);
        }

        // if the time reaches the limit - end the game
        if (elapsedTime == config.maxTime) {
            outTime(windows.status_win, config, *frog, elapsedTime);
            lost = 1;
            napms(WAIT_TIME*1000);
            break;
        }
        stopCar(cars, *frog, config);
        drawBoard(config, buffer); // draw the border

        newPos(config, *frog, buffer); // draw the player's new position
        updateCars(cars, &config, dis);
        drawCars(buffer, cars, config);
        drawObs(buffer, config, obs);

        drawBuffer(windows.game_win, buffer, config, cars); // draw the map to the screen
        
        drawStatus(windows.status_win, config, *frog, elapsedTime); // draw the box status

        if (frog->y - 1 == 0) { // finisl line
            frog->score += 1;
            frogFin(windows.status_win, config, *frog, elapsedTime);
            napms(WAIT_TIME*1000);
            startTime += WAIT_TIME;
            frogInitPos(config, frog);
            initCars(cars, config); 
            initCarParams(cars, config);
            num_obs = getRandom(config.r-1, 1); // new amount of obstacles
            obs = (Obstacle *)realloc(obs, num_obs * sizeof(Obstacle));
            initObs(config, obs, num_obs);
            stopCarGen(cars, *frog, config);
            setSpeed(cars, config, MIN_SPEED, MAX_SPEED);
        }

        int hit_result = frogHit(cars, config, frog, buffer, windows.game_win);
        if(hit_result == 1){
            frogLost(windows.status_win, config, *frog, elapsedTime);   
            napms(WAIT_TIME*1000); // wait for 2 seconds
            lost = 1; // flag so that quitGame() menu wont show
            break;            
        }
        else if(hit_result == 2){
            hitStatus(windows.status_win, config, *frog, elapsedTime);   
            napms(WAIT_TIME*1000);
            startTime += WAIT_TIME;
            frogInitPos(config, frog);
            initCars(cars, config);
            initCarParams(cars, config);
            num_obs = getRandom(config.r-1, 1);
            obs = (Obstacle *)realloc(obs, num_obs * sizeof(Obstacle));
            initObs(config, obs, num_obs);
            stopCarGen(cars, *frog, config);
            setSpeed(cars, config, MIN_SPEED, MAX_SPEED);
        }
    }
    if(lost == 0){
        quitGame(windows.status_win, config, *frog);
        napms(QUIT_TIME*1000);
    }
    free(buffer);
    free(cars);
    free(obs);
}

int main() {
    srand(time(NULL)); // seeding
    Frog frog;
    GameConfig config;
    loadConfig(&config, "config.txt");
    frog.lives = config.frogLives;
    frog.score = 0;
    char* buffer = createBuffer(config);

    int num_obs = getRandom(config.r-1, 1);
    Obstacle* obs = (Obstacle*) malloc(num_obs*sizeof(Obstacle));

    initscr(); // initialize ncurses
    curs_set(0);
    cbreak();
    noecho();
    start_color(); // enable colors
    init_pair(1, COLOR_GREEN, COLOR_BLACK); // frog color
    init_pair(2, COLOR_RED, COLOR_BLACK);   // car color
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);   // car color
    
    Windows windows = initWindows(config);    
    keypad(windows.game_win,TRUE);
    refresh();
    if (!windows.game_win) {
    fprintf(stderr, "Failed to create window.\n");
    endwin();
    exit(1);
    }   

    Car* cars = (Car*)malloc(config.numCars * sizeof(Car));
    
    mainLoop(windows, buffer, config, &frog, cars, obs, num_obs);

    endwin(); // end ncurses session

    return 0;
}
