#include <conio.h>
#include <stdio.h>
#include <windows.h>
#include <stdlib.h>
#include <time.h>


typedef struct {
    int r;          // number of rows in the playing area
    int c;          // number of columns in the playing area
    int numCars;          // number of cars in the game
    int maxTime;          // time limit for the game
    int *carSpeeds; ;     // array to store speeds for cars // change to malloc later?
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
            config->carSpeeds = (int*)malloc(config->numCars * sizeof(int));
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
    int cx;
    int speed;
    time_t lastMove; // Time representation; to capture the point of time of cars last move, to update its position
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
int GetRandom(int max_value, int min_value, int step) {
    int random_value = (rand() % ((++max_value - min_value) / step)) * step + min_value;
    return random_value;
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

        if (i == 0) {
            int rrow = GetRandom(2 * config.r - 1, 1, 2); // if its the first car, we assign it a row (with a odd y coordinate, because road/lane can only have such)
            cars[i].row = rrow;
            crows[i] = rrow;
        }
        else {
            while (flag_in == 0) { // generates odd rows, until it generates a unique one

                rrow = GetRandom(2 * config.r , 1, 2);
                int count = 0;
                for (int j = 0; j < i; j++) {
                    if (rrow != crows[j]) {
                        ++count;
                    }
                }
                if (count == i) {
                    cars[i].row = rrow;
                    crows[i] = rrow;
                    flag_in = 1;

                }
            }
        }

        cars[i].lastMove = time(NULL); // time(NULL) returns current point of time
        cars[i].cx = 1;
        
    }
    free(crows);
 }


// updating cars position based on its last move
void updateCars(Car* cars, GameConfig config) {
    time_t currentTime = time(NULL); // time(NULL) returns current point of time

    for (int i = 0; i < config.numCars; i++) {
        if ((currentTime - cars[i].lastMove) >= cars[i].speed) { // substracting current time and time of the last move and comparing it to cars speed (in seconds), gives us the answer whether we should move the car or no
            // move car to new position
            cars[i].cx++;
            cars[i].lastMove = currentTime;
            // if car is next to the border, wrap it
            if (cars[i].cx == config.c - 1) {
                cars[i].cx = 1;
            }
        }
    }
}
// function for checking if the frog was hit by a car
int frogHit(Car *cars, GameConfig config, Frog frog) {
    for (int i = 0; i < config.numCars; i++) {
        if (cars[i].cx == frog.x && cars[i].row == frog.y) return 1;
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
        buffer[cars[i].row * config.c + cars[i].cx] = 'C'; // 'C' represents the car
    }
}


int main() {
    GameConfig config;
    GameConfig* cnfptr = &config;
    loadConfig(cnfptr, "config.txt");
    
    char* buffer = (char*)malloc((2 * config.r + 1) * config.c * sizeof(char));
    

    Car* cars = (Car*)malloc(config.numCars * sizeof(Car));

    // setting car speed
    for(int i = 0; i<config.numCars; i++){
        cars[i].speed = config.carSpeeds[i];
    }

    
    Frog frog;
    // game.maxTime = 60;
    frog.shape = '@';
    frog.lives = 5;
    frog.score = 0;
    char action;
    time_t startTime = time(NULL);
    //printf("Config.numCars: %d\n", config.numCars);
    //printf("Car array allocated at: %p\n", cars);
repeat:

    srand(time(NULL)); // ? seeding
    initCars(cars, config);

    // initial frog position at the centre of the bottom line
    frog.x = config.c / 2;
    frog.y = 2 * config.r - 1;
    
    

    system("cls"); // clear the screen once at the start


    do {
        clearBuffer(buffer, config);

        time_t currentTime = time(NULL);
        double elapsedTime = difftime(currentTime, startTime);

        // if the time reaches the limit - end the game
        if ((int)elapsedTime == config.maxTime) {
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

        // Draw the player's new position
        buffer[frog.y * config.c + frog.x] = frog.shape;

        //print car

            /*for (int i = 0; i < num_cars; i++) {
                if (cars[i].row == y && cars[i].cx == x) {
                    if (i == 1) printf("1");
                    if (i == 2) printf("2");
                    if (i == 3) printf("3");
                    if (i == 4) printf("4");
                    if (i == 5) printf("5");
                }
            }*/

        updateCars(cars, config);
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


        // Check for user input
        if (_kbhit()) {
            action = _getch();
            if (action == 'x') {
                break;
            }
            // Update the previous position
            /*prev_px = px;
            prev_py = py;*/

            // Move the player based on input
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

        // Sleep(50); // Add a slight delay to control the update speed
    } while (1);

    free(buffer);
    free(cars);

    return 0;
}
