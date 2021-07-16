#include <iostream>
#include <SDL2/SDL.h>
#include <cstdlib>
#include <cassert>
#include <cmath>
#include <cstring>

#define PI 3.14159
#define MAX_SCALE 4
#define MIN_SCALE 0.5
#define MAX_UNITS 200
#define MINIMAP_SHRINK 6
#define UI_BORDER 5
#define MAX_MAPWIDTH 80
#define MAX_MAPHEIGHT 70
#define CELL_SIZE 10
#define ACTIONMENU_SIZE 5
#define ACTIONMENU_BOXWIDTH 180
#define ACTIONMENU_BOXHEIGHT 24

SDL_Renderer *renderer = nullptr;
SDL_Window *window = nullptr;
SDL_Texture *tileset = nullptr;
SDL_Texture *unitSpritesheet = nullptr;
SDL_Texture *uiSpritesheet = nullptr;
SDL_Texture *font = nullptr;
float scale = 4;
float cameraX = 0, cameraY = 0;

typedef enum
{
	INFANTRY,
	ROCKET,
	TANK,
	BASE
}
unit_t;

typedef enum
{
	NO_ACTION,
	DESELECT_ALL,
	CANCEL_TASKS
}
action_t;

void initSDL()
{
	assert(SDL_Init(SDL_INIT_EVERYTHING) >= 0);
	window = SDL_CreateWindow("Simple RTS", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_RESIZABLE);
	assert(window != nullptr);
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	assert(renderer != nullptr);
	SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
}

void endSDL()
{
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

SDL_Texture *loadTexture(const char *filename)
{
	SDL_Surface *loadedSurface = SDL_LoadBMP(filename);
	assert(loadedSurface != nullptr);
	SDL_SetColorKey(loadedSurface, SDL_TRUE, SDL_MapRGB(loadedSurface->format, 0xff, 0x00, 0xff));
	SDL_Texture *newTexture = SDL_CreateTextureFromSurface(renderer, loadedSurface);
	SDL_FreeSurface(loadedSurface);
	assert(newTexture != nullptr);
	return newTexture;
}

void loadMedia()
{
	tileset = loadTexture("data/terrain.bmp");
	unitSpritesheet = loadTexture("data/units.bmp");
	uiSpritesheet = loadTexture("data/ui.bmp");
	font = loadTexture("data/font.bmp");
}

void freeMedia()
{
	SDL_DestroyTexture(font);
	SDL_DestroyTexture(uiSpritesheet);
	SDL_DestroyTexture(unitSpritesheet);
	SDL_DestroyTexture(tileset);
}

void drawText(int xo, int yo, const char *text, float textScale)
{
	int x = xo;
	int y = yo;
	for(const char *c = &text[0]; *c != 0; c++)
	{
		SDL_Rect src, dst;
		src.x = (*c % 16) * 6;
		src.y = (*c / 16) * 8;
		src.w = 6;
		src.h = 8;
		dst.x = x;
		dst.y = y;
		dst.w = 6 * textScale;
		dst.h = 8 * textScale;
		SDL_RenderCopy(renderer, font, &src, &dst);
		x += 6 * textScale;
		if(*c == '\n')
		{
			x = xo;
			y += 8 * textScale;
		}
	}
}

class Level
{
	int w, h;
	int *map;

	public:
	Level()
	{
		map = nullptr;
	}

	void setTile(int x, int y, int t)
	{
		if(x < 0 || y < 0 || x >= w || y >= h)
			return;
		map[y*w+x] = t;
	}

	int getTile(int x, int y)
	{
		if(x < 0 || y < 0 || x >= w || y >= h)
			return -1;
		return map[y*w+x];
	}

	int touchingTiles(int x, int y, int t)
	{
		int n = 0;
		for(int xm = -1; xm <= 1; xm++)
			for(int ym = -1; ym <= 1; ym++)
			{
				if(xm == 0 && ym == 0)
					continue;
				int tx = x + xm;
				int ty = y + ym;
				if(getTile(tx, ty) == t)
					n++;
			}
		return n;
	}

	void surroundTile(int x, int y, int t)
	{
		for(int xm = -1; xm <= 1; xm++)
			for(int ym = -1; ym <= 1; ym++)
			{
				if(xm != 0 && ym != 0)
					continue;
				setTile(x+xm, y+ym, t);
			}
	}

	void generate()
	{
		const int tileSize = CELL_SIZE;
		int mw = MAX_MAPWIDTH - rand() % (MAX_MAPWIDTH/3);
		int mh = MAX_MAPHEIGHT - rand() % (MAX_MAPHEIGHT/3);
		int *mmap = new int[mw*mh];
		for(int i = 0; i < mw*mh; i++)
			mmap[i] = 0;

		int cx = mw/2 - 4 + rand() % 4;
		int cy = mh/2 - 4 + rand() % 4;
		cameraX = (cx + 0.5) * tileSize;
		cameraY = (cy + 0.5) * tileSize;
		int r = 20 + rand() % 10;
		float a1 = 0.03 + (rand() % (int)(PI*20)) / 100.0;
		float a2 = a1 + (rand() % (int)(PI*70)) / 100.0;
		for(float a = a1; a < a2; a += 0.05)
			for(int i = 0; i < r; i++)
			{
				int x = cx + cosf(a) * i;
				int y = cy + sinf(a) * i;
				if(x < 0 || y < 0 || x >= mw || y >= mh)
					continue;
				if(rand() % 2)
					mmap[y*mw+x] = 2 + rand() % 2;
			}

		int extraIslands = rand() % 250;
		for(int i = 0; i < extraIslands; i++)
		{
			int x = rand() % mw;
			int y = rand() % mh;
			mmap[y*mw+x] = 2 + rand() % 2;
		}

		for(int i = 0; i < 3; i++)
			for(int x = 0; x < mw; x++)
				for(int y = 0; y < mh; y++)
				{
					if(mmap[y*mw+x] <= 1)
						continue;
					for(int xm = -1; xm <= 1; xm++)
						for(int ym = -1; ym <= 1; ym++)
						{
							if(ym != 0 && xm != 0)
								continue;
							if(xm == 0 && ym == 0)
								continue;
							if(x+xm < 0 || y+ym < 0 || x+xm >= mw || y+ym >= mh)
								continue;
							if(mmap[(y+ym)*mw+(x+xm)] == 0)
								mmap[(y+ym)*mw+(x+xm)] = mmap[y*mw+x]-1;
						}
				}

		w = mw * tileSize;
		h = mh * tileSize;
		map = new int[w*h];
		for(int x = 0; x < mw; x++)
			for(int y = 0; y < mh; y++)
				for(int ix = x*tileSize; ix < (x+1)*tileSize; ix++)
					for(int iy = y*tileSize; iy < (y+1)*tileSize; iy++)
						map[iy*w+ix] = mmap[y*mw+x];

		delete mmap;

		for(int x = 0; x < w; x++)
			for(int y = 0; y < h; y++)
			{
				if(getTile(x, y) == 2)
					if(rand() % 5 > 2)
						setTile(x, y, 1);
				if(getTile(x, y) == 3)
					if(rand() % 7 == 0)
						setTile(x, y, 1);
				if(getTile(x, y) == 1)
					if(rand() % 3 == 0)
						setTile(x, y, 2);
			}

		for(int i = 0; i < 5; i++)
			for(int x = 0; x < w; x++)
				for(int y = 0; y < h; y++)
				{
					int t = getTile(x, y);
					if(t == 1)
					{
						if(touchingTiles(x, y, 2) > 3)
							setTile(x, y, 1);
						if(touchingTiles(x, y, 3) > 2)
							setTile(x, y, 3);
						if(touchingTiles(x, y, 0) > 3)
							surroundTile(x, y, 0);
					}
					if(t == 0)
						if(touchingTiles(x, y, 1) > 3)
							setTile(x, y, 1);
					if(t == 2)
					{
						if(touchingTiles(x, y, 0) > 4)
							setTile(x, y, 0);
						if(touchingTiles(x, y, 2) < 3)
							surroundTile(x, y, 1);
					}
					if(t == 3)
						if(touchingTiles(x, y, 3) < 5)
							setTile(x, y, 1);
				}
	}

	~Level()
	{
		if(map != nullptr)
			delete map;
	}

	void draw()
	{
		int ww, wh;
		SDL_GetWindowSize(window, &ww, &wh);
		for(int x = cameraX-(ww/2)/(8*scale)-3; x <= cameraX+(ww/2)/(8*scale)+3; x++)
			for(int y = cameraY-(wh/2)/(8*scale)-3; y <= cameraY+(wh/2)/(8*scale)+3; y++)
			{
				if(x < 0 || y < 0 || x >= w || y >= h)
					continue;
				int t = map[y*w+x];
				int xo = cameraX * 8 * scale - ww/2;
				int yo = cameraY * 8 * scale - wh/2;
				SDL_Rect src, dst;
				src.x = (t % 8) * 8;
				src.y = (t / 8) * 8;
				src.w = 8;
				src.h = 8;
				dst.x = x * 8 * scale - xo;
				dst.y = y * 8 * scale - yo;
				dst.w = 8 * (scale+0.2);
				dst.h = 8 * (scale+0.2);
				SDL_RenderCopy(renderer, tileset, &src, &dst);
			}
	}

	void drawMinimap()
	{
		const int s = MINIMAP_SHRINK;
		int sw, sh;
		SDL_GetWindowSize(window, &sw, &sh);
		int xo = sw - (w/s)/2 - UI_BORDER - ACTIONMENU_BOXWIDTH/2;
		int yo = UI_BORDER;
		for(int x = 0; x < w; x += s)
			for(int y = 0; y < h; y += s)
			{
				int t = getTile(x, y);
				if(t == 0)
					continue;
				if(t == 1)
					SDL_SetRenderDrawColor(renderer, 0xef, 0xef, 0xef, 0x98);
				if(t == 2)
					SDL_SetRenderDrawColor(renderer, 0xd0, 0xd0, 0xd0, 0x98);
				if(t == 3)
					SDL_SetRenderDrawColor(renderer, 0xaf, 0xaf, 0xaf, 0x98);
				SDL_RenderDrawPoint(renderer, x/s+xo, y/s+yo);
			}
		SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
		SDL_Rect r;
		r.x = xo;
		r.y = yo;
		r.w = w/s + 1;
		r.h = h/s + 1;
		SDL_RenderDrawRect(renderer, &r);
		r.x = xo + cameraX / s - (((sw/8.0) / scale) / 2.0) / s;
		r.y = yo + cameraY / s - (((sh/8.0) / scale) / 2.0) / s;
		r.w = ((sw/8.0) / scale) / s;
		r.h = ((sh/8.0) / scale) / s;
		SDL_RenderDrawRect(renderer, &r);
		SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);
	}

	friend class Game;
};

class Unit
{
	int team;
	unit_t type;
	float x, y, a, tx, ty, speed[4];
	bool moving, selected;

	public:
	void init(float x, float y, unit_t type, int team)
	{
		Unit::x = x;
		Unit::y = y;
		a = PI/2;
		moving = false;
		selected = false;
		Unit::type = type;
		Unit::team = team;
		switch(type)
		{
		case INFANTRY:
			speed[0] = 0.005;
			speed[1] = 0.03;
			speed[2] = 0.02;
			speed[3] = 0.01;
			break;
		case ROCKET:
			speed[0] = 0.003;
			speed[1] = 0.025;
			speed[2] = 0.015;
			speed[3] = 0.005;
			break;
		case TANK:
			speed[0] = 0.001;
			speed[1] = 0.015;
			speed[2] = 0.01;
			speed[3] = 0.005;
			break;
		case BASE:
			speed[0] = 0;
			speed[1] = 0;
			speed[2] = 0;
			speed[3] = 0;
			break;
		}
	}

	void target(float x, float y)
	{
		if(type == BASE)
			return;
		moving = true;
		tx = x;
		ty = y;
	}

	void update(int t)
	{
		if(moving)
		{
			a = atan2(ty-y, tx-x);
			x += cosf(a) * speed[t];
			y += sinf(a) * speed[t];
			if(pow(tx-x, 2) + pow(ty-y, 2) < 1.5*1.5)
				moving = false;
		}
	}

	void draw()
	{
		int sw, sh;
		SDL_GetWindowSize(window, &sw, &sh);
		int xo = cameraX * 8 * scale - sw/2;
		int yo = cameraY * 8 * scale - sh/2;
		SDL_Rect src, dst;

		if(selected)
		{
			src.x = 0;
			src.y = 0;
			src.w = 32;
			src.h = 32;
			dst.x = x * 8 * scale - 16 * scale - xo;
			dst.y = y * 8 * scale - 16 * scale - yo;
			dst.w = 32 * scale;
			dst.h = 32 * scale;
			SDL_RenderCopy(renderer, uiSpritesheet, &src, &dst);
			if(moving)
			{
				src.x = 32;
				dst.x = tx * 8 * scale - 16 * scale - xo;
				dst.y = ty * 8 * scale - 16 * scale - yo;
				SDL_RenderCopy(renderer, uiSpritesheet, &src, &dst);
				SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
				SDL_RenderDrawLine(renderer, x * 8 * scale - xo, y * 8 * scale - yo, tx * 8 * scale - xo, ty * 8 * scale - yo);
				SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);
			}
		}

		src.x = (type % 4) * 32;
		src.y = (type / 4) * 32 + team * 64;
		src.w = 32;
		src.h = 32;
		dst.x = x * 8 * scale - 16 * scale - xo;
		dst.y = y * 8 * scale - 16 * scale - yo;
		dst.w = 32 * scale;
		dst.h = 32 * scale;
		float deg = (a / PI) * 180 + 90;
		SDL_Point pvt;
		pvt.x = 16 * scale;
		pvt.y = 16 * scale;
		SDL_RenderCopyEx(renderer, unitSpritesheet, &src, &dst, deg, &pvt, SDL_FLIP_NONE);
	}

	void populateMinimap(int w, int h)
	{
		const int s = MINIMAP_SHRINK;
		int sw, sh;
		SDL_GetWindowSize(window, &sw, &sh);
		int xo = sw - (w/s)/2 - UI_BORDER - ACTIONMENU_BOXWIDTH/2;
		int yo = UI_BORDER;
		switch(team)
		{
		case 0:
			SDL_SetRenderDrawColor(renderer, 0xff, 0x00, 0x00, 0xff);
			break;
		case 1:
			SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x00, 0xff);
			break;
		}
		SDL_Rect r;
		r.x = xo + x / s - 1;
		r.y = yo + y / s - 1;
		r.w = 3;
		r.h = 3;
		SDL_RenderFillRect(renderer, &r);
		SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);
	}

	bool busy()
	{
		if(moving)
			return true;
		return false;
	}

	void stop()
	{
		moving = false;
	}

	friend class Game;
};

class Player
{
	int team;
	bool ai;
	Unit *base;

	public:
	void init(int team, bool ai, Unit *base)
	{
		Player::team = team;
		Player::ai = ai;
		Player::base = base;
	}

	friend class Game;
};

class Game
{
	Level level;
	SDL_Event event;
	bool quit, panning, redraw, clicking;
	int lastUpdate;
	int mouseX, mouseY;
	Unit *units[MAX_UNITS];
	const Uint8 *keyboardState;
	Player players[2];
	action_t actionMenu[ACTIONMENU_SIZE];
	int totalSelected;

	public:
	Game()
	{
		srand(time(nullptr));
		initSDL();
		loadMedia();
		keyboardState = SDL_GetKeyboardState(NULL);
	}

	~Game()
	{
		for(int i = 0; i < MAX_UNITS; i++)
			if(units[i] != nullptr)
			{
				delete units[i];
				units[i] = nullptr;
			}
		endSDL();
		freeMedia();
	}

	void addUnit(float x, float y, unit_t type, int team)
	{
		int i;
		for(i = 0; units[i] != nullptr; i++);
		units[i] = new Unit();
		units[i]->init(x, y, type, team);
	}

	void init()
	{
		level.generate();
		panning = false;
		quit = false;
		redraw = true;
		clicking = false;
		lastUpdate = SDL_GetTicks();
		SDL_GetMouseState(&mouseX, &mouseY);
		for(int i = 0; i < MAX_UNITS; i++)
			units[i] = nullptr;
		for(int i = 0; i < ACTIONMENU_SIZE; i++)
			actionMenu[i] = NO_ACTION;
		totalSelected = 0;
		addUnit(level.w/2, level.h/2, INFANTRY, 0);
	}

	const char *actionString(action_t a)
	{
		switch(a)
		{
		case NO_ACTION:
			return "No action";
		case DESELECT_ALL:
			return "Deselect all";
		case CANCEL_TASKS:
			return "Cancel tasks";
		default:
			return "";
		}
	}
	
	void drawActionMenu()
	{
		int sw, sh;
		SDL_GetWindowSize(window, &sw, &sh);
		int xo = sw - UI_BORDER - ACTIONMENU_BOXWIDTH;
		int yo = UI_BORDER + level.h / MINIMAP_SHRINK + UI_BORDER;
		for(int i = 0; i < ACTIONMENU_SIZE; i++)
		{
			SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0x88);
			SDL_Rect r;
			r.x = xo;
			r.y = yo + (ACTIONMENU_BOXHEIGHT + UI_BORDER) * i;
			r.w = ACTIONMENU_BOXWIDTH;
			r.h = ACTIONMENU_BOXHEIGHT;
			SDL_RenderFillRect(renderer, &r);
			if(actionMenu[i] != NO_ACTION)
			{
				SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
				SDL_RenderDrawRect(renderer, &r);
				float ts = 2;
				const char *str = actionString(actionMenu[i]);
				int x = r.x + ACTIONMENU_BOXWIDTH/2 - (strlen(str)/2)*6*ts;
				int y = r.y + ACTIONMENU_BOXHEIGHT/2 - 8*ts/2;
				drawText(x, y, str, ts);
			}
		}
		SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);
	}

	void draw()
	{
		SDL_RenderClear(renderer);
		level.draw();
		for(int i = 0; i < MAX_UNITS; i++)
			if(units[i] != nullptr)
				units[i]->draw();
		level.drawMinimap();
		for(int i = 0; i < MAX_UNITS; i++)
			if(units[i] != nullptr)
				units[i]->populateMinimap(level.w, level.h);
		drawActionMenu();
		SDL_RenderPresent(renderer);
	}

	bool clickUI()
	{
		int mx, my;
		SDL_GetMouseState(&mx, &my);
		int sw, sh;
		SDL_GetWindowSize(window, &sw, &sh);
		int x1, x2, y1, y2;
		x1 = sw - (level.w/MINIMAP_SHRINK)/2 - UI_BORDER - ACTIONMENU_BOXWIDTH/2;
		x2 = sw - UI_BORDER;
		y1 = UI_BORDER;
		y2 = UI_BORDER + level.h / MINIMAP_SHRINK;
		if(mx > x1 && mx < x2 && my > y1 && my < y2)
		{
			cameraX = (mx - x1) * MINIMAP_SHRINK;
			cameraY = (my - y1) * MINIMAP_SHRINK;
			return true;
		}

		clicking = false;

		for(int i = 0; i < ACTIONMENU_SIZE; i++)
		{
			x1 = sw - UI_BORDER - ACTIONMENU_BOXWIDTH;
			y1 = UI_BORDER*2 + level.h / MINIMAP_SHRINK + (ACTIONMENU_BOXHEIGHT + UI_BORDER) * i;
			x2 = sw - UI_BORDER;
			y2 = y1 + ACTIONMENU_BOXHEIGHT;
			if(!(mx > x1 && my > y1 && mx < x2 && my < y2))
				continue;

			int j;
			switch(actionMenu[i])
			{
			default:
				return true;
			case DESELECT_ALL:
				selectAll(false);
				return true;
			case CANCEL_TASKS:
				for(j = 0; j < MAX_UNITS; j++)
					if(units[j] != nullptr)
						if(units[j]->selected)
							units[j]->stop();
				return true;
			}
		}

		return false;
	}

	void clickMap()
	{
		int mx, my;
		SDL_GetMouseState(&mx, &my);
		int sw, sh;
		SDL_GetWindowSize(window, &sw, &sh);
		float cx = cameraX + ((mx-sw/2.0) / scale) / 8.0;
		float cy = cameraY + ((my-sh/2.0) / scale) / 8.0;
		if(cx < 0 || cy < 0 || cx >= level.w || cy >= level.h)
			return;

		bool select = false;
		for(int i = 0; i < MAX_UNITS; i++)
		{
			if(units[i] == nullptr)
				continue;
			if(pow(units[i]->x - cx, 2) + pow(units[i]->y - cy, 2) < 1.5*1.5 && !select)
			{
				select = true;
				units[i]->selected = !keyboardState[SDL_SCANCODE_LSHIFT];
				totalSelected += units[i]->selected - !units[i]->selected;
			}
		}
		if(select)
			return;

		for(int i = 0; i < MAX_UNITS; i++)
			if(units[i] != nullptr)
				if(units[i]->selected)
					units[i]->target(cx, cy);
	}

	void selectAll(bool select)
	{
		for(int i = 0; i < MAX_UNITS; i++)
			if(units[i] != nullptr)
			{
				units[i]->selected = select;
				totalSelected += select;
				totalSelected -= !select;
			}
	}

	void clearActionMenu()
	{
		for(int i = 0; i < ACTIONMENU_SIZE; i++)
			actionMenu[i] = NO_ACTION;
	}

	void addAction(action_t a)
	{
		int i;
		for(i = 0; actionMenu[i] != NO_ACTION && i < ACTIONMENU_SIZE; i++);
		if(i >= ACTIONMENU_SIZE)
			return;
		actionMenu[i] = a;
	}

	void update()
	{
		redraw = true;

		clearActionMenu();
		if(totalSelected > 0)
			addAction(DESELECT_ALL);

		int mx, my;
		SDL_GetMouseState(&mx, &my);
		if(panning)
		{
			cameraX += ((mouseX-mx)/8.0)/scale;
			cameraY += ((mouseY-my)/8.0)/scale;
		}
		if(cameraX < 0) cameraX = 0;
		if(cameraY < 0) cameraY = 0;
		if(cameraX > level.w) cameraX = level.w;
		if(cameraY > level.h) cameraY = level.h;
		mouseX = mx;
		mouseY = my;

		bool busyUnits = false;
		for(int i = 0; i < MAX_UNITS; i++)
			if(units[i] != nullptr)
			{
				units[i]->update(level.getTile(units[i]->x, units[i]->y));
				if(units[i]->selected)
					if(units[i]->busy())
						busyUnits = true;
			}
		if(busyUnits)
			addAction(CANCEL_TASKS);

		if(clicking)
			if(!clickUI())
				clickMap();

		cameraX += (keyboardState[SDL_SCANCODE_RIGHT] - keyboardState[SDL_SCANCODE_LEFT]) / (8.0*scale) * 8;
		cameraY += (keyboardState[SDL_SCANCODE_DOWN] - keyboardState[SDL_SCANCODE_UP]) / (8.0*scale) * 8;
	}

	void run()
	{
		while(SDL_PollEvent(&event) != 0)
		{
			switch(event.type)
			{
			case SDL_QUIT:
				quit = true;
				break;
			case SDL_MOUSEBUTTONDOWN:
				switch(event.button.button)
				{
				case SDL_BUTTON_RIGHT:
					panning = true;
					break;
				case SDL_BUTTON_LEFT:
					clicking = true;
					break;
				}
				break;
			case SDL_MOUSEBUTTONUP:
				switch(event.button.button)
				{
				case SDL_BUTTON_RIGHT:
					panning = false;
					break;
				case SDL_BUTTON_LEFT:
					clicking = false;
					break;
				}
				break;
			case SDL_MOUSEWHEEL:
				scale += event.wheel.y * 0.1;
				if(scale < MIN_SCALE)
					scale = MIN_SCALE;
				if(scale > MAX_SCALE)
					scale = MAX_SCALE;
				break;
			case SDL_KEYDOWN:
				switch(event.key.keysym.sym)
				{
				case SDLK_ESCAPE:
					quit = true;
					break;
				case SDLK_a:
					if(keyboardState[SDL_SCANCODE_LCTRL])
						selectAll(!keyboardState[SDL_SCANCODE_LSHIFT]);
					break;
				}
			case SDL_WINDOWEVENT:
				switch(event.window.event)
				{
				case SDL_WINDOWEVENT_RESIZED:
					SDL_SetWindowSize(window, event.window.data1, event.window.data2);
					break;
				}
			}
		}

		int currentTime = SDL_GetTicks();
		while(currentTime - lastUpdate > 10)
		{
			update();
			lastUpdate += 10;
		}

		if(redraw)
		{
			draw();
			redraw = false;
		}
	}

	void loop()
	{
		while(!quit)
			run();
	}
};

int main(int argc, char **args)
{
	Game game;
	game.init();
	game.loop();
	return 0;
}
