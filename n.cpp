#define _CRT_SECURE_NO_WARNINGS
#include <conio.h>
#include <stdio.h>
#include <windows.h>
#include <stdlib.h>
#include <time.h>


typedef struct {
    int r;          // Number of rows in the playing area
    int c;          // Number of columns in the playing area
    int numCars;          // Number of cars in the game
    int maxTime;          // Time limit for the game
    int carSpeeds[];     // Array to store speeds for cars
} GameConfig;

GameConfig config;

void loadConfig(GameConfig* config, const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening config file");
        return;
    }


    char line[100];
    while (fgets(line, sizeof(line), file)) {
        // Parse each configuration line
        if (sscanf(line, "numRows=%d", &config->r) == 1) continue;
        if (sscanf(line, "numCols=%d", &config->c) == 1) continue;
        if (sscanf(line, "numCars=%d", &config->numCars) == 1) continue;
        if (sscanf(line, "maxTime=%d", &config->maxTime) == 1) continue;

        // For carSpeeds, parse as a comma-separated lists
        if (strncmp(line, "carSpeeds=", 10) == 0) {
            char* speeds = line + 10; // a pointer to the first character after "carSpeeds=" in line to directly access the comma-separated values
            for (int i = 0; i < config->numCars; i++) {
                config->carSpeeds[i] = atoi(strtok(i == 0 ? speeds : NULL, ","));
            }
        }
    }
    

    fclose(file);
    
}

// Allocate a single block of memory for the 2D buffer

// declared constants for: number of cars, amount of the rows and amount of the columns
// const int num_cars = 5;
// const int r = 12, c = 25;
// declared char 2dim array, as a buffer for storing the map characters
// char buffer[2*r+1][c];

// struct for cars, with parameters: row - cars row, cx - cars x coordinate, speed - cars speed
typedef struct {
    int row;
    int cx;
    int speed;
    time_t lastMove; // Time representation; to capture the point of time of cars last move, to update its position
} Car;

// Declare a pointer for dynamic array of Car structs



typedef struct {
    int x;        // Current x-position of the frog
    int y;        // Current y-position of the frog
    int lives;    // Number of lives the frog has
    int score;    // Player's score
    char shape;
} Frog;

Frog frog;

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



void gotoxy(int x, int y) {
    COORD coordinate;
    coordinate.X = x;
    coordinate.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coordinate);
}

int GetRandom(int max_value, int min_value, int step) {
    // int num_steps = (max_value - min_value) / step + 1;
    int random_value = (rand() % ((++max_value - min_value) / step)) * step + min_value;
    return random_value;
}

// initializing the cars and its parameters
void initCars(Car* cars) {

    
    // an array for storing the rows of cars that have been already used, so they don't repeat
    int* crows = (int*)malloc(config.numCars * sizeof(int));
    // int crows[config.numCars];


    for (int i = 0; i < config.numCars; i++) {
        int flag_in = 0;
        int rrow = 0;

        if (i == 0) {
            int rrow = GetRandom(2 * config.r - 1, 1, 2); // if its the first car, we assign it a row (with a odd y coordinate, because road/lane can only have such)
            cars[i].row = rrow;
            crows[i] = rrow;
            //printf("%d", rrow);

        }
        else {
            while (flag_in == 0) {

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
                    //printf("%d", rrow);
                    flag_in = 1;

                }
                // cars[i].row = rand() % ((r-1) - 1 + 1);
            }
        }

        cars[i].speed = config.carSpeeds[i];
        cars[i].lastMove = time(NULL); // time(NULL) returns current point of time
        cars[i].cx = 1;
        if (i == config.numCars - 1) free(crows);
    }
    
 }

    


// updating cars position based on its last move
void updateCars(Car* cars) {
    time_t currentTime = time(NULL); // time(NULL) returns current point of time

    for (int i = 0; i < config.numCars; i++) {
        if ((currentTime - cars[i].lastMove) >= cars[i].speed) { //? substracting current time and time of the last move and comparing it to cars speed (in seconds), gives us the answer whether we should move the car or no
            // Move car to new position
            cars[i].cx++;
            cars[i].lastMove = currentTime;

            if (cars[i].cx == config.c - 1) {
                cars[i].cx = 1;
            }
        }
    }
}

int frogHit(Car* cars) {
    for (int i = 0; i < config.numCars; i++) {
        if (cars[i].cx == frog.x && cars[i].row == frog.y) return 1;
    }
    return 0;
}

// void drawScreen() {
//     gotoxy(0, 0);
//     for (int y = 0; y < 2*r+1; y++) {
//         for (int x = 0; x < c; x++) {
//             if (buffer[y][x] == 'C') red();
//             else if (buffer[y][x] == '@') green();
//             else white();

//             printf("%c", buffer[y][x]);
//         }
//         printf("\n");
//     }
// }

void drawScreen(char* buffer) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CHAR_INFO* charBuffer = (CHAR_INFO*)malloc((2 * config.r + 1) * config.c * sizeof(CHAR_INFO));
    COORD bufferSize = { config.c, 2 * config.r + 1 };
    COORD bufferCoord = { 0, 0 };
    SMALL_RECT writeRegion = { 0, 0, config.c - 1, 2 * config.r };

    // Populate charBuffer with data from buffer
    for (int y = 0; y < 2 * config.r + 1; y++) {
        for (int x = 0; x < config.c; x++) {
            charBuffer[y * config.c + x].Char.AsciiChar = buffer[y * config.c + x];
            if (buffer[y * config.c + x] == '@') charBuffer[y * config.c + x].Attributes = FOREGROUND_GREEN;
            else if (buffer[y * config.c + x] == 'C') charBuffer[y * config.c + x].Attributes = FOREGROUND_RED;
            else charBuffer[y * config.c + x].Attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // White text

        }
    }
    // Write the charBuffer to the console
    WriteConsoleOutput(hConsole, (CHAR_INFO*)charBuffer, bufferSize, bufferCoord, &writeRegion);

}

void drawStatus(double elapsedTime) {
    gotoxy(0, 2 * config.r + 2); // Move cursor to the line below the game area
    printf("Michal Binek 203726; Timer: %d s/ %d s; Score: %d; Lives: %d", (int)elapsedTime, config.maxTime, frog.score, frog.lives);
}

void clearBuffer(char* buffer) {
    for (int y = 0; y < 2 * config.r + 1; y++) {
        for (int x = 0; x < config.c; x++) {
            buffer[y * config.c + x] = ' ';
        }
    }
}
void drawCars(char* buffer, Car* cars) {
    for (int i = 0; i < config.numCars; i++) {
        buffer[cars[i].row * config.c + cars[i].cx] = 'C'; // 'C' represents the car
    }
}


int main() {
    GameConfig* cptr = &config;
    loadConfig(cptr, "config.txt");
    
    char* buffer = (char*)malloc((2 * config.r + 1) * config.c * sizeof(char));
    

    Car* cars = (Car*)malloc(config.numCars * sizeof(Car));

    
    
    
    // game.maxTime = 60;
    frog.shape = '@';
    frog.lives = 5;
    char action;
    time_t startTime = time(NULL);
    //printf("Config.numCars: %d\n", config.numCars);
    //printf("Car array allocated at: %p\n", cars);
repeat:

    srand(time(NULL)); // ? seeding
    initCars(cars);

    //int px = c/2, py = 2*r-1;
    frog.x = config.c / 2;
    frog.y = 2 * config.r - 1;
    frog.score = 0;
    

    system("cls"); // Clear the screen once at the start


    do {
        clearBuffer(buffer);

        time_t currentTime = time(NULL);
        double elapsedTime = difftime(currentTime, startTime);

        // if the time reaches the limit - end the game
        if ((int)elapsedTime == config.maxTime) {
            gotoxy(0, 2 * config.r + 3);
            printf("Out of time! Your score is: %d", frog.score);
            Sleep(2000);
            return 0;
        }

        // Convert to milliseconds
        //int milliseconds = (int)((elapsedTime - (int)elapsedTime) * 1000); // extracting the fraction part of elapsedTime, multipli by 1000 and (int) to get milliseconds
        // Draw the border
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

        updateCars(cars);
        drawCars(buffer, cars);

        drawScreen(buffer);
        drawStatus(elapsedTime);
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
        if (frogHit(cars) == 1) {
            if (--frog.lives == 0) {
                gotoxy(0, 2 * config.r + 3);
                printf("You lost. Your score was: %d", frog.score);
                Sleep(2000);
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
