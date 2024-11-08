#define _CRT_SECURE_NO_WARNINGS
#include <conio.h>
#include <stdio.h>
#include <windows.h>
#include <stdlib.h>
#include <time.h>

// declaring amount of rows (roads) and columns
const int r = 9;
const int c = 20;
// declaring a 2d buffer - 2*r+1 is a total of rows considering rows and borders
char buffer[2*r+1][c];

// declared constants for: number of cars, amount of the rows and amount of the columns
const int num_cars = 5;


// struct for cars, with parameters: row - cars row, cx - cars x coordinate, speed - cars speed
typedef struct {
    int row;
    int cx;
    int speed;
    time_t lastMove; // Time representation; to capture the point of time of cars last move, to update its position
} Car;

Car cars[num_cars];


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


// function for moving the cursor to a (x,y) point
void gotoxy(int x, int y) {
    COORD coordinate;
    coordinate.X = x;
    coordinate.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coordinate);
}


// function for generating a random road for a car
int GetRandom(int max_value, int min_value, int step) {
    // int num_steps = (max_value - min_value) / step + 1;
    int random_value = (rand() % ((++max_value - min_value) / step)) * step + min_value;
    return random_value;
}

// initializing the cars and its parameters
void initCars() {
    // array for storing the rows where there is a car already, so that 2 cars dont appear on the same road
    int crows[num_cars];

    // for loop executing num_cars times to arrenge an odd row for each car
    for (int i = 0; i < num_cars; i++) {
        // flag to check if some row was already assigned to some car: 0 - it was; 1 - it wasn't
        int flag_in = 0;
        // int rrow - to store a current row
        int rrow = 0;
        if (i == 0) {
            int rrow = GetRandom(2 * r - 1, 1, 2); // if its the first car, we assign it a row (with a odd y coordinate, because road/lane can only have such)
            cars[i].row = rrow;
            crows[i] = rrow;
        }
        else {
            // a loop that runs while a unique row wasn't generated for a particular car
            while (flag_in == 0) {
                rrow = GetRandom(2 * r - 1, 1, 2); 
                int count = 0; // if count after iterations is equal to the current amount of assigned rows, then we know that the row is unique
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

        
        cars[i].speed = 1 + i; // the frequency of cars movement - if speed = 2, car moves every 2 seconds
        cars[i].lastMove = time(NULL); // time(NULL) returns current point of time
        cars[i].cx = 1;
    }
}


// updating cars position based on its last move
void updateCars() {
    time_t currentTime = time(NULL); // time(NULL) returns current point of time

    for (int i = 0; i < num_cars; i++) {
        if ((currentTime - cars[i].lastMove) >= cars[i].speed) { // substracting current time and time of the last move and comparing it to cars speed (in seconds), gives us the answer whether we should move the car or no
            // Move car to new position
            cars[i].cx++;
            cars[i].lastMove = currentTime;
            // if car is next to the border, wrap it
            if (cars[i].cx == c-1) {
                cars[i].cx = 1;
            }
        }
    }
}

// function for checking if the frog was hit by a car
int frogHit(){
    for(int i = 0; i < num_cars; i++){
        if(cars[i].cx == frog.x && cars[i].row == frog.y) return 1;
    }
    return 0;
}


void drawScreen() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE); // console handle
    CHAR_INFO charBuffer[2*r+1][c]; // a pointer to a buffer containing character and attribute data to be written to the console
    COORD bufferSize = {c, 2 * r + 1}; // a COORD structure that specifies the size of the charBuffer array
    COORD bufferCoord = {0, 0}; // a COORD structure specifying the coordinates of the top-left cell in the buffer from which to start copying data
    SMALL_RECT writeRegion = {0, 0, c - 1, 2 * r}; // a pointer to a SMALL_RECT structure that specifies the region of the console screen buffer to which the data will be written

    // fill charBuffer with data from buffer
    for (int y = 0; y < 2*r + 1; y++) {
        for (int x = 0; x < c; x++) {
            charBuffer[y][x].Char.AsciiChar = buffer[y][x];
            if(buffer[y][x] == frog.shape) charBuffer[y][x].Attributes = FOREGROUND_GREEN; // if (y,x) = frog symbol, set the color green
            else if(buffer[y][x] == 'C') charBuffer[y][x].Attributes = FOREGROUND_RED; // if (y,x) = car symbol, set the color to red
            else charBuffer[y][x].Attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // white text
                      
        }
    }       
    // write the charBuffer to the console
    WriteConsoleOutput(hConsole, (CHAR_INFO*)charBuffer, bufferSize, bufferCoord, &writeRegion);
    
}

// draw the status of the game
void drawStatus(double elapsedTime, int maxTime) {
    gotoxy(0, 2 * r + 2); // move cursor to the line below the game area
    printf("Michal Binek 203726; Timer: %d s/ %d s; Score: %d; Lives: %d", (int)elapsedTime, maxTime, frog.score, frog.lives);
}

// clearing the entire buffer with empty spaces
void clearBuffer() {
    for (int y = 0; y < 2*r+1; y++) {
        for (int x = 0; x < c; x++) {
            buffer[y][x] = ' ';
        }
    }
}

// putting the car symbol into the buffer
void drawCars() {
    for (int i = 0; i < num_cars; i++) {
        buffer[cars[i].row][cars[i].cx] = 'C'; // 'C' represents the car
    }
}


int main() {
    int maxTime = 60;
    frog.shape = '@';
    frog.lives = 5;
    char action; // for storing the pressed key
    time_t startTime = time(NULL); // start time of the game
    repeat: // goto repeat, to restart the game when a player got to finish line or got hit by a car
    srand(time(NULL)); // so the random values generate all the time
    initCars(); // initializing the cars

    // players default position, at the centre of the bottom row
    frog.x = c / 2;
    frog.y = 2 * r - 1;
    
    
    
   

    system("cls"); // clear the screen once at the start

    
    do {
        clearBuffer();
        
        time_t currentTime = time(NULL);
        double elapsedTime = difftime(currentTime, startTime); // calculating the difference between the start time and the current time

        // if the time reaches the limit - end the game
        if((int)elapsedTime == maxTime){
            gotoxy(0, 2 * r + 3); // go below the status and print the message
            printf("Out of time! Your score is: %d", frog.score);
            Sleep(2000); // show for 2 seconds
            return 0;
        }

        // draw the border
        for (int x = 0; x < c; x++) { // rows
            buffer[0][x] = '#';
            buffer[2*r][x] = '#';
        }
        for (int y = 0; y < 2*r+1; y++) { // columns
            buffer[y][0] = '#';
            buffer[y][c-1] = '#';
        }
        // draw lane borders
        for (int y = 2; y < (2 * r); y += 2) {
            for (int x = 1; x < c - 1; x++) {
                buffer[y][x] = '-';
            }
        }

        // draw the player's new position
        buffer[frog.y][frog.x] = frog.shape;

        updateCars();
        drawCars();
        drawScreen();
        drawStatus(elapsedTime, maxTime);
        // when frog gets to the finish line
        if(frog.y - 1 == 0){
            frog.score += 1; // update the score
            gotoxy(0, 2 * r + 3); // go below the status to print the message
            printf("You won! Your score is: %d", frog.score);
            Sleep(2000);
            startTime+=2; // increase startTime by 2 seconds, so the timer is still acurate after Sleep()
            goto repeat; // go to the "repeat" line of code, to start the game over
        
        }

        
        // check for user input
        if (_kbhit()) {
            action = _getch();
            if (action == 'x') {
                break;
            }

            // move the player based on input
            if ((action == 'w' || action == 'W') && frog.y - 1 != 0) frog.y--;
            if ((action == 's' || action == 'S') && frog.y + 1 != 2*r) frog.y++;
            if ((action == 'a' || action == 'A') && frog.x - 1 != 0) frog.x--;
            if ((action == 'd' || action == 'D') && frog.x + 1 != c - 1) frog.x++;
        }
        // check if the frog hit the car, if yes - restart the game and -1 life
        if(frogHit() == 1){
            if(--frog.lives == 0){ // if lives end
                gotoxy(0, 2 * r + 3); // set cursor below the status and print a message there
                printf("You lost. Your score was: %d", frog.score);
                Sleep(2000); // show it for 2s
                return 0; // end the game
            }
            gotoxy(0, 2 * r + 3); // if there are still lives remaining, print the message and start the game over
            printf("Frog got ran over:( Try again. Lives remaining: %d", frog.lives);
            Sleep(2000);
            startTime+=2;
            goto repeat;
        }

    } while (1);

    return 0;
}
