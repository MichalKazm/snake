#define _USE_MATH_DEFINES
#include<math.h>
#include<stdio.h>
#include<string.h>
#include<time.h>

extern "C" {
#include"./SDL2-2.0.10/include/SDL.h"
#include"./SDL2-2.0.10/include/SDL_main.h"
}


#define SEGMENT_SIZE	20													//size of one segment of the snake in px

#define COLS			30													//number of columns of the map
#define ROWS			30													//number of rows of the map

#define SCREEN_WIDTH	(COLS * SEGMENT_SIZE)								//size of the map is COLS x ROWS
#define SCREEN_HEIGHT	(ROWS * SEGMENT_SIZE)								//where each cell is size of a segment

#define BORDER_LENGTH	(8 + 1)												//1 additional pixel for the border  
#define INFO_LENGTH		(44 + 1)											//around the screen

#define WINDOW_WIDTH	(SCREEN_WIDTH  + 2 * BORDER_LENGTH)					
#define WINDOW_HEIGHT	(SCREEN_HEIGHT  + BORDER_LENGTH + INFO_LENGTH)		//info section is where all of the information is displayed

#define STARTING_X 		20													//coordinates of the cell where 
#define STARTING_Y 		20													//a head of the snake will be
#define STARTING_LENGTH	8													//all remaining segments are below head in the straight line

#define STARTING_SPEED	0.25												//time after which snake will move in s
#define SPEEDUP_TIME	10													//time after which snake's speed will increase
#define SPEEDUP_RATE	0.9													//constant by which speed will be multiplied to make snake move faster	
#define SLOWDOWN_RATE	1.5													//constant by which speed will be multiplied to make snake move slower

#define RED_BAR_SIZE	100													//width of the bar showing time left before red dot disapears in px
#define RED_DOT_TIMER	10													//time for which red ddt is active in s
#define RED_MIN_APP		3													//min and max time after which red dot will
#define RED_MAX_APP		15													//appear after last one disappeared in s

#define RED_SCORE		250													//number of points given for collecting a red dot		
#define BLUE_SCORE		100													//number of points given for collecting a blue dot

#define RA(min, max) ( (min) + rand() % ((max) - (min) + 1) )				// random number between min and max (inc)

typedef struct {
	SDL_Surface *screen, *charset;
	SDL_Texture *scrtex;
	SDL_Window *window;
	SDL_Renderer *renderer;
} DISPLAY;

typedef struct {
	int black;
	int green; 
	int red; 
	int blue;
	int white;
} COLOR;

typedef struct {
	int t1;
	int t2;
	int frames;
	double delta;
	double worldTime;
	double fpsTimer;
	double fps;
} TIME;

typedef struct segment{
	int x;
	int y;
	struct segment* next;
} SEGMENT;

typedef struct {
	int len;
	double nextMove;
	double speed;
	double speedupTime;
	char lastMove;
	SEGMENT* head;
} SNAKE;

typedef struct {
	int x;
	int y;
} BLUE_DOT;

typedef struct {
	int x;
	int y;
	double activeTimer;
	double appearTimer;
} RED_DOT;


////////////////////////////
/*INITIALISATION FUNCTIONS*/
////////////////////////////	

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

	void initColor(DISPLAY* display, COLOR* color)
	{
		SDL_SetColorKey(display->charset, true, 0x000000);

		color->black = SDL_MapRGB(display->screen->format, 0x00, 0x00, 0x00);
		color->green = SDL_MapRGB(display->screen->format, 0x00, 0xFF, 0x00);
		color->red = SDL_MapRGB(display->screen->format, 0xFF, 0x00, 0x00);
		color->blue = SDL_MapRGB(display->screen->format, 0x11, 0x11, 0xCC);
		color->white = SDL_MapRGB(display->screen->format, 0xFF, 0xFF, 0xFF);
	}

	void initTime(TIME* time)
	{
		time->t1 = SDL_GetTicks();
		time->frames = 0;
		time->fpsTimer = 0;
		time->fps = 0;
		time->worldTime = 0;
	}

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

		// next = snake->head;

		// for(int i = 0; i < STARTING_LENGTH; i++) {
		// 	printf("%d) x: %d y: %d ptr: %p\n", i, next->x, next->y, next->next);
		// 	next = next->next;
		// }
	}

	void initRedDot (RED_DOT* redDot)
	{
		redDot->appearTimer = RA(RED_MIN_APP, RED_MAX_APP);
		redDot->activeTimer = 0;
	}

////////////////////////////

/////////////////////
/*DRAWING FUNCTIONS*/
/////////////////////
	
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

	void DrawPixel(SDL_Surface *surface, int x, int y, Uint32 color) 
	{
		int bpp = surface->format->BytesPerPixel;
		Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;
		*(Uint32 *)p = color;
	}

	void DrawLine(SDL_Surface *screen, int x, int y, int l, int dx, int dy, Uint32 color) 
	{
		for(int i = 0; i < l; i++) {
			DrawPixel(screen, x, y, color);
			x += dx;
			y += dy;
		}
	}

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

	int checkCollision(SNAKE* snake, int x, int y)							//checks if cell with cooridates x, y is currently occupied by
	{																		//one ofsnake segments (doesn't count the tail) because it will
		SEGMENT* current = snake->head;										//disapear after a move so new head can be placed in it's place

		for(int i = 0; i < snake->len - 1; current = current->next, i++)
		{
			if((x == current->x) && (y == current->y))
				return 1;
		}
		
		return 0;
	}

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
				//printf("x: %d y: %d\n", x, y);
			}
		}
	}

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
				//printf("x: %d y: %d\n", x, y);
			}
		}

		redDot->activeTimer = RED_DOT_TIMER;
	}

	int moveSnake(SNAKE* snake, BLUE_DOT* blueDot, RED_DOT* redDot, char move, int* score)
	{
		int newX, newY;
		char lastMove = snake->lastMove;
		SEGMENT* current = snake->head;

		if(((move == 'w') && (lastMove == 's'))  ||  ((move == 's') && (lastMove == 'w'))  ||  ((move == 'a') && (lastMove == 'd'))  ||  ((move == 'd') && (lastMove == 'a'))) {
			move = lastMove;
		}
		
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
		

		if((newX == blueDot->x) && (newY == blueDot->y)) {		//makes snake longer and generates 'fake' tail
			for(int j = 0; j < snake->len - 1; j++) {			//which will make 'real' tail count in checkCollision() 
				current = current->next;						//and which will be deleted instead of 'real' tail
			}													//(it is easier to do this this way)
			current->next = (SEGMENT*)malloc(sizeof(SEGMENT));
			snake->len++;
			newBlueDot = 1;

			*score += BLUE_SCORE;
		}

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

	void printBorder(DISPLAY* display, int score, COLOR* color, TIME* time)
	{
		char text[128];
		
		SDL_FillRect(display->screen, NULL, color->blue);

		DrawRectangle(display->screen, BORDER_LENGTH - 1, INFO_LENGTH - 1, SCREEN_WIDTH + 1, SCREEN_HEIGHT + 1, color->white, color->black);	//+/- 1 is beacuse of the 1 px border around the screen which is counted into the size of the screen
																																				//but it is useful to have it saved in border/info size which is used when printing the snake
		sprintf(text, "Time: %.1lf    FPS: %.0lf", time->worldTime, time->fps);
		DrawString(display->screen, BORDER_LENGTH + 2, 10, text, display->charset);

		sprintf(text, "'Esc' - Exit    'n' - New game");
		DrawString(display->screen, display->screen->w - (strlen(text) * 8) - (BORDER_LENGTH + 2), 10, text, display->charset);

		sprintf(text, "Score: %d", score);
		DrawString(display->screen, BORDER_LENGTH + 2, 26, text, display->charset);

		sprintf(text, "Implemented: 1, 2, 3, 4, A, B, C, D, E");
		DrawString(display->screen, display->screen->w - (strlen(text) * 8) - (BORDER_LENGTH + 2), 26, text, display->charset);

	}

	void printSnake(DISPLAY* display, SNAKE* snake, COLOR* color)
	{
		int len = snake->len;
		SEGMENT* current = snake->head;
		int x, y;

		for(int i = 0; i < len; i++)
		{
			if(i != 0)	current = current->next;

			x = BORDER_LENGTH + current->x * SEGMENT_SIZE;
			y = INFO_LENGTH + current->y * SEGMENT_SIZE;
			DrawRectangle(display->screen, x, y, SEGMENT_SIZE, SEGMENT_SIZE, color->green, color->green);
		}

	}

	void printBlueDot(DISPLAY* display, BLUE_DOT* blueDot, COLOR* color)
	{
		int x = BORDER_LENGTH + blueDot->x * SEGMENT_SIZE + 0.25 * SEGMENT_SIZE;
		int y = INFO_LENGTH + blueDot->y * SEGMENT_SIZE + 0.25 * SEGMENT_SIZE;

		DrawRectangle(display->screen, x, y, SEGMENT_SIZE / 2, SEGMENT_SIZE / 2, color->blue, color->blue);
	}

	void printRedDot(DISPLAY* display, RED_DOT* redDot, COLOR* color)
	{
		int x = BORDER_LENGTH + redDot->x * SEGMENT_SIZE + 0.25 * SEGMENT_SIZE;
		int y = INFO_LENGTH + redDot->y * SEGMENT_SIZE + 0.25 * SEGMENT_SIZE;

		DrawRectangle(display->screen, x, y, SEGMENT_SIZE / 2, SEGMENT_SIZE / 2, color->red, color->red);

		int greenWidth = RED_BAR_SIZE * (redDot->activeTimer / RED_DOT_TIMER);
		
		int redWidth = RED_BAR_SIZE - greenWidth;

		if(greenWidth > 0) DrawRectangle(display->screen, display->screen->w / 2 - RED_BAR_SIZE / 2, 10, greenWidth, 8, color->green, color->green);

		if(redWidth > 0) DrawRectangle(display->screen, display->screen->w / 2 - RED_BAR_SIZE / 2 + greenWidth, 10, redWidth, 8, color->red, color->red);
	}

	void printEnd(DISPLAY* display, int score, COLOR* color)
	{
		char text[128];

		sprintf(text, "Your score is: %d", score);
		DrawString(display->screen, display->screen->w / 2 - strlen(text) * 8 / 2, INFO_LENGTH + SCREEN_HEIGHT / 2 - 4 - 16, text, display->charset);

		sprintf(text, "Press 'Esc' to exit");
		DrawString(display->screen, display->screen->w / 2 - strlen(text) * 8 / 2, INFO_LENGTH + SCREEN_HEIGHT / 2 - 4, text, display->charset);

		sprintf(text, "Press 'n' to start new game");
		DrawString(display->screen, display->screen->w / 2 - strlen(text) * 8 / 2, INFO_LENGTH + SCREEN_HEIGHT / 2 - 4 + 16, text, display->charset);

	}
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
		if(!Start()) return 1;

		DISPLAY* display = (DISPLAY*)malloc(sizeof(DISPLAY));
		COLOR* color = (COLOR*)malloc(sizeof(COLOR));
		TIME* time = (TIME*)malloc(sizeof(TIME));
		SNAKE* snake = (SNAKE*)malloc(sizeof(SNAKE));
		BLUE_DOT* blueDot = (BLUE_DOT*)malloc(sizeof(BLUE_DOT));
		RED_DOT* redDot = (RED_DOT*)malloc(sizeof(RED_DOT));

		if(!initDisplay(display)) {
			End(display, color, time, snake, blueDot, redDot);
			return 1;
		}

		initColor(display, color);
		initTime(time);
		initSnake(snake);

		generateBlueDot(blueDot, redDot, snake);

		initRedDot(redDot);

		int quit = 0, end = 0, score = 0;
		SDL_Event event;
		char move = 'w';

		while(!quit) {
			time->t2 = SDL_GetTicks();
			
			time->delta = (time->t2 - time->t1) * 0.001;
			time->t1 = time->t2;

			time->worldTime += time->delta;


			if(snake->speedupTime - time->delta <= 0) {
				snake->speedupTime = SPEEDUP_TIME;
				snake->speed *= SPEEDUP_RATE;
			}
			else {
				snake->speedupTime -= time->delta;
			}


			if(redDot->appearTimer - time->delta <= 0) {
				generateRedDot(redDot, blueDot, snake);
				redDot->appearTimer = RED_DOT_TIMER + RA(RED_MIN_APP, RED_MAX_APP);
			}
			else {
				redDot->appearTimer -= time->delta;
			}
			
			if(redDot->activeTimer > 0) {
				redDot->activeTimer -= time->delta;

				if(redDot->activeTimer < 0) {
					
					redDot->activeTimer = 0;
				}
			}


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





			time->fpsTimer += time->delta;
			if(time->fpsTimer > 0.5) {
				time->fps = time->frames * 2;
				time->frames = 0;
				time->fpsTimer -= 0.5;
			}

			

			printBorder(display, score, color, time);

			printSnake(display, snake, color);

			printBlueDot(display, blueDot, color);

			if(redDot->activeTimer > 0) printRedDot(display, redDot, color);

			updateScreen(display);



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

		

		
		End(display, color, time, snake, blueDot, redDot);

		return 0;
	}

////////