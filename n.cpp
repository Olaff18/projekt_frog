#define _CRT_SECURE_NO_WARNINGS
#include <conio.h>
#include <stdio.h>
#include <windows.h>
#include <stdlib.h>
#include <time.h>


// declared constants for: number of cars, amount of the rows and amount of the columns
const int num_cars = 5;
const int r = 15, c = 40;
// declared char 2dim array, as a buffer for storing the map characters
char buffer[2*r+1][c];

// struct for cars, with parameters: row - cars row, cx - cars x coordinate, speed - cars speed
typedef struct {
    int row;
    int cx;
    int speed;
    time_t lastMove; // Time representation; to capture the point of time of cars last move, to update its position
} Car;

Car cars[num_cars]; // Array of structs for cars

typedef struct {
    int x;        // Current x-position of the frog
    int y;        // Current y-position of the frog
    int lives;    // Number of lives the frog has
    int score;    // Player's score
} Frog;

Frog frog;

void green() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);
}

void red() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
}

void white() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

typedef struct {
    int numRows;          // Number of rows in the playing area
    int numCols;          // Number of columns in the playing area
    int numCars;          // Number of cars in the game
    int maxTime;          // Time limit for the game
    int carSpeeds[5];     // Array to store speeds for cars
} GameConfig;

GameConfig game;


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
void initCars() {
    for (int i = 0; i < num_cars; i++) {

        int crows[num_cars];


        for (int i = 0; i < num_cars; i++) {
            int flag_in = 0;
            int rrow = 0;
            if (i == 0) {
                int rrow = GetRandom(2 * r - 1, 1, 2); // if its the first car, we assign it a row (with a odd y coordinate, because road/lane can only have such)
                cars[i].row = rrow;
                crows[i] = rrow;
            }
            else {
                while (flag_in == 0) {
                    rrow = GetRandom(2 * r - 1, 1, 2);
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
                    // cars[i].row = rand() % ((r-1) - 1 + 1);
                }
            }

            
            cars[i].speed = 1 + i; // the frequency of cars movement - if speed = 2, car moves every 2 seconds
            cars[i].lastMove = time(NULL); // time(NULL) returns current point of time
            cars[i].cx = 1;
        }
    }
}

// updating cars position based on its last move
void updateCars() {
    time_t currentTime = time(NULL); // time(NULL) returns current point of time

    for (int i = 0; i < num_cars; i++) {
        if ((currentTime - cars[i].lastMove) >= cars[i].speed) { //? substracting current time and time of the last move and comparing it to cars speed (in seconds), gives us the answer whether we should move the car or no
            // Move car to new position
            cars[i].cx++;
            cars[i].lastMove = currentTime;

            if (cars[i].cx == c-1) {
                cars[i].cx = 1;
            }
        }
    }
}


void drawScreen() {
    gotoxy(0, 0);
    for (int y = 0; y < 2*r+1; y++) {
        for (int x = 0; x < c; x++) {
            if (buffer[y][x] == 'C') red();
            else if (buffer[y][x] == '@') green();
            else white();
            
            printf("%c", buffer[y][x]);
        }
        printf("\n");
    }
}

void clearBuffer() {
    for (int y = 0; y < 2*r+1; y++) {
        for (int x = 0; x < c; x++) {
            buffer[y][x] = ' ';
        }
    }
}

void drawCars() {
    for (int i = 0; i < num_cars; i++) {
        buffer[cars[i].row][cars[i].cx] = 'C'; // 'C' represents the car
    }
}


int main() {
    srand(time(NULL)); // ? seeding
    initCars();
    //int px = c/2, py = 2*r-1;
    frog.x = c / 2;
    frog.y = 2 * r - 1;

    char action;
    
    
   

    system("cls"); // Clear the screen once at the start

    time_t startTime = time(NULL);
    do {
        clearBuffer();

        time_t currentTime = time(NULL);
        double elapsedTime = difftime(currentTime, startTime);

        // Convert to milliseconds
        //int milliseconds = (int)((elapsedTime - (int)elapsedTime) * 1000); // extracting the fraction part of elapsedTime, multipli by 1000 and (int) to get milliseconds

        // Draw the border
        for (int x = 0; x < c; x++) { // rows
            buffer[0][x] = '#';
            buffer[2*r][x] = '#';
        }
        for (int y = 0; y < 2*r+1; y++) { // columns
            buffer[y][0] = '#';
            buffer[y][c-1] = '#';
        }
        // draw lanes
        for (int y = 2; y < (2 * r); y += 2) {
            for (int x = 1; x < c - 1; x++) {
                buffer[y][x] = '-';
            }
        }

        // Draw the player's new position
        buffer[frog.y][frog.x] = '@';

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
        
        updateCars();
        drawCars();

        drawScreen();
        printf("Michal Binek 203726, timer: %d", (int)elapsedTime," s");
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
            if ((action == 's' || action == 'S') && frog.y + 1 != 2*r) frog.y++;
            if ((action == 'a' || action == 'A') && frog.x - 1 != 0) frog.x--;
            if ((action == 'd' || action == 'D') && frog.x + 1 != c - 1) frog.x++;
        }

        // Sleep(50); // Add a slight delay to control the update speed
    } while (1);

    return 0;
}
