#define _USE_MATH_DEFINES
#include<math.h>
#include<stdio.h>
#include<string.h>
#include<time.h>

extern "C" {
#include"./SDL2-2.0.10/include/SDL.h"
#include"./SDL2-2.0.10/include/SDL_main.h"
}

// Length of one side of each square segment of the snake in pixels.
#define SEGMENT_SIZE    30

// Number of columns and rows in the playable area.
#define COLS    17
#define ROWS    20

// Size of the playable area: COLS x ROWS.
// Each cell is the same size in pixels as one segment.
#define SCREEN_WIDTH    (COLS * SEGMENT_SIZE)
#define SCREEN_HEIGHT    (ROWS * SEGMENT_SIZE)

// Both lengths are in pixels.
// Additional pixel is added for the outline around the playable area.
#define BORDER_LENGTH    (8 + 1)
#define INFO_HEIGHT    (44 + 1)

// Info section is where all of the information is displayed.
#define WINDOW_WIDTH    (SCREEN_WIDTH  + 2 * BORDER_LENGTH)
#define WINDOW_HEIGHT    (SCREEN_HEIGHT  + BORDER_LENGTH + INFO_HEIGHT)

// IMPORTANT:
// Changes to the starting position and length must be made carefully.
// If even one segment spawns outside the playable area, the game will crash.

// Starting position of the snake's head.
#define STARTING_X    12
#define STARTING_Y    10

// Length includes the head.
// Remaining segments are placed in a straight line below the head.
#define STARTING_LENGTH    10

// Time settings are all in s.

// Time between snake's moves.
// The lower the value the faster the snake will move.
#define STARTING_SPEED    0.2

// Time after which the snake's speed will be multiplied by SPEEDUP_RATE.
#define SPEEDUP_TIME    10

// Constant by which speed will be multiplied to make snake move faster.
#define SPEEDUP_RATE    0.95

// Constant by which speed will be multiplied to make snake move slower.
#define SLOWDOWN_RATE    1.2

// Width of the bar showing time left before red dot disapears in px.
#define RED_BAR_SIZE    100

// Time for which red dot is active.
#define RED_DOT_TIMER    10

// Min and max time between one red dot disappeared and new one appearing.
#define RED_MIN_APP    5
#define RED_MAX_APP    15

// Number of points given for collecting a red and blue dot.
#define RED_SCORE    250
#define BLUE_SCORE    100

// Generating a random number between min and max (inclusive).
#define RA(min, max)    ( (min) + rand() % ((max) - (min) + 1) )

// Handles all SDL resources needed for rendering.
typedef struct {
	SDL_Surface *screen, *charset;
	SDL_Texture *scrtex;
	SDL_Window *window;
	SDL_Renderer *renderer;
} DISPLAY;

// Color IDs used in drawing game elements.
typedef struct {
	int black;
	int green; 
	int red; 
	int blue;
	int white;
} COLOR;

// Tracks frame timing, FPS, and total world time.
typedef struct {
	int t1;
	int t2;
	int frames;
	double delta;
	double worldTime;
	double fpsTimer;
	double fps;
} TIME;

// Information about a segment of the snake as a linked list element.
typedef struct segment{
	int x;
	int y;
	struct segment* next;
} SEGMENT;

// Tracks global information about the snake and points to the linked list of segments.
typedef struct {
	int len;
	double nextMove;
	double speed;
	double speedupTime;
	char lastMove;
	SEGMENT* head;
} SNAKE;

// Information about the basic collectable.
typedef struct {
	int x;
	int y;
} BLUE_DOT;

// Information about the special collectable.
typedef struct {
	int x;
	int y;
	double activeTimer;
	double appearTimer;
} RED_DOT;

////////////////////////////
/*INITIALISATION FUNCTIONS*/
////////////////////////////	

	// Initializes SDL and seeds rand.
	int Start()
	{
		int rc;

		if(SDL_Init(SDL_INIT_EVERYTHING) != 0) {
			printf("SDL_Init error: %s.\n", SDL_GetError());
			return 0;
		}

		srand(time(NULL));

		return 1;
	}

	// Sets up window, renderer, textures, and screen surface.
	int initDisplay(DISPLAY* display)
	{
		int rc;

		rc = SDL_CreateWindowAndRenderer(WINDOW_WIDTH, WINDOW_HEIGHT, 0, &display->window, &display->renderer);

		if(rc != 0) {
			printf("SDL_CreateWindowAndRenderer error: %s.\n", SDL_GetError());
			return 0;
		}

		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
		SDL_RenderSetLogicalSize(display->renderer, WINDOW_WIDTH, WINDOW_HEIGHT);
		SDL_SetRenderDrawColor(display->renderer, 0, 0, 0, 255);

		SDL_SetWindowTitle(display->window, "Snake - Michał Kaźmierowski");

		display->screen = SDL_CreateRGBSurface(0, WINDOW_WIDTH, WINDOW_HEIGHT, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);

		display->scrtex = SDL_CreateTexture(display->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, WINDOW_WIDTH, WINDOW_HEIGHT);

		SDL_ShowCursor(SDL_DISABLE);

		display->charset = SDL_LoadBMP("./cs8x8.bmp");
		if(display->charset == NULL) {
			printf("SDL_LoadBMP(cs8x8.bmp) error: %s.\n", SDL_GetError());
			return 0;
		}

		return 1;
	}

	// Maps color values to names.
	void initColor(DISPLAY* display, COLOR* color)
	{
		SDL_SetColorKey(display->charset, true, 0x000000);

		color->black = SDL_MapRGB(display->screen->format, 0x00, 0x00, 0x00);
		color->green = SDL_MapRGB(display->screen->format, 0x00, 0xFF, 0x00);
		color->red = SDL_MapRGB(display->screen->format, 0xFF, 0x00, 0x00);
		color->blue = SDL_MapRGB(display->screen->format, 0x11, 0x11, 0xCC);
		color->white = SDL_MapRGB(display->screen->format, 0xFF, 0xFF, 0xFF);
	}

	// Sets starting values for time tracking.
	void initTime(TIME* time)
	{
		time->t1 = SDL_GetTicks();
		time->frames = 0;
		time->fpsTimer = 0;
		time->fps = 0;
		time->worldTime = 0;
	}

	// Sets snake properties and generates initial body segments.
	void initSnake(SNAKE* snake)
	{
		snake->len = STARTING_LENGTH;
		snake->head = (SEGMENT*)malloc(sizeof(SEGMENT));
		snake->speed = STARTING_SPEED;
		snake->speedupTime = SPEEDUP_TIME;
		snake->nextMove = STARTING_SPEED;
		snake->lastMove = 'w';

		SEGMENT* current = snake->head;

		for(int i = 0; i < STARTING_LENGTH; i++) {
			current->x = STARTING_X;
			current->y = STARTING_Y + i;

			if(i < STARTING_LENGTH - 1) {
				current->next = (SEGMENT*)malloc(sizeof(SEGMENT));
				current = current->next;
			}
			else {
				current->next = NULL;
			}
		}
	}

	// sets random time for red dot apperance.
	void initRedDot (RED_DOT* redDot)
	{
		redDot->appearTimer = RA(RED_MIN_APP, RED_MAX_APP);
		redDot->activeTimer = 0;
	}

////////////////////////////

/////////////////////
/*DRAWING FUNCTIONS*/
/////////////////////
	
	// Draws a string using bitmap as font.
    void DrawString(SDL_Surface *screen, int x, int y, const char *text, SDL_Surface *charset)
	{
		int px, py, c;
		SDL_Rect s, d;
		s.w = 8;
		s.h = 8;
		d.w = 8;
		d.h = 8;
		while(*text) {
			c = *text & 255;
			px = (c % 16) * 8;
			py = (c / 16) * 8;
			s.x = px;
			s.y = py;
			d.x = x;
			d.y = y;
			SDL_BlitSurface(charset, &s, screen, &d);
			x += 8;
			text++;
		}
	}

    //Draws a pixel in a specific color.
	void DrawPixel(SDL_Surface *surface, int x, int y, Uint32 color) 
	{
		int bpp = surface->format->BytesPerPixel;
		Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;
		*(Uint32 *)p = color;
	}

	// Draws a line of length l in direction (dx, dy).
	void DrawLine(SDL_Surface *screen, int x, int y, int l, int dx, int dy, Uint32 color) 
	{
		for(int i = 0; i < l; i++) {
			DrawPixel(screen, x, y, color);
			x += dx;
			y += dy;
		}
	}

	// Draws a filled rectangle with a colored outline.
	void DrawRectangle(SDL_Surface *screen, int x, int y, int l, int k, Uint32 outlineColor, Uint32 fillColor) 
	{
		int i;
		DrawLine(screen, x, y, k, 0, 1, outlineColor);
		DrawLine(screen, x + l - 1, y, k, 0, 1, outlineColor);
		DrawLine(screen, x, y, l, 1, 0, outlineColor);
		DrawLine(screen, x, y + k - 1, l, 1, 0, outlineColor);
		for(i = y + 1; i < y + k - 1; i++)
			DrawLine(screen, x + 1, i, l - 2, 1, 0, fillColor);
	}

/////////////////////

////////////////////////
/*GAME LOGIC FUNCTIONS*/
////////////////////////

	// Checks if the cell at coordinates (x, y) is occupied by any of the snake's segments.
    // Ignores the last segment because it will be deleted when the move will happen.
	int checkCollision(SNAKE* snake, int x, int y)
	{
		SEGMENT* current = snake->head;

		for(int i = 0; i < snake->len - 1; current = current->next, i++)
		{
			if((x == current->x) && (y == current->y))
				return 1;
		}
		
		return 0;
	}

	// Generates the position of a new blue dot on an unoccupied cell.
	void generateBlueDot(BLUE_DOT* blueDot,RED_DOT* redDot, SNAKE* snake)
	{
		SEGMENT* current = snake->head;
		int free[COLS][ROWS];

		for(int i = 0; i < ROWS; i++)
			for(int j = 0; j < COLS; j++)
				free[j][i] = 1;

		for(int i = 0; i < snake->len; current = current->next, i++) {
			free[current->x][current->y] = 0;
		}

		if(redDot->activeTimer > 0)
		{
			free[redDot->x][redDot->y] = 0;
		}

		int found = 0;
		while(!found) {
			int x = RA(0, COLS - 1);
			int y = RA(0, ROWS - 1);
			if(free[x][y]) {
				blueDot->x = x;
				blueDot->y = y;
				found = 1;
			}
		}
	}

	// Generates the position of a new red dot on an unoccupied cell.
	void generateRedDot(RED_DOT* redDot, BLUE_DOT* blueDot, SNAKE* snake)
	{
		SEGMENT* current = snake->head;
		int free[COLS][ROWS];

		for(int i = 0; i < ROWS; i++)
			for(int j = 0; j < COLS; j++)
				free[j][i] = 1;

		for(int i = 0; i < snake->len; current = current->next, i++) {
			free[current->x][current->y] = 0;
		}

		free[blueDot->x][blueDot->y] = 0;
	

		int found = 0;
		while(!found) {
			int x = RA(0, COLS - 1);
			int y = RA(0, ROWS - 1);
			if(free[x][y]) {
				redDot->x = x;
				redDot->y = y;
				found = 1;
			}
		}

		redDot->activeTimer = RED_DOT_TIMER;
	}

	// Moves the snake in the given direction.
    // Handles collisions, scoring, growth, red dot interaction, and direction correction to prevent reversing.
	int moveSnake(SNAKE* snake, BLUE_DOT* blueDot, RED_DOT* redDot, char move, int* score)
	{
		int newX, newY;
		char lastMove = snake->lastMove;
		SEGMENT* current = snake->head;

        // Prevents moving backwards.
		if(((move == 'w') && (lastMove == 's'))  ||  ((move == 's') && (lastMove == 'w'))  ||  ((move == 'a') && (lastMove == 'd'))  ||  ((move == 'd') && (lastMove == 'a'))) {
			move = lastMove;
		}
		
		// Moves the snake in the given direction.
        // Handles bumping from the border.
        // If it is possible moves snake to the right of the head; otherwise to the left.
        switch (move){
			case 'w':
				if(current->y == 0) {
					if((current->x == COLS - 1) || (checkCollision(snake, current->x + 1, current->y) && (current->x != 0))) {
						newX = current->x - 1;
						newY = current->y;
						move = 'a';
					}
					else {
						newX = current->x + 1;
						newY = current->y;
						move = 'd';
					}
				}
				else {
					newX = current->x;
					newY = current->y - 1;
				}
				
				break;
			case 's':
				if(current->y == ROWS - 1) {
					if((current->x == 0) || (checkCollision(snake, current->x - 1, current->y) && (current->x != COLS - 1))) {
						newX = current->x + 1;
						newY = current->y;
						move = 'd';
					}
					else {
						newX = current->x - 1;
						newY = current->y;
						move = 'a';
					}
					
				}
				else {
					newX = current->x;
					newY = current->y + 1;
				}

				break;
			case 'a':
				if(current->x == 0) {
					if((current->y == 0) || (checkCollision(snake, current->x, current->y - 1) && (current->y != ROWS - 1))) {
						newX = current->x;
						newY = current->y + 1;
						move = 's';
					}
					else {
						newX = current->x;
						newY = current->y - 1;
						move = 'w';
					}
					
				}
				else {
					newX = current->x - 1;
					newY = current->y;
				}

				break;
			case 'd':
				if(current->x == COLS -1) {
					if((current->y == ROWS - 1) || (checkCollision(snake, current->x, current->y + 1) && (current->y != 0))) {
						newX = current->x;
						newY = current->y - 1;
						move = 'w';
					}
					else {
						newX = current->x;
						newY = current->y + 1;
						move = 's';
					}
					
				}
				else {
					newX = current->x + 1;
					newY = current->y;
				}
				
				break;
		}

		int newBlueDot = 0;
		int newRedDot = 0;

        // Checks if snake moved on a blue dot.
        // If it did makes snake longer.
        // A new segment is added at the end.
        // It doesn't need to have a specific position, because it will be deleted before it could be rendered.
		if((newX == blueDot->x) && (newY == blueDot->y)) {
			for(int j = 0; j < snake->len - 1; j++) {
				current = current->next;
			}
			current->next = (SEGMENT*)malloc(sizeof(SEGMENT));
			snake->len++;
			newBlueDot = 1;

			*score += BLUE_SCORE;
		}

        // Checks if snake moved on a red dot.
        // If it did randomizes an effect (makes snake shorter or slower).
		if((redDot->activeTimer > 0) && (newX == redDot->x) && (newY == redDot->y)) {
			if((snake->len == 1) || (RA(0, 1))) {
				snake->speed *= SLOWDOWN_RATE;
			}
			else {
				current = snake->head;
				for(int j = 0; j < snake->len - 2; j++) {
					current = current->next;
				}
				free(current->next);
				current->next = NULL;
				snake->len--;
			}

			newRedDot = 1;

			*score += RED_SCORE;
		}
		

        // If snake can move properly, adds new head at position (newX, newY) and deletes last segment.
		if(checkCollision(snake, newX, newY)) {
			return 0;
		}
		else {
			current = snake->head;
			SEGMENT* temp = (SEGMENT*)malloc(sizeof(SEGMENT));
			temp->x = newX;
			temp->y = newY;
			temp->next = current;

			snake->head = temp;

			for(int j = 0; j < snake->len - 2; j++) {
				current = current->next;
			}
			free(current->next);
			current->next = NULL;

			snake->lastMove = move;
		}

		if(newBlueDot) generateBlueDot(blueDot, redDot, snake);

		if(newRedDot) initRedDot(redDot);



		return 1;
	}

////////////////////////

///////////////////
/*PRINT FUNCTIONS*/
///////////////////

	// Prints statistics, control info, borders around the playable area and the background of the area.
    void printBorder(DISPLAY* display, int score, COLOR* color, TIME* time)
	{
		char text[128];
		
		SDL_FillRect(display->screen, NULL, color->blue);

		// +/- 1 is used here because of the 1 px outline around the screen, which is included in the overall screen size.
		// It is useful in the rest of the program to have it saved in border/info sizes, but here it must be compensated for.
		DrawRectangle(display->screen, BORDER_LENGTH - 1, INFO_HEIGHT - 1, SCREEN_WIDTH + 1, SCREEN_HEIGHT + 1, color->white, color->black);

		sprintf(text, "Time: %.1lf    FPS: %.0lf", time->worldTime, time->fps);
		DrawString(display->screen, BORDER_LENGTH + 2, 10, text, display->charset);

		sprintf(text, "'Esc' - Exit    'n' - New game");
		DrawString(display->screen, display->screen->w - (strlen(text) * 8) - (BORDER_LENGTH + 2), 10, text, display->charset);

		sprintf(text, "Score: %d", score);
		DrawString(display->screen, BORDER_LENGTH + 2, 26, text, display->charset);

		sprintf(text, "Implemented: 1, 2, 3, 4, A, B, C, D, E");
		DrawString(display->screen, display->screen->w - (strlen(text) * 8) - (BORDER_LENGTH + 2), 26, text, display->charset);

	}

    // Prints whole snake segment by segment in correct cells.
	void printSnake(DISPLAY* display, SNAKE* snake, COLOR* color)
	{
		int len = snake->len;
		SEGMENT* current = snake->head;
		int x, y;

		for(int i = 0; i < len; i++)
		{
			if(i != 0)	current = current->next;

			x = BORDER_LENGTH + current->x * SEGMENT_SIZE;
			y = INFO_HEIGHT + current->y * SEGMENT_SIZE;
			DrawRectangle(display->screen, x, y, SEGMENT_SIZE, SEGMENT_SIZE, color->green, color->green);
		}

	}

    // Prints blue dot in correct cell as a smaller centered square.
	void printBlueDot(DISPLAY* display, BLUE_DOT* blueDot, COLOR* color)
	{
		int x = BORDER_LENGTH + blueDot->x * SEGMENT_SIZE + 0.25 * SEGMENT_SIZE;
		int y = INFO_HEIGHT + blueDot->y * SEGMENT_SIZE + 0.25 * SEGMENT_SIZE;

		DrawRectangle(display->screen, x, y, SEGMENT_SIZE / 2, SEGMENT_SIZE / 2, color->blue, color->blue);
	}

    // Prints red dot and the time bar showing how much longer it will stay.
	void printRedDot(DISPLAY* display, RED_DOT* redDot, COLOR* color)
	{
		int x = BORDER_LENGTH + redDot->x * SEGMENT_SIZE + 0.25 * SEGMENT_SIZE;
		int y = INFO_HEIGHT + redDot->y * SEGMENT_SIZE + 0.25 * SEGMENT_SIZE;

		DrawRectangle(display->screen, x, y, SEGMENT_SIZE / 2, SEGMENT_SIZE / 2, color->red, color->red);

		int greenWidth = RED_BAR_SIZE * (redDot->activeTimer / RED_DOT_TIMER);
		
		int redWidth = RED_BAR_SIZE - greenWidth;

		if(greenWidth > 0) DrawRectangle(display->screen, display->screen->w / 2 - RED_BAR_SIZE / 2, 10, greenWidth, 8, color->green, color->green);

		if(redWidth > 0) DrawRectangle(display->screen, display->screen->w / 2 - RED_BAR_SIZE / 2 + greenWidth, 10, redWidth, 8, color->red, color->red);
	}

	// Prints final statistics.
    void printEnd(DISPLAY* display, int score, COLOR* color)
	{
		char text[128];

		sprintf(text, "Your score is: %d", score);
		DrawString(display->screen, display->screen->w / 2 - strlen(text) * 8 / 2, INFO_HEIGHT + SCREEN_HEIGHT / 2 - 4 - 16, text, display->charset);

		sprintf(text, "Press 'Esc' to exit");
		DrawString(display->screen, display->screen->w / 2 - strlen(text) * 8 / 2, INFO_HEIGHT + SCREEN_HEIGHT / 2 - 4, text, display->charset);

		sprintf(text, "Press 'n' to start a new game");
		DrawString(display->screen, display->screen->w / 2 - strlen(text) * 8 / 2, INFO_HEIGHT + SCREEN_HEIGHT / 2 - 4 + 16, text, display->charset);

	}

    // Refreshes the whole screen.
	void updateScreen(DISPLAY* display)
	{
		SDL_UpdateTexture(display->scrtex, NULL, display->screen->pixels, display->screen->pitch);

		SDL_RenderCopy(display->renderer, display->scrtex, NULL, NULL);
		SDL_RenderPresent(display->renderer);
	}

///////////////////

/////////////////
/*END FUNCTIONS*/
/////////////////

	// Frees memory allocated for the linked list containing all snake segments.
    void freeSnake(SNAKE* snake)
	{
		for(int i = 0; i < snake->len - 1; snake->len--) {
			SEGMENT* segment = snake->head;
			for(int j = 0; j < snake->len - 2; j++) {
				segment = segment->next;
			}
			free(segment->next);
		}
		free(snake->head);
	}
	
	// Frees all allocated memory and quits SDL.
    void End(DISPLAY* display, COLOR* color, TIME* time, SNAKE* snake, BLUE_DOT* blueDot, RED_DOT* redDot)
	{
		if(display != NULL) {

			if(display->charset != NULL)	SDL_FreeSurface(display->charset);
			if(display->screen != NULL)		SDL_FreeSurface(display->screen);
			if(display->scrtex != NULL)		SDL_DestroyTexture(display->scrtex);
			if(display->renderer != NULL)	SDL_DestroyRenderer(display->renderer);
			if(display->window != NULL)		SDL_DestroyWindow(display->window);
			free(display);
		}
		if(color != NULL)	free(color);
		if(time != NULL)	free(time);
		if(snake != NULL) {
			freeSnake(snake);
			free(snake);
		}
		if(blueDot != NULL)	free(blueDot);
		if(redDot != NULL) free(redDot);

		SDL_Quit();
	}

	// Returns the game to its starting state.
	void Restart(TIME* time, SNAKE* snake, BLUE_DOT* blueDot, RED_DOT* redDot, int* end, char* move, int* score)
	{
		freeSnake(snake);
		initSnake(snake);
		initTime(time);
		generateBlueDot(blueDot, redDot, snake);
		initRedDot(redDot);

		*end = 0;
		*move = 'w';
		*score = 0;
	}

	// Writes the current game state to a file.
    void Save(TIME* time, SNAKE* snake, BLUE_DOT* blueDot, RED_DOT* redDot, char move, int score)
	{
		FILE* file = fopen("save.txt", "w");

		if(file == NULL) {
			printf("Cannot save the game.\n");
		}
		else {
			fprintf(file, "%lf %lf %d %lf\n", time->fps, time->fpsTimer, time->frames, time->worldTime);
			
			fprintf(file, "%c %d %lf %lf %lf\n", snake->lastMove, snake->len, snake->nextMove, snake->speed, snake->speedupTime);

			SEGMENT* current = snake->head;
			for(int i = 0; i < snake->len; i++) {
				if(i != 0) current = current->next;
				fprintf(file, "%d %d ", current->x, current->y);
			}
			fprintf(file, "\n");

			fprintf(file, "%d %d\n", blueDot->x, blueDot->y);

			fprintf(file, "%lf %lf %d %d\n", redDot->activeTimer, redDot->appearTimer, redDot->x, redDot->y);

			fprintf(file, "%c\n", move);

			fprintf(file, "%d", score);

			fclose(file);
		}
	}

	// Loads the saved game state from a file.
    int Load(TIME* time, SNAKE* snake, BLUE_DOT* blueDot, RED_DOT* redDot, char* move, int* score)
	{
		FILE* file = fopen("save.txt", "r");
		
		if(file == NULL) {
			printf("Cannot load the game.\n");
		}
		else {
			freeSnake(snake);

			if(fscanf(file, "%lf %lf %d %lf\n", &time->fps, &time->fpsTimer, &time->frames, &time->worldTime) != 4) return 0;

			time->delta = 0;
			time->t1 = SDL_GetTicks();
			time->t2 = time->t1;

			if(fscanf(file, "%c %d %lf %lf %lf\n", &snake->lastMove, &snake->len, &snake->nextMove, &snake->speed, &snake->speedupTime) != 5) return 0;

			snake->head = (SEGMENT*)malloc(sizeof(SEGMENT));
			SEGMENT* current = snake->head;

			for(int i = 0; i < snake->len; i++) {
				
				if(fscanf(file, "%d %d ", &current->x, &current->y) != 2) return 0;

				if(i < snake->len - 1) {
					current->next = (SEGMENT*)malloc(sizeof(SEGMENT));
					current = current->next;
				}
				else {
					current->next = NULL;
				}
			}

			if(fscanf(file, "%d %d\n", &blueDot->x, &blueDot->y) != 2) return 0;

			if(fscanf(file, "%lf %lf %d %d\n", &redDot->activeTimer, &redDot->appearTimer, &redDot->x, &redDot->y) != 4) return 0;

			if(fscanf(file, "%c\n", move) != 1) return 0;

			if(fscanf(file, "%d", score) != 1) return 0;

			fclose(file);

		}

		return 1;
	}

/////////////////



////////
/*MAIN*/
////////

	#ifdef __cplusplus
	extern "C"
	#endif
	int main(int argc, char **argv) 
	{
        // Checks if SDL was initialized properly.
		if(!Start()) return 1;

        // Allocates memory for game components.
		DISPLAY* display = (DISPLAY*)malloc(sizeof(DISPLAY));
		COLOR* color = (COLOR*)malloc(sizeof(COLOR));
		TIME* time = (TIME*)malloc(sizeof(TIME));
		SNAKE* snake = (SNAKE*)malloc(sizeof(SNAKE));
		BLUE_DOT* blueDot = (BLUE_DOT*)malloc(sizeof(BLUE_DOT));
		RED_DOT* redDot = (RED_DOT*)malloc(sizeof(RED_DOT));

		// Checks if display was initialized properly.
        if(!initDisplay(display)) {
			End(display, color, time, snake, blueDot, redDot);
			return 1;
		}

		// Initializes other game components.
		initColor(display, color);
		initTime(time);
		initSnake(snake);
		generateBlueDot(blueDot, redDot, snake);
		initRedDot(redDot);

		// Initializes game state trackers.
		int quit = 0, end = 0, score = 0;
		SDL_Event event;
		char move = 'w';

        // Main game loop.
		while(!quit) {
            // Updates timing.
			time->t2 = SDL_GetTicks();
			time->delta = (time->t2 - time->t1) * 0.001;
			time->t1 = time->t2;
			time->worldTime += time->delta;

			// Handles snake speed up.
			if(snake->speedupTime - time->delta <= 0) {
				snake->speedupTime = SPEEDUP_TIME;
				snake->speed *= SPEEDUP_RATE;
			}
			else {
				snake->speedupTime -= time->delta;
			}

			// Handles red dot appearance timing.
			if(redDot->appearTimer - time->delta <= 0) {
				generateRedDot(redDot, blueDot, snake);
				redDot->appearTimer = RED_DOT_TIMER + RA(RED_MIN_APP, RED_MAX_APP);
			}
			else {
				redDot->appearTimer -= time->delta;
			}

			// Handles red dot disappearance timing.
			if(redDot->activeTimer > 0) {
				redDot->activeTimer -= time->delta;

				if(redDot->activeTimer < 0) {
					
					redDot->activeTimer = 0;
				}
			}

			// Handles snake movement timing.
			if(snake->nextMove - time->delta <= 0) {
				snake->nextMove = snake->speed;
				if(moveSnake(snake, blueDot, redDot, move, &score)) {
					move = snake->lastMove;
				}
				else {
					end = 1;
				}
			}
			else {
				snake->nextMove -= time->delta;
			}

			// Updates FPS counter.
			time->fpsTimer += time->delta;
			if(time->fpsTimer > 0.5) {
				time->fps = time->frames * 2;
				time->frames = 0;
				time->fpsTimer -= 0.5;
			}

			
			// Renders all game elements.
			printBorder(display, score, color, time);
			printSnake(display, snake, color);
			printBlueDot(display, blueDot, color);
			if(redDot->activeTimer > 0) printRedDot(display, redDot, color);
			updateScreen(display);


			// Handles input events.
			while(SDL_PollEvent(&event)) {
				switch(event.type) {
					case SDL_KEYDOWN:
						if(event.key.keysym.sym == SDLK_ESCAPE) quit = 1;
						if(event.key.keysym.sym == SDLK_n) Restart(time, snake, blueDot, redDot, &end, &move, &score);
						if(event.key.keysym.sym == SDLK_s) Save(time, snake, blueDot, redDot, move, score);
						if(event.key.keysym.sym == SDLK_l) {
							if(!Load(time, snake, blueDot, redDot, &move, &score)) {
								End(display, color, time, snake, blueDot, redDot);
								printf("Unexpected error while loading the file.\n");
								return 1;
							}
						}
						if(event.key.keysym.sym == SDLK_UP) move = 'w';
						if(event.key.keysym.sym == SDLK_DOWN) move = 's';
						if(event.key.keysym.sym == SDLK_LEFT) move = 'a';
						if(event.key.keysym.sym == SDLK_RIGHT) move = 'd';
						break;
					case SDL_QUIT:
						quit = 1;
						break;
				}
			}
			
			time->frames++;

            // Game over logic.
			if(end == 1) {
				int finished = 0;
				printBorder(display, score, color, time);
				printEnd(display, score, color);
				updateScreen(display);
				while(!finished) {
					while(SDL_PollEvent(&event)) {
						switch(event.type) {
							case SDL_KEYDOWN:
								if(event.key.keysym.sym == SDLK_ESCAPE) {
									quit = 1;
									finished = 1;
								}
								if(event.key.keysym.sym == SDLK_n) {
									Restart(time, snake, blueDot, redDot, &end, &move, &score);
									finished = 1;
								} 
								break;
							case SDL_QUIT:
								quit = 1;
								finished = 1;
								break;
						}
					}
				}
			}
		}

		

		// Clean up.
		End(display, color, time, snake, blueDot, redDot);

		return 0;
	}

////////