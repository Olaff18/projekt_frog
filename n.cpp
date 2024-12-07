// some lines of code were inspired by dr Malafiejski's demo game
// f.e structure of the game, #define's, while loop

// https://www.tutorialspoint.com/c_standard_library/c_function_realloc.htm - realloc tutorial

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
#define FRIEND         2   // amount friendly taxi cars
#define JUMP_BREAK     0.1 // break in seconds between frog jumps
#define TOP_DISPLAY    5   // amount of top scores to display in the ranking

//------------------------------------------------
//----------------  DATA STRUCTURES --------------
//------------------------------------------------

typedef struct {
    int r;          // number of rows in the playing area
    int c;          // number of columns in the playing area
    int numCars;          // number of cars in the game
    int maxTime;          // time limit for the game
    int *carSpeeds;       // array to store speeds for cars 
    char frogSymbol;
    char carSymbol;
    char obsSymbol;
    char storkSymbol;
    char frogColor[10];
    char carColor[10];
    int frogLives;
} Config;

typedef struct {
    WINDOW* game_win;   // main game window
    WINDOW* status_win; // status menu window
    WINDOW* error_win; // error window for showing non-fatal errors, for fatal errors I use exit(1)
    WINDOW* bonus_win; // window for jumpcount and bonus points
} Windows;

// struct for cars, with parameters: row - cars w, cx - cars x coordinate, speed - cars speed
typedef struct {
    int row;
    int cx;
    int speed;
    double lastMove; // time representation; to capture the point of time of cars last move, to update its position
    double disappearTime;
    int disapeared;
    char symbol;
    int prev_pos;
    int prev_speed;
    int carStopped;
    int carBounce;
    int friendly;
    int ride;       // flag whether a friendly car is giving a ride to a frog or no
    int lvl2;       // whether it is a car generated on lvl2 or not
} Car;

typedef struct {
    int x;          // current x-position of the frog
    int y;          // current y-position of the frog
    int lives;      // number of lives the frog has
    int score;      // player's score
    char symbol;
    struct timespec lastMove;
    int ride;       // checking if the frog is ready to ried a friendly car
    int is_riding;  // checking if the frog is currently riding a friendly car
    int jumpCount;
    int lvl;
} Frog;

typedef struct {
    char symbol;
    int x;
    int y;
    int numObs;
} Obstacle;

typedef struct {
    int x; 
    int y; 
    char symbol;
    struct timespec lastMove;
} Stork;

//------------------------------------------------
//----------------  WINDOW FUNCTIONS -------------
//------------------------------------------------

// initialize windows
Windows initWindows(Config config) {
    Windows windows;
    windows.game_win = newwin(2*config.r + 1, config.c, 0, 0);
    windows.status_win = newwin(5, config.c, 0, config.c+1);
    windows.error_win = newwin(1, config.c, 2*config.r + 1, 0);
    windows.bonus_win = newwin(1, config.c, 5, config.c+1);
    box(windows.status_win, 0, 0); // draw a border for the status menu
    refresh();
    return windows;
}

// initializing necessary functions for window to properly work
void initWinFunc(){
    initscr(); // initialize ncurses
    curs_set(0);
    cbreak();
    noecho();
    start_color(); // enable colors
    init_pair(1, COLOR_GREEN, COLOR_BLACK); // frog color
    init_pair(2, COLOR_RED, COLOR_BLACK);   // enemy car color
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);   // stop car color
    init_pair(4, COLOR_MAGENTA, COLOR_BLACK);   // friendly car color
    init_pair(5, COLOR_CYAN, COLOR_BLACK);   // stork color
}

// displaying the bonus points window
void bonus(Windows windows, int flag, Config config, Frog *frog){
    mvwprintw(windows.bonus_win, 0, 1, "Jumps: %d", frog->jumpCount);
    if(flag == 1){
        if(frog->jumpCount <= 3*config.r){
            mvwprintw(windows.bonus_win, 0, 11, " | Bonus point for jumps!");
            frog->score += 1;
        }
    }
    wrefresh(windows.bonus_win);
}

// check if terminal fits
void sizeErr(Config config){
    int r, c;
    int requiredRows = 2*config.r+2; // gamewin and errwin must fit vertically
    int requiredCols = 2*config.c; // amount of columns must fit horizontally
    getmaxyx(stdscr, r, c);
    if (r < requiredRows || c < requiredCols) {
        // if terminal size is smaller than the required size
        printw("Error: Terminal window is too small to display the game map.\n");
        printw("Required size: %d rows x %d columns\n", requiredRows, requiredCols);
        printw("Current size: %d rows x %d columns\n", r, c);
        refresh();
        napms(1000);
        // wait for user input before exiting
        getchar();
        endwin();
        exit(1);  
    }
}

//------------------------------------------------
//----------------  TIME FUNCTIONS ---------------
//------------------------------------------------

// function to calculate elapsed time in milliseconds
long getElapsedTime(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000;
}

//------------------------------------------------
//---------------- MESSAGE FUNCTIONS -------------  
//------------------------------------------------  

void startGame(){
    const char *frogEmoji =
    "      @..@\n"
    "     (----)\n"
    "    ( >__< )\n"
    "   ^^  ~~  ^^";
    printw("Welcome to the frog game!\n");
    printw("Avoid the cars, dodge the stork, and reach the goal!\n");
    printw("Press any key to start.\n\n");
    printw(frogEmoji);

    refresh();
    getchar();
    clear();
    refresh();
}

void frogLostMes(WINDOW* status_win, Frog frog) {
    werase(status_win);  // clear the status menu
    box(status_win, 0, 0);  // redraw the border
    mvwprintw(status_win, 1, 1, "Game over! Final score: %d", frog.score);
    wrefresh(status_win);  // refresh to display updates
    napms(WAIT_TIME*1000);
}

void hitStatus(WINDOW* status_win, Frog frog) {
    werase(status_win);  // clear the status menu
    box(status_win, 0, 0);  // redraw the border
    mvwprintw(status_win, 1, 1, "Frog hit! Lives left: %d", frog.lives);
    wrefresh(status_win);  // refresh to display updates
    napms(WAIT_TIME*1000);
    
}

void outTimeMes(WINDOW* status_win, Frog frog) {
    werase(status_win);  // clear the status menu
    box(status_win, 0, 0);  // redraw the border
    mvwprintw(status_win, 1, 1, "Out of time! Final score: %d", frog.score);
    wrefresh(status_win);  // refresh to display updates
    napms(WAIT_TIME*1000);
}

void frogFinMes(WINDOW* status_win, Frog frog) {
    werase(status_win);  // clear the status menu
    box(status_win, 0, 0);  // redraw the border
    mvwprintw(status_win, 1, 1, "Finish line! Score: %d", frog.score);
    wrefresh(status_win);  // refresh to display updates
    napms(WAIT_TIME*1000);
}

void quitGameMes(WINDOW* status_win, Frog frog) {
    werase(status_win);  // clear the status menu
    box(status_win, 0, 0);  // redraw the border
    mvwprintw(status_win, 1, 1, "You've decided to quit. Score: %d", frog.score);
    wrefresh(status_win);  // refresh to display updates
    napms(QUIT_TIME*1000);
}

//------------------------------------------------
//----------------  FILE FUNCTIONS ---------------
//------------------------------------------------

void parseLineConfig(Config* config, const char* line) {
    if (sscanf(line, "numRows=%d", &config->r) == 1) return;
    if (sscanf(line, "numCols=%d", &config->c) == 1) return;
    if (sscanf(line, "numCars=%d", &config->numCars) == 1) return;
    if (sscanf(line, "maxTime=%d", &config->maxTime) == 1) return;
    if (sscanf(line, "frogLives=%d", &config->frogLives) == 1) return;
    if (sscanf(line, "frogSymbol=%c", &config->frogSymbol) == 1) return;
    if (sscanf(line, "carSymbol=%c", &config->carSymbol) == 1) return;
    if (sscanf(line, "obsSymbol=%c", &config->obsSymbol) == 1) return;
    if (sscanf(line, "storkSymbol=%c", &config->storkSymbol) == 1) return;
}


int countSpeeds(char *speeds, Config *config){
    int speedCount = 0;
    // count the number of speeds in the string
    for (char* temp = speeds; *temp; temp++) {
        if (*temp == ',' || *temp == '\n') speedCount++;
    }
    if (speedCount == 0 && strlen(speeds) > 0) speedCount = 1; // single speed case
    return speedCount;
}

// function for opening the file and loading the data into the config struct
void loadConfig(Config* config, const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error opening config file");
        getchar();
        endwin();
        exit(1);
    }

    char line[100];
    while (fgets(line, sizeof(line), file)) {
        // parse each configuration line; 
        parseLineConfig(config, line);
        // for carSpeeds, parse as a comma-separated lists
        if (strncmp(line, "carSpeeds=", 10) == 0) {

            config->carSpeeds = (int*)malloc(config->numCars * sizeof(int));
            char* speeds = line + 10; // a pointer to the first character after "carSpeeds=" in line to directly access the comma-separated values
            
            // error if rows != numcars
            if (config->r < config->numCars) {
                fprintf(stderr, "Error: Number rows (%d) < numCars (%d)\n", config->r, config->numCars);
                fclose(file);
                getchar();
                endwin();
                exit(1);
            }

            // error if numcars != countSpeeds
            int c = countSpeeds(speeds, config);
            if (c != config->numCars) {
                fprintf(stderr, "Error: Number of car speeds (%d) does not match numCars (%d)\n", c, config->numCars);
                fclose(file);
                getchar();
                endwin();
                exit(1);
            }

            // tokenizing with delimiter ","
            for (int i = 0; i < config->numCars; i++) {
                config->carSpeeds[i] = atoi(strtok(i == 0 ? speeds : NULL, ","));
            }
        }     
    }
    fclose(file);
}

void saveScore(Windows windows, const char *filename, int score) {
    FILE *file = fopen(filename, "a");
    if (file == NULL) {
        mvwprintw(windows.error_win, 0, 0, "Failed to opent the scores file!");
        wrefresh(windows.error_win);
        return;
    }
    fprintf(file, "%d\n", score);
    fclose(file);
}

int countScores(Windows windows, const char *filename) {
    int count = 0;
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        mvwprintw(windows.error_win, 0, 0, "Failed to opent the scores file!");
        wrefresh(windows.error_win);
        return 1;
    }

    int temp;
    while (fscanf(file, "%d", &temp) == 1) {
        count++;
    }

    fclose(file);
    return count;
}

void sortScores(int *scores, int scoreCount) {
    for (int i = 0; i < scoreCount-1 - 1; i++) {
        for (int j = i + 1; j < scoreCount; j++) {
            if (scores[i] < scores[j]) {
                int temp = scores[i];
                scores[i] = scores[j];
                scores[j] = temp;
            }
        }
    }
}

void rankingErr(Windows windows, int count){
        if(count < TOP_DISPLAY){
        mvwprintw(windows.game_win, 1, 1, "at least %d scores needed for ranking!", TOP_DISPLAY);
        wrefresh(windows.game_win); 
        getchar();
        endwin();
        exit(1);
        
    } 
}

void displayTopScores(Windows windows, const char *filename){
    int scoreCount = countScores(windows, filename);
    int *scores = (int *)malloc(scoreCount * sizeof(int));
    if (scores == NULL) {
        printw("Error with scores pointer");
        refresh();
        getchar();
        endwin();
        exit(1); 
    }
    
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        mvwprintw(windows.error_win, 0, 1, "Failed to opent the scores file!");
        wrefresh(windows.error_win);
        free(scores); 
        return;
    }
    int count;
    // put elements into scores array
    for (int i = 0; i < scoreCount; i++) {
        fscanf(file, "%d", &scores[i]);
        count++; 
    }

    sortScores(scores, scoreCount);
    // clear the window
    werase(windows.game_win);       
    wrefresh(windows.game_win); 
    werase(windows.bonus_win);       
    wrefresh(windows.bonus_win);  
    werase(windows.status_win);       
    wrefresh(windows.status_win);  

    rankingErr(windows, count);

    // print a title
    mvwprintw(windows.game_win, 1, 1, "Top %d Scores:", TOP_DISPLAY);
    // display scores
    for (int i = 0; i < TOP_DISPLAY; i++) {
        mvwprintw(windows.game_win, i + 2, 1, "%d. %d", i + 1, scores[i]);
    }

    mvwprintw(windows.game_win, TOP_DISPLAY+2, 1, "press any character to exit.");

    // refresh the window to show the changes
    wrefresh(windows.game_win);
    getchar();
    free(scores);
}

//------------------------------------------------
//--------------- RANDOM FUNCTIONS ---------------
//------------------------------------------------

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

//------------------------------------------------
//----------------  STORK FUNCTIONS --------------
//------------------------------------------------

void initStork(Stork *stork, Config config) {
    do{
        stork->x = getRandom(config.c-1, 1); // random x position
        stork->y = getRandom(2*config.r, 1); // random y position
    stork->symbol = config.storkSymbol;
    }while(stork->x == (2*config.r - 1) && stork->y == config.r/2); // genereate storks x and y until different than frogs position
}

void mvStork(Stork *stork, Frog frog, Config config) {
    // calculating the direction of movement
    struct timespec currentTime;
    clock_gettime(CLOCK_MONOTONIC, &currentTime); // get high-resolution time
    if (getElapsedTime(stork->lastMove, currentTime) >= 8*JUMP_BREAK*1000) {
        int dx = frog.x - stork->x; // checking if frog is on the left or right side of the stork (or inline)
        int dy = frog.y - stork->y; // checking if the frog is above or below of the stork (or inline)

        if (dx != 0) dx /= abs(dx); // if not inline - divide dx by its absolute value to generate 1 or -1 (left move or right)
        if (dy != 0) dy /= abs(dy); // if not inline - divide dx by its absolute value to generate 1 or -1 (up move or down)

        // updating the storks position
        stork->x += dx;
        stork->y += dy;

        stork->lastMove = currentTime;

        // boundary check for the stork, just to be safe
        if (stork->x < 0) stork->x = 0;
        if (stork->x >= config.c) stork->x = config.c - 1;
        if (stork->y < 0) stork->y = 0;
        if (stork->y >= 2 * config.r) stork->y = 2 * config.r - 1;
    }
}

//------------------------------------------------
//----------------  CAR FUNCTIONS ----------------
//------------------------------------------------

void row(Car* cars, Config *config, int *i, int *rrow, int *flag_in, int *crows){
    if (*i == 0) {
            int rrow = getRandomStep(2 * config->r, 1, 2); // if its the first car, we assign it a row (with a odd y coordinate, because road/lane can only have such)
            cars[0].row = rrow;
            crows[0] = rrow;
        }
        else {
            while (*flag_in == 0) { // generates odd rows, until it generates a unique one

                *rrow = getRandomStep(2 * config->r, 1, 2); 
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
void initCars(Car* cars, Config *config) { 
    // dynamic 1D array, for storing the rows of cars that had been already used, so they don't repeat
    int* crows = (int*)malloc(config->numCars * sizeof(int));
    if (crows == NULL) {
        // handle realloc failure (
            printw("Memory reallocation failed");
            refresh();
            getchar();
            endwin();
            exit(1);
        }

    // for loop executing num_cars times to generate an odd row for each car
    for (int i = 0; i < config->numCars; i++) {
        // flag to check if some row was already assigned to some car: 0 - it was; 1 - it wasn't
        int flag_in = 0;
        // int rrow - to store a current row
        int rrow = 0;
        row(cars, config, &i, &rrow, &flag_in, crows);
        struct timespec start_time;
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        double start_time_ms = start_time.tv_sec * 1000 + start_time.tv_nsec / 1000000;
        cars[i].lastMove = start_time_ms;
        cars[i].ride = 0; // so that the frog doesnt move when the game starts after hit by stork
    }
    free(crows);
}

void initCarParams(Car* cars, Config* config){
    // cars init position and disappeared status
    for(int i=0; i<config->numCars; i++){
        cars[i].disapeared = 0;
        cars[i].carStopped = 0;
        cars[i].cx = 1;
        cars[i].symbol = config->carSymbol;
        cars[i].carBounce = 0;
        cars[i].friendly = 0;
        cars[i].prev_pos = cars[i].row;
    }
}

// setting lvl2 property to 0 for initial cars, so they have original speed after restart on lvl2
void lvl2Zero(Car *cars, Config *config){
    for(int i=0; i<config->numCars; i++){
        cars[i].lvl2 = 0;
    }
}

// setting the initial speed for the cars
void setSpeed(Car *cars, Config *config, int min_sp, int max_sp){
    for(int i = 0; i<config->numCars; i++){       
        if (cars[i].lvl2) { 
            cars[i].speed = getRandom(max_sp, min_sp);
            cars[i].prev_speed = cars[i].speed;
        } else {
            if (config->carSpeeds[i] < min_sp) cars[i].speed = min_sp;
            else if (config->carSpeeds[i] > max_sp) cars[i].speed = max_sp;
            else {
                cars[i].speed = config->carSpeeds[i];
                cars[i].prev_speed = config->carSpeeds[i];
            }
        }
    }
}

void carDisBeh(Car *cars, struct timespec start_time, int i, int dis){
    if(i == dis){ // if car happens to be disappeared, change its parameters
        struct timespec time;
        clock_gettime(CLOCK_MONOTONIC, &time);
        // convert to a double representing milliseconds
        double time_ms = start_time.tv_sec * 1000.0 + start_time.tv_nsec / 1000000.0;
        cars[i].symbol=' ';
        cars[i].disapeared = 1;
        cars[i].disappearTime = time_ms;
        cars[i].carBounce = 0; // if car disappears, its bounce resets so it starts from the left
        cars[i].cx = 1;
    }
}
// logic for updating cars position, when its disappear parameter is set to 0
void carDis0(Frog *frog, Config *config, Car *cars, double current_time, struct timespec start_time, int i, int dis){
    double elapsedTime = current_time - cars[i].lastMove;
    double interval = 1000.0/cars[i].speed;
    if (elapsedTime >= interval) { // checking if the interval in ms is <= than elapsedTime, if it is, move the car by 1 column  
        if (cars[i].speed != 0) { // logic for moving cars that did not stop in front of frog
            if(!cars[i].carBounce){
                cars[i].cx++;
                if(cars[i].ride) frog->x+=1;
            }
            else{
                cars[i].cx--;
                if(cars[i].ride) frog->x-=1;
            }
            cars[i].lastMove = current_time;
            // if car is next to the border, wrap it
            if (cars[i].cx == config->c-1 && !cars[i].friendly) {
                cars[i].cx = 1;
                carDisBeh(cars, start_time, i, dis);
            }
            if(cars[i].cx == config->c-2 && cars[i].friendly){
                cars[i].carBounce = 1;
                if(!frog->ride) carDisBeh(cars, start_time, i, dis); // friendly car can disappear only if frog is not riding it
            }
            if (cars[i].cx == 1 && cars[i].carBounce){
                cars[i].carBounce = 0;
            }
        }   
    }   
}                


// logic for updating cars position, when its disappear parameter is set to 1
void carDis1(Config *config, Car *cars, double current_time, int i){
    int attempts = 0;
    double elapsedTime = current_time - cars[i].disappearTime;
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

// updating cars position based on its last move
void updateCars(Car* cars, Config *config, Frog *frog, int dis) {
    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    // convert to a double representing milliseconds
    double current_time = start_time.tv_sec * 1000.0 + start_time.tv_nsec / 1000000.0;
    for (int i = 0; i < config->numCars; i++) {
        if(cars[i].disapeared == 0){
            carDis0(frog, config, cars, current_time, start_time, i, dis);
        }
        else{
            carDis1(config, cars, current_time, i);
        }
    }
}

void speedChange(Windows windows, Car *cars, Config config, int min_sp, int max_sp, int elapsedTime){
    if(elapsedTime % SPEED_INT == 0 && elapsedTime > 0){
        int car;
        int attempts = 0;
        car = getRandom(config.numCars-1, 0);
        // srand(time(NULL));
        do{
            car = getRandom(config.numCars-1, 0);

            attempts++;
            if (attempts > config.numCars) {
                mvwprintw(windows.error_win, 0, 0, "Failed to find a car with non-zero speed.");
                wrefresh(windows.error_win);
                return; 
            }
        }while(cars[car].speed == 0); // to ensure that we dont move the car that stopped in front of the frog
        int speed_ch = 0;
        attempts = 0;
        
        do{
            speed_ch = getRandom(SPEED_RANGE, -SPEED_RANGE);
            attempts++;
            if (attempts > SPEED_RANGE * 2) {
                speed_ch = 1; // default to a valid value if takes too long
                break;
            }
        }while(speed_ch == 0); // to ensure that the speed change won't be = 0
        cars[car].speed += speed_ch;
        if(cars[car].speed < min_sp) cars[car].speed = min_sp; // if set car speed is below the minimum allowed speed, set it to the minimum allowed speed
        if(cars[car].speed > max_sp) cars[car].speed = max_sp; // if set car speed is over the maximum allowed speed, set it to the maximum allowed speed
        
    }
}

void stopCarGen(Windows windows, Car *cars, Config *config){ // generate STOP_NUM amount of stop cars
    int car;
    if(STOP_NUM <= config->numCars-2){
        for(int i=0; i<STOP_NUM; i++){
            while(1){
                car = getRandom(config->numCars-1, 0);
                if(cars[car].carStopped == 0) break;
            }
        
            cars[car].prev_speed = cars[car].speed;
            cars[car].carStopped = 1;
        }
    } 
    if(STOP_NUM > config->numCars){
            mvwprintw(windows.error_win, 0, 0, "STOP_NUM exceeds numCars!");
            wrefresh(windows.error_win);
        }
    else if(STOP_NUM > config->numCars - 2){
        mvwprintw(windows.error_win, 0, 0, "no STOP_NUM amount of free cars!");
        wrefresh(windows.error_win);
    }
}       
        
void stopCar(Car *cars, Frog frog, Config config){
    for(int i=0; i<config.numCars; i++){
        if(cars[i].carStopped == 1){
            if(frog.x - cars[i].cx <= 2 && frog.x - cars[i].cx >= 0 && cars[i].row == frog.y){
                cars[i].speed = 0;
            }
            else{
                cars[i].speed = cars[i].prev_speed;
            }
        }
    }
}

void fCarGen(Windows windows, Car *cars, Config *config){ // generate FRIEND amount of stop cars
    int car;
    if(FRIEND <= config->numCars - STOP_NUM - 2){
        for(int i=0; i<FRIEND; i++){
            while(1){
                
                car = getRandom(config->numCars-1, 0);
                if(!cars[car].friendly && !cars[car].carStopped) break;
            }
            cars[car].friendly = 1;
        }
    }
    if(FRIEND > config->numCars){
        mvwprintw(windows.error_win, 0, 0, "FRIEND exceeds numCars!");
        wrefresh(windows.error_win);
    }
    else if(FRIEND > config->numCars - STOP_NUM - 2){
        mvwprintw(windows.error_win, 0, 0, "no FRIEND amount of free cars!");
        wrefresh(windows.error_win);
    }
}  

void fCar(Car *cars, Frog* frog, Config config){
    for(int i=0; i<config.numCars; i++){
        if(cars[i].friendly == 1){
            if(frog->x == cars[i].cx && frog->y == cars[i].row && frog->ride){
                cars[i].ride = 1;
                frog->is_riding = 1;
            }
            if(frog->x == cars[i].cx && frog->y == cars[i].row && !frog->ride){
                cars[i].ride = 0;
                frog->is_riding = 0;
            }
        }
    }
}

//------------------------------------------------
//------------- OBSTACLE FUNCTIONS ---------------
//------------------------------------------------

void initObs(Config config, Obstacle* obs, int num_obs){
    for(int i=0; i<num_obs; i++){
        obs[i].x = getRandom(config.c-2, 1); // x coordinate of an obstacle ranges between first place beside the border to the last one
        obs[i].y = getRandomStep(2*config.r-1, 2, 2); // y coordinate of an obstacle ranges between the 3rd x of the map, which is a first lane, and the last lane
        obs[i].symbol = config.obsSymbol;
        obs[i].numObs = num_obs;
    }
}

void obsR(Obstacle **obs, Config config){ // obstacle reallocation function
    int num_obs = getRandom(config.r-1, 1); // new amount of obstacles
    Obstacle *new_obs = (Obstacle *)realloc(*obs, num_obs * sizeof(Obstacle));
    if (new_obs == NULL) {
        // handle realloc failure (
        printw("Memory reallocation failed");
        refresh();
        getchar();
        endwin();
        exit(1);
    }
    *obs = new_obs;
    initObs(config, *obs, num_obs);
}

//------------------------------------------------
//---------------- LEVEL FUNCTIONS ---------------  
//------------------------------------------------

void checkLvl(Frog* frog){
    if(frog->score < 3) frog->lvl = 1;
    else if(frog->score < 5) frog->lvl = 2;
    else frog->lvl = 3;
}

void initLvl2(Car *cars, Config *config, int first, int last){
    for(int i=first; i<=last; i++){
        cars[i].speed = getRandom(config->c-2, 1);
        cars[i].prev_speed = cars[i].speed;
        cars[i].disapeared = 0;
        cars[i].carStopped = 0;
        cars[i].cx = 1;
        cars[i].symbol = config->carSymbol;
        cars[i].carBounce = 0;
        cars[i].friendly = 0;
        cars[i].lvl2 = 1;
    }
}
    
// at lvl2 n-2 cars appear on the map
void lvl2(Car **cars, Config *config){
    if(config->numCars < config->r-2){
        int first = config->numCars;
        config->numCars = config->r-2;
        int last = config->r-3;
        Car *new_cars = (Car *)realloc(*cars, config->numCars * sizeof(Car));
        if (new_cars == NULL) {
        // handle realloc failure (
            printw("Memory reallocation failed");
            refresh();
            getchar();
            endwin();
            exit(1);
        }
        *cars = new_cars;
        initLvl2(*cars, config, first, last);
    }
}

void levels(Frog frog, Stork *stork, Config *config, Car **cars){
    if(frog.lvl==2) lvl2(cars, config);
    if(frog.lvl==3) initStork(stork, *config);
}

//------------------------------------------------
//----------------  FROG FUNCTIONS ---------------
//------------------------------------------------

// function for checking if the frog was hit by a car

void initFrog(Windows windows, Config config, Frog* frog){
        frog->x = config.c / 2;
        frog->y = 2*config.r - 1;
        frog->symbol = config.frogSymbol;
        frog->is_riding = 0;
        frog->ride = 0;
        frog->jumpCount = 0;
        werase(windows.error_win);       
        wrefresh(windows.error_win); 
        werase(windows.bonus_win);
        wrefresh(windows.bonus_win);
}

int frogHit(Car *cars, Config config, Frog* frog, Stork stork) {
            
    for (int i = 0; i < config.numCars; i++) {

        if (!cars || !frog) {
        printw("Invalid pointer: cars or frog is NULL");
        getchar();
        endwin();
        exit(1);
        }
        if (i < 0 || i >= config.numCars) {
            printw("Out of bounds access on cars array: i=%d\n", i);
            getchar();
            endwin();
            exit(1);
        }
        int carHit = cars[i].cx == frog->x && cars[i].row == frog->y && !cars[i].disapeared && !cars[i].friendly;
        int storkHit = stork.x == frog->x && stork.y == frog->y;
        if (carHit || storkHit){
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

int frogObs(Obstacle *obs, int newX, int newY){
    for (int i = 0; i < obs[0].numObs; i++) {
        if (obs[i].x == newX && obs[i].y == newY) {
            return 0; // invalid move
        }
    }
    return 1; // valid move
}

void frogRestart(Windows windows, Config *config, Car **cars, Frog *frog, time_t *startTime, int min_sp, int max_sp, Stork *stork){
    *startTime += WAIT_TIME;
    setSpeed(*cars, config, min_sp, max_sp);
    initFrog(windows, *config, frog);
    checkLvl(frog);
    levels(*frog, stork, config, cars);
    initCars(*cars, config);
    initCarParams(*cars, config);
    stopCarGen(windows, *cars, config);
    fCarGen(windows, *cars, config);
}

void frState(Windows windows, Frog *frog, Config *config, Car** cars, time_t *startTime, Obstacle **obs, int *lost, Stork *stork){
    int min_sp = 1;
    int max_sp = config->c - 2;
    if (frog->y - 1 == 0) { // when frog gets to the finish line
            frog->score += 1;
            bonus(windows, 1, *config, frog);
            frogFinMes(windows.status_win, *frog);
            frogRestart(windows, config, cars, frog, startTime, min_sp, max_sp, stork);
            *obs = NULL;
            obsR(obs, *config);
    }
    else{
        int hit_result = frogHit(
            *cars, *config, frog, *stork);
        if(hit_result == 1){
            frogLostMes(windows.status_win, *frog);   
            *lost = 1; // flag so that quitGame() menu wont show
        }
        else if(hit_result == 2){
            hitStatus(windows.status_win, *frog);   
            frogRestart(windows, config, cars, frog, startTime, min_sp, max_sp, stork);
            obsR(obs, *config);
        }
        else{
            return;
        }
    }
}

void mvFrog(Windows windows, Frog *frog, Config config, int action, Obstacle *obs){
    sizeErr(config);
    struct timespec currentTime;
    clock_gettime(CLOCK_MONOTONIC, &currentTime); // get high-resolution time

    if (getElapsedTime(frog->lastMove, currentTime) >= JUMP_BREAK*1000) {
        if (action != ERR) { // if a key is pressed  
                if (action == 'e' || action == 'E'){
                    if(frog->ride){
                        frog->ride = 0;
                    }
                    else{
                        frog->ride = 1;
                    }
                    }   
                // move the frog based on input, if the frog is not riding a car
                if(!frog->is_riding){
                    if ((action == 'w' || action == 'W') && frog->y - 1 != 0 && frogObs(obs, frog->x, frog->y - 1)) {
                        frog->y--; 
                        frog->jumpCount+=1;
                    }
                    if ((action == 's' || action == 'S') && frog->y + 1 != 2 * config.r && frogObs(obs, frog->x, frog->y + 1)){
                        frog->y++; 
                        frog->jumpCount+=1;
                    }
                    if ((action == 'a' || action == 'A') && frog->x - 1 != 0 && frogObs(obs, frog->x - 1, frog->y)){
                        frog->x--; 
                        frog->jumpCount+=1;
                    }
                    if ((action == 'd' || action == 'D') && frog->x + 1 != config.c - 1 && frogObs(obs, frog->x + 1, frog->y)){
                        frog->x++; 
                        frog->jumpCount+=1;
                    }
                    bonus(windows, 0, config, frog);

                }
            frog->lastMove = currentTime;
        }
    }
    
    napms(10);
}

//------------------------------------------------
//-------------- BUFFER FUNCTIONS ----------------  
//------------------------------------------------

char* createBuffer(Config config) {
    return (char*)malloc((2 * config.r + 1) * config.c * sizeof(char));
}

void clBuffer(char* buffer, Config config) {
    for (int y = 0; y < 2*config.r + 1; y++) {
        for (int x = 0; x < config.c; x++) {
            buffer[y*config.c + x] = ' ';
        }
    }
}

void drawBuffer(WINDOW* win, char* buffer, Config config, Car *cars) {
    for (int y = 0; y < 2 * config.r + 1; y++) {
        for (int x = 0; x < config.c; x++) {
            char ch = buffer[y * config.c + x];
            wmove(win, y, x); // move to position in window to draw
            if (ch == config.frogSymbol) {
                wattron(win, COLOR_PAIR(1)); // set color for frog
                waddch(win, ch);
                wattroff(win, COLOR_PAIR(1)); // reset color
            
            } 
            else if(ch == config.storkSymbol){
                wattron(win, COLOR_PAIR(5)); // set color for stork
                waddch(win, ch);
                wattroff(win, COLOR_PAIR(5)); // reset color
            }
            else if (ch == config.carSymbol) {
                for(int i=0; i<config.numCars; i++){
                    if(cars[i].cx == x && cars[i].row == y){
                        if(cars[i].carStopped){
                            wattron(win, COLOR_PAIR(3)); // set color for enemy cars
                            waddch(win, ch);
                            wattroff(win, COLOR_PAIR(3)); // reset color
                        }
                        else if(cars[i].friendly){
                            wattron(win, COLOR_PAIR(4)); // set color for freindly cars
                            waddch(win, ch);
                            wattroff(win, COLOR_PAIR(4)); // reset color
                        }
                        else{
                            wattron(win, COLOR_PAIR(2)); // set color for stop cars
                            waddch(win, ch);
                            wattroff(win, COLOR_PAIR(2)); // reset color
                        }
                    }

                }
            } 
            else {
                waddch(win, ch); // default white character
            }
        }
    }
    wrefresh(win); 
}

//------------------------------------------------
//---------------- DRAW FUNCTIONS ----------------  
//------------------------------------------------

void drawBoard(Config config, char* buffer) {
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

void drawStatus(WINDOW* status_win, Config config, Frog frog, int elapsedTime) {
    werase(status_win);  // clear the status menu
    box(status_win, 0, 0);  // redraw the border
    mvwprintw(status_win, 1, 1, "Timer: %d/%d s | Score: %d | Lives: %d | Level: %d", elapsedTime, config.maxTime, frog.score, frog.lives, frog.lvl);
    mvwprintw(status_win, 3, 1, "Michal Binek, 203726");
    wrefresh(status_win);  // refresh to display updates
}

void drawCars(char* buffer, Car* cars, Config config) {
    for (int i = 0; i < config.numCars; i++) {
        buffer[cars[i].row * config.c + cars[i].cx] = cars[i].symbol; 
    }
}

void drawObs(char *buffer, Config config, Obstacle *obs){
    for(int i=0; i<obs[0].numObs; i++){
        buffer[obs[i].y*config.c + obs[i].x] = obs[i].symbol;
    }
}

void newPos(Config config, Frog frog, char *buffer){
    buffer[frog.y * config.c + frog.x] = frog.symbol;
}

void strkPos(Config config, Stork stork, char *buffer){
    buffer[stork.y * config.c + stork.x] = stork.symbol;
}

void drawing(Windows windows, Config *config, Car *cars, Frog *frog, Stork *stork, char *buffer, int dis, Obstacle *obs, double elapsedTime){
    drawBoard(*config, buffer); // draw the border
    newPos(*config, *frog, buffer); // draw the player's new position
    stopCar(cars, *frog, *config);
    updateCars(cars, config, frog, dis);
    fCar(cars, frog, *config);
    drawCars(buffer, cars, *config);
    drawObs(buffer, *config, obs);
    if(frog->lvl == 3) strkPos(*config, *stork, buffer); 
    drawBuffer(windows.game_win, buffer, *config, cars); // draw the map to the screen
    drawStatus(windows.status_win, *config, *frog, elapsedTime); // draw the box status

}

//------------------------------------------------
//---------------- OTHER FUNCTIONS ---------------  
//------------------------------------------------

void initFunc(Windows windows, Config* config, Car *cars, Frog *frog, Obstacle *obs, int num_obs, int max_sp, int min_sp){
    setSpeed(cars, config, min_sp, max_sp); // setting initial car speeds
    initCars(cars, config);
    initFrog(windows, *config, frog);
    initCarParams(cars, config); // initial car parameters
    initObs(*config, obs, num_obs); // initialize obstacle parameters
    stopCarGen(windows, cars, config); // generate car id's which will be stopping
    fCarGen(windows, cars, config);    // generate friendly car id'
}

void frB(char *buffer, Car *cars, Obstacle *obs, Config *config){ //free the dynamic arrays
    free(buffer);
    free(cars);
    free(obs);
    free(config->carSpeeds); 
}

void mainLoop(Windows windows, char *buffer, Config *config, Frog* frog, Car **cars, Obstacle *obs, int num_obs, Stork *stork){
    int min_sp= 1; // minimal speed
    int max_sp = config->c - 2; // maximum speed

    // starting the timer
    time_t startTime = time(NULL); 
    
    initFunc(windows, config, *cars, frog, obs, num_obs, max_sp, min_sp);
    nodelay(windows.game_win, TRUE); // non-blocking getch on
    
    int dis; // id of disappeared car
    int action; // get user input
    int lost = 0; // a flag for checking if the frog lost
    
    while((action = wgetch(windows.game_win)) != QUIT && lost != 1){
        mvFrog(windows, frog, *config, action, obs);

        if(frog->lvl==3)mvStork(stork, *frog, *config);

        clBuffer(buffer, *config);

        time_t currTime = time(NULL);
        int elapsedTime = difftime(currTime, startTime); // difference between current time and start of the game in main()
        
        // every DIS_INT seconds, a random car will be generated to disappear
        if(elapsedTime % DIS_INT == 0 && elapsedTime > 0){
            dis = getRandom(config->numCars, 0);
        }

        // every SPEED_INT seconds, a random car will be generated to change speed
        speedChange(windows, *cars, *config, min_sp, max_sp, elapsedTime);
        
        // if the time reaches the limit - end the game
        if (elapsedTime == config->maxTime) {
            outTimeMes(windows.status_win, *frog);
            lost = 1;
            break;
        }
        
        drawing(windows, config, *cars, frog, stork, buffer, dis, obs, elapsedTime);
        frState(windows, frog, config, cars, &startTime, &obs, &lost, stork);
    }
    if(lost == 0){
        quitGameMes(windows.status_win, *frog);
    }
    frB(buffer, *cars, obs, config);
}

int main() {
    srand(time(NULL)); // seeding
    Frog frog;
    Stork stork;
    Config config;

    loadConfig(&config, "config.txt");

    initWinFunc();
    
    Windows windows = initWindows(config);    
    if (!windows.game_win) {
        printw("Failed to create window");
        refresh();
        getchar();
        endwin();
        exit(1);
    }   
    refresh();
    
    frog.lives = config.frogLives;
    frog.score = 0;

    char* buffer = createBuffer(config);

    int num_obs = getRandom(config.r-1, 1); // initial number of obstacles
    Obstacle* obs = (Obstacle*) malloc(num_obs*sizeof(Obstacle));

    // starting the timer for the entire game
    struct timespec currentTime;
    clock_gettime(CLOCK_MONOTONIC, &currentTime); 

    frog.lastMove = currentTime; 
    stork.lastMove = currentTime;
    frog.lvl = 1;

    Car* cars = (Car*)malloc(config.numCars * sizeof(Car));

    lvl2Zero(cars, &config);
    sizeErr(config);
    startGame();
    mainLoop(windows, buffer, &config, &frog, &cars, obs, num_obs, &stork);
    saveScore(windows, "scores.txt", frog.score);
    displayTopScores(windows, "scores.txt");
    endwin(); // end ncurses session

    return 0;
}