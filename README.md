# Frogger Terminal Game

A terminal-based Frogger-inspired game written in C using the **ncurses** library. The player controls a frog attempting to reach the finish line while avoiding moving cars, obstacles, and (at higher levels) a stork predator.

## Features

### Core Gameplay

* Move the frog using **WASD** keys.
* Reach the finish line to score points.
* Avoid collisions with enemy cars.
* Limited number of lives.
* Time limit for the entire game.

### Cars

* Cars move with configurable speeds loaded from a configuration file.
* Random speed changes during gameplay.
* Special "stop cars" that halt when the frog is directly in front of them.
* Cars can temporarily disappear and respawn in different lanes.

### Obstacles

* Randomly generated obstacles block movement.
* Obstacle positions are regenerated when the game restarts.

### Levels

The game contains three levels with increasing difficulty:

#### Level 1

* Standard gameplay.
* Initial number of cars.

#### Level 2

* Additional cars are added dynamically.
* Increased traffic density makes crossing roads more difficult.

#### Level 3

* A stork enemy appears.
* The stork actively follows the frog and can cause collisions.

### Bonus System

* Points are awarded for reaching the finish line.
* Additional bonuses can be granted based on gameplay conditions.

### High Scores

* Scores are saved to a file.
* Top scores can be displayed after the game ends.

### Replay System

* The game records player actions and game states to a file.
* Recorded sessions can be replayed after the game ends.

## Configuration

Game parameters are loaded from a configuration file.

Example:

```txt
numRows=10
numCols=50
numCars=5
maxTime=120
frogLives=5

frogSymbol=@
carSymbol=C
obsSymbol=o
storkSymbol=S

carSpeeds=15,15,15,15,15
```

### Configuration Parameters

| Parameter   | Description                        |
| ----------- | ---------------------------------- |
| numRows     | Number of road rows                |
| numCols     | Width of the game board            |
| numCars     | Initial number of cars             |
| maxTime     | Time limit in seconds              |
| frogLives   | Number of lives                    |
| frogSymbol  | Character representing the frog    |
| carSymbol   | Character representing cars        |
| obsSymbol   | Character representing obstacles   |
| storkSymbol | Character representing the stork   |
| carSpeeds   | Comma-separated list of car speeds |

## Controls

| Key | Action     |
| --- | ---------- |
| W   | Move up    |
| S   | Move down  |
| A   | Move left  |
| D   | Move right |
| Q   | Quit game  |

## Building

### Requirements

* GCC
* ncurses

### Fedora

```bash
sudo dnf install ncurses-devel
```

### Ubuntu/Debian

```bash
sudo apt install libncurses-dev
```

### Compile

```bash
gcc *.c -o frogger -lncurses
```

### Run

```bash
./frogger
```

## Technical Concepts Used

* Dynamic memory allocation (`malloc`, `realloc`, `free`)
* Structures (`struct`)
* File handling (`FILE`, `fopen`, `fread`, `fwrite`)
* Configuration file parsing
* Time-based movement and events
* Random generation
* Ncurses windows and colors
* Collision detection
* Replay recording and playback
* Multi-level game logic

## Author

Michal Binek

## Student Project

This project was developed as part of a university programming course focused on procedural programming in C, dynamic memory management, file operations, and terminal user interfaces using ncurses.
