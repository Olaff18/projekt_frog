#include <conio.h>
#include <stdio.h>
#include <windows.h>
#include <stdlib.h>
#include <time.h>

#define DISAPPEAR_WAIT 4

typedef struct {
    int r;          // number of rows in the playing area
    int c;          // number of columns in the playing area
    int numCars;          // number of cars in the game
    int maxTime;          // time limit for the game
    double *carSpeeds;       // array to store speeds for cars // change to malloc later?
    char frogShape;
    char frogColor[10];
    char carColor[10];
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

// struct for cars, with parameters: row - cars row, cx - cars x coordinate, speed - cars speed
typedef struct {
    int row;
    int cx = 1; // ????
    double speed;
    double lastMove; // time representation; to capture the point of time of cars last move, to update its position
    double disappearTime;
    int disapeared = 0;
    char symbol = 'C';
    int prev_pos = -1;
} Car;

typedef struct {
    int x;        // current x-position of the frog
    int y;        // current y-position of the frog
    int lives;    // number of lives the frog has
    int score;    // player's score
    char shape;
} Frog;

void green() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, 10);
}

void red() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
}

void white() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}


// function for changing cursors position
void gotoxy(int x, int y) {
    COORD coordinate;
    coordinate.X = x;
    coordinate.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coordinate);
}

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
        DWORD start_time = GetTickCount();
        cars[i].lastMove = static_cast<double>(start_time);
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
    DWORD start_time = GetTickCount();
    double current_time = static_cast<double>(start_time);
    double elapsedTime, disappearTime;
    for (int i = 0; i < config.numCars; i++) {
        if(cars[i].disapeared == 0){
            elapsedTime = current_time - cars[i].lastMove;
            double interval = 1000/cars[i].speed;
            // cars[i].accumulatedTime += elapsedTime;
            if (elapsedTime >= interval) { // checking if the interval in ms is <= than elapsedTime, if it is, move the car by 1 column          
                cars[i].cx++;
                cars[i].lastMove = current_time;
                // if car is next to the border, wrap it
                if (cars[i].cx == config.c - 1) {
                    cars[i].cx = 1;
                    if(i == dis){
                        cars[i].symbol=' ';
                        cars[i].disapeared = 1;
                        DWORD time = GetTickCount();
                        cars[i].disappearTime = static_cast<double>(time);
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
int frogHit(Car *cars, GameConfig config, Frog frog) {
    for (int i = 0; i < config.numCars; i++) {
        if (cars[i].cx == frog.x && cars[i].row == frog.y && cars[i].disapeared == 0) return 1;
    }
    return 0;
}

// function for drawing the entire game screen based on what is stored in the buffer
void drawScreen(char* buffer, GameConfig config) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CHAR_INFO* charBuffer = (CHAR_INFO*)malloc((2 * config.r + 1) * config.c * sizeof(CHAR_INFO));
    COORD bufferSize = { config.c, 2 * config.r + 1 };
    COORD bufferCoord = { 0, 0 };
    SMALL_RECT writeRegion = { 0, 0, config.c - 1, 2 * config.r };

    // fill charBuffer with data from buffer
    for (int y = 0; y < 2 * config.r + 1; y++) {
        for (int x = 0; x < config.c; x++) {
            charBuffer[y * config.c + x].Char.AsciiChar = buffer[y * config.c + x];
            if (buffer[y * config.c + x] == '@') charBuffer[y * config.c + x].Attributes = FOREGROUND_GREEN; // if (y,x) = frog symbol, set the color green
            else if (buffer[y * config.c + x] == 'C') charBuffer[y * config.c + x].Attributes = FOREGROUND_RED; // if (y,x) = car symbol, set the color to red
            else charBuffer[y * config.c + x].Attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // if other symbol, just w  hite text
        }
    }
    // write the charBuffer to the console; sidenote: I used WriteConsoleOutputA specifically for VisualStudio, because it's console treats characters as UNICODE characters, and it leads to display errors
    WriteConsoleOutputA(hConsole, (CHAR_INFO*)charBuffer, bufferSize, bufferCoord, &writeRegion);

}

// draw the current status of the game
void drawStatus(double elapsedTime, GameConfig config, Frog frog) {
    gotoxy(0, 2 * config.r + 2); // move cursor to the line below the game area
    printf("Michal Binek 203726; Timer: %d s/ %d s; Score: %d; Lives: %d", (int)elapsedTime, config.maxTime, frog.score, frog.lives);
}

// clearing the entire buffer with empty spaces
void clearBuffer(char* buffer, GameConfig config) {
    for (int y = 0; y < 2 * config.r + 1; y++) {
        for (int x = 0; x < config.c; x++) {
            buffer[y * config.c + x] = ' ';
        }
    }
}

// putting the car symbol into the buffer at a specified position
void drawCars(char* buffer, Car* cars, GameConfig config) {
    for (int i = 0; i < config.numCars; i++) {
        buffer[cars[i].row * config.c + cars[i].cx] = cars[i].symbol; // 'C' represents the car
    }
}

int main() {
    GameConfig config;
    GameConfig* cnfptr = &config;
    loadConfig(cnfptr, "config.txt");   
    char* buffer = (char*)malloc((2 * config.r + 1) * config.c * sizeof(char));

    const int MIN_SPEED = 1;
    const int MAX_SPEED = config.c - 2;

    int dis = -1; // number of a disappeared car
    Car* cars = (Car*)malloc(config.numCars * sizeof(Car));

    setSpeed(cars, config, MIN_SPEED, MAX_SPEED);
    
    Frog frog;
    // game.maxTime = 60;
    frog.shape = '@';
    frog.lives = 5;
    frog.score = 0;
    char action;
    time_t startTime = time(NULL);

    cars[0].symbol = 'C';
    cars[1].symbol = 'C';
    cars[2].symbol = 'C';
    cars[3].symbol = 'C';
    cars[4].symbol = 'C';
    
repeat:

    srand(time(NULL)); // ? seeding
    initCars(cars, config);

    // initial frog position at the centre of the bottom line
    frog.x = config.c / 2;
    frog.y = 2 * config.r - 1;
    
    for(int i=0; i<config.numCars; i++){
        cars[i].disapeared=0;
        cars[i].cx = 1;
    }

    system("cls"); // clear the screen once at the start

    do {
        clearBuffer(buffer, config);

        time_t currentTime = time(NULL);
        int elapsedTime = difftime(currentTime, startTime);
        

        if(elapsedTime % 5 == 0 && elapsedTime > 0){
            // disappearing car
            // do {
                dis = getRandom(config.numCars, 0);
            // } while (cars[dis].disapeared == 0);
            gotoxy(0, 2 * config.r + 3);
            printf("dis: %d", dis);
        }
        

        // if the time reaches the limit - end the game
        if (elapsedTime == config.maxTime) {
            gotoxy(0, 2 * config.r + 3);
            printf("Out of time! Your score is: %d", frog.score);
            Sleep(2000);
            return 0;
        }

        // draw the border
        for (int x = 0; x < config.c; x++) { // rows
            buffer[x] = '#';
            buffer[2 * config.r * config.c + x] = '#';
        }
        for (int y = 0; y < 2 * config.r + 1; y++) { // columns
            buffer[y * config.c] = '#';
            buffer[y * config.c + config.c - 1] = '#';
        }
        // draw lanes
        for (int y = 2; y < (2 * config.r); y += 2) {
            for (int x = 1; x < config.c - 1; x++) {
                buffer[y * config.c + x] = '-';
            }
        }

        // draw the player's new position
        buffer[frog.y * config.c + frog.x] = frog.shape;
        updateCars(cars, config, dis);
        drawCars(buffer, cars, config);

        drawScreen(buffer, config);
        drawStatus(elapsedTime, config, frog);
        // when frog gets to the finish line
        if (frog.y - 1 == 0) {
            frog.score += 1;
            gotoxy(0, 2 * config.r + 3);
            printf("You won! Your score is: %d", frog.score);
            Sleep(2000);
            startTime += 2;
            goto repeat;

        }


        // check for user input
        if (_kbhit()) {
            action = _getch();
            if (action == 'x') {
                break;
            }

            // move the player based on input
            if ((action == 'w' || action == 'W') && frog.y - 1 != 0) frog.y--;
            if ((action == 's' || action == 'S') && frog.y + 1 != 2 * config.r) frog.y++;
            if ((action == 'a' || action == 'A') && frog.x - 1 != 0) frog.x--;
            if ((action == 'd' || action == 'D') && frog.x + 1 != config.c - 1) frog.x++;
        }
        // check if the frog hit the car, if yes - restart the game and -1 life
        if (frogHit(cars, config, frog) == 1) {
            if (--frog.lives == 0) {
                gotoxy(0, 2 * config.r + 3);
                printf("You lost. Your score was: %d", frog.score);
                Sleep(2000);
                free(buffer);
                free(cars); 
                return 0;
            }
            gotoxy(0, 2 * config.r + 3);
            printf("Frog got ran over:( Try again. Lives remaining: %d", frog.lives);
            Sleep(2000);
            startTime += 2;
            goto repeat;
        }
    } while (1);

    free(buffer);
    free(cars);

    return 0;
}

// updates:
// cleared unnecessary comments
// new concept of speed: frequency instead of periods - car speed is columns/s now
// speed limits
// concept of disappearing cars - every x seconds a random car will disappear, with x second cooldown, will appear in a new row

// to add:
// frogShape, frog and car color
// if numCars > numLanes -> error
// when lost print that lives 0
// when press 'x' print: you exited the game
// discard of code in main(), create more functions
