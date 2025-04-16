# SNAKE GAME

Classic snake game made in C using SDL.

## Features

- **BASIC MOVEMENT:** Snake can turn left or right based on arrow key input. If no input is detected, it continues moving forward in the last known direction.
- **WALL AVOIDANCE:** When the snake hits a wall, it will automatically change direction. If possible to the right, otherwise to the left.
- **SNAKE COLLISION:** When the snake moves into it's own tail, the game ends.
- **PROGRESSING DIFFICULTY:** The snake's speed increases over time.
- **TIME TRACKING:** The game includes built-in time and FPS counters.
- **SCORE SYSTEM:** Your score is tracked based on the number of collected dots.
- **BLUE DOT:** A basic collectable that makes snake one segment longer and increases the score.
- **RED DOT:** Special collectable that appears only for a short amount of time. Collecting it increases the score by a big amount and either shortens the snake or slows it down.
- **RESTARTING THE GAME:** You can restart the game at any time.
- **SAVE & LOAD:** The game can be saved to a file and loaded from it at any point.
- **EASY CUSTOMIZATION:** You can easily customize the game by editing constant values at the top of the `main.cpp` file. 

## Controls

- *ARROW KEYS* – Move the snake
- *N* – Start a new game
- *S* – Save the game
- *L* – Load the game
- *Esc* – Exit the game

## Build & Run

To compile the game on Linux, paste the following line into the terminal.

```bash
g++ -O2 -I./sdl/include -L. -o main main.cpp -lm -lSDL2 -lpthread -ldl -lrt
```

Make sure that executable `main` was created successfully, then paste following line.

```bash
./main
```

## License

This project is licensed under the [MIT](./LICENSE)
