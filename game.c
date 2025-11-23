/*
 * Endless Dodge - A small, production-ready 2D arcade game in C using SDL2.
 *
 * Features:
 *  - Simple and addictive dodge gameplay (endless, increasing difficulty).
 *  - Game states: MENU, PLAYING, PAUSED, GAME_OVER.
 *  - High score persistence to a local file (highscore.dat).
 *  - Error-checked SDL initialization and resource management.
 *  - Window title shows score, high score, and state.
 *
 * Controls:
 *  - Move Left:  A or Left Arrow
 *  - Move Right: D or Right Arrow
 *  - Start / Restart: Enter
 *  - Pause / Resume: P
 *  - Quit: Esc or close window
 */

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>

/* ----------------------------- Configuration ----------------------------- */

#define WINDOW_WIDTH   800
#define WINDOW_HEIGHT  600

#define TARGET_FPS     60
#define FRAME_TIME_MS  (1000 / TARGET_FPS)

/* Player configuration */
#define PLAYER_WIDTH       80.0f
#define PLAYER_HEIGHT      20.0f
#define PLAYER_SPEED       500.0f  /* pixels per second */

/* Obstacle configuration */
#define MAX_OBSTACLES             64
#define OBSTACLE_MIN_WIDTH        40.0f
#define OBSTACLE_MAX_WIDTH        140.0f
#define OBSTACLE_HEIGHT           20.0f
#define OBSTACLE_BASE_SPEED       200.0f
#define OBSTACLE_SPEED_INCREMENT  0.03f   /* added per second elapsed */
#define OBSTACLE_BASE_INTERVAL    700.0f  /* ms between spawns at start */
#define OBSTACLE_MIN_INTERVAL     140.0f
#define OBSTACLE_INTERVAL_DECAY   0.985f  /* multiply interval after each spawn */

#define BACKGROUND_COLOR_R 15
#define BACKGROUND_COLOR_G 15
#define BACKGROUND_COLOR_B 25

#define PLAYER_COLOR_R 60
#define PLAYER_COLOR_G 220
#define PLAYER_COLOR_B 120

#define OBSTACLE_COLOR_R 230
#define OBSTACLE_COLOR_G 60
#define OBSTACLE_COLOR_B 80

#define MENU_TINT_ALPHA      120
#define PAUSE_TINT_ALPHA     120
#define GAME_OVER_TINT_ALPHA 160

static const char *HIGHSCORE_FILE = "highscore.dat";

/* ------------------------------ Logging ---------------------------------- */

#define LOG_ERROR(fmt, ...) \
    fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) \
    fprintf(stdout, "[INFO] " fmt "\n", ##__VA_ARGS__)

/* ------------------------------- Types ----------------------------------- */

typedef enum {
    GAME_STATE_MENU = 0,
    GAME_STATE_PLAYING,
    GAME_STATE_PAUSED,
    GAME_STATE_GAME_OVER
} GameState;

typedef struct {
    float x;
    float y;
    float w;
    float h;
    float speed;
    int   active;
} Obstacle;

typedef struct {
    float x;
    float y;
    float w;
    float h;
    float speed;
} Player;

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    int           running;

    GameState state;

    Player    player;
    Obstacle  obstacles[MAX_OBSTACLES];

    int    score;
    int    highScore;
    float  elapsedTime;      /* seconds since game start (for difficulty) */
    Uint32 lastSpawnTicks;   /* ms timestamp of last obstacle spawn */
    float  spawnIntervalMs;  /* dynamic spawn interval */

    int    leftPressed;
    int    rightPressed;
} Game;

/* -------------------------- Utility Functions ---------------------------- */

static float clampf(float v, float min, float max) {
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

/* Simple AABB collision check */
static int rects_intersect(float x1, float y1, float w1, float h1,
                           float x2, float y2, float w2, float h2) {
    return !(x1 > x2 + w2 ||
             x1 + w1 < x2 ||
             y1 > y2 + h2 ||
             y1 + h1 < y2);
}

/* Random float in [min, max] */
static float rand_range(float min, float max) {
    float t = (float)rand() / (float)RAND_MAX;
    return min + t * (max - min);
}

/* -------------------------- High Score Storage --------------------------- */

static int load_high_score(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        /* no file yet; high score is zero */
        LOG_INFO("No high score file found, starting fresh.");
        return 0;
    }

    int value = 0;
    if (fread(&value, sizeof(int), 1, f) != 1) {
        LOG_ERROR("Failed to read high score file; resetting to zero.");
        value = 0;
    }

    fclose(f);
    return value;
}

static void save_high_score(const char *path, int score) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        LOG_ERROR("Failed to open high score file for writing.");
        return;
    }

    if (fwrite(&score, sizeof(int), 1, f) != 1) {
        LOG_ERROR("Failed to write high score to file.");
    }

    fclose(f);
}

/* ---------------------------- Game Setup --------------------------------- */

static void reset_obstacles(Game *game) {
    for (int i = 0; i < MAX_OBSTACLES; ++i) {
        game->obstacles[i].active = 0;
    }
}

/* Initialize player in the center-bottom of the screen */
static void init_player(Game *game) {
    game->player.w = PLAYER_WIDTH;
    game->player.h = PLAYER_HEIGHT;
    game->player.x = (WINDOW_WIDTH - PLAYER_WIDTH) / 2.0f;
    game->player.y = WINDOW_HEIGHT - PLAYER_HEIGHT - 40.0f;
    game->player.speed = PLAYER_SPEED;
}

/* Reset the gameplay values when starting a new run */
static void reset_gameplay(Game *game) {
    game->score        = 0;
    game->elapsedTime  = 0.0f;
    game->spawnIntervalMs = OBSTACLE_BASE_INTERVAL;
    game->lastSpawnTicks  = SDL_GetTicks();

    init_player(game);
    reset_obstacles(game);
}

/* Initialize SDL, window, renderer, etc. */
static int init_sdl(Game *game) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return 0;
    }

    game->window = SDL_CreateWindow(
        "Endless Dodge",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    if (!game->window) {
        LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 0;
    }

    game->renderer = SDL_CreateRenderer(
        game->window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!game->renderer) {
        LOG_ERROR("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(game->window);
        SDL_Quit();
        return 0;
    }

    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_BLEND);

    return 1;
}

static void shutdown_sdl(Game *game) {
    if (game->renderer) {
        SDL_DestroyRenderer(game->renderer);
    }
    if (game->window) {
        SDL_DestroyWindow(game->window);
    }
    SDL_Quit();
}

/* Initialize entire game structure */
static int init_game(Game *game) {
    memset(game, 0, sizeof(Game));

    if (!init_sdl(game)) {
        return 0;
    }

    /* Seed RNG for obstacle randomization */
    srand((unsigned int)time(NULL));

    game->running = 1;
    game->state   = GAME_STATE_MENU;
    game->leftPressed  = 0;
    game->rightPressed = 0;

    game->highScore = load_high_score(HIGHSCORE_FILE);

    reset_gameplay(game);

    LOG_INFO("Game initialized. High score: %d", game->highScore);
    return 1;
}

/* --------------------------- Obstacle Logic ------------------------------ */

static void spawn_obstacle(Game *game) {
    /* Find an inactive obstacle slot */
    int idx = -1;
    for (int i = 0; i < MAX_OBSTACLES; ++i) {
        if (!game->obstacles[i].active) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        /* No space; skip this spawn */
        return;
    }

    Obstacle *o = &game->obstacles[idx];
    o->w = rand_range(OBSTACLE_MIN_WIDTH, OBSTACLE_MAX_WIDTH);
    o->h = OBSTACLE_HEIGHT;

    /* Keep the obstacle fully inside the screen horizontally */
    float maxX = (float)WINDOW_WIDTH - o->w;
    o->x = rand_range(0.0f, maxX);
    o->y = -o->h;  /* start above screen */

    float speedBoost = OBSTACLE_SPEED_INCREMENT * game->elapsedTime * OBSTACLE_BASE_SPEED;
    o->speed = OBSTACLE_BASE_SPEED + speedBoost;

    o->active = 1;
    game->lastSpawnTicks = SDL_GetTicks();

    /* Gradually reduce spawn interval, down to OBSTACLE_MIN_INTERVAL */
    game->spawnIntervalMs *= OBSTACLE_INTERVAL_DECAY;
    if (game->spawnIntervalMs < OBSTACLE_MIN_INTERVAL) {
        game->spawnIntervalMs = OBSTACLE_MIN_INTERVAL;
    }
}

/* Update all active obstacles */
static void update_obstacles(Game *game, float dt) {
    for (int i = 0; i < MAX_OBSTACLES; ++i) {
        Obstacle *o = &game->obstacles[i];
        if (!o->active) {
            continue;
        }

        o->y += o->speed * dt;

        /* Deactivate if off-screen */
        if (o->y > WINDOW_HEIGHT) {
            o->active = 0;
            /* Reward dodging by slightly increasing score */
            game->score += 10;
        }
    }
}

/* Check if any obstacle hits the player */
static int check_collisions(Game *game) {
    const Player *p = &game->player;

    for (int i = 0; i < MAX_OBSTACLES; ++i) {
        Obstacle *o = &game->obstacles[i];
        if (!o->active) {
            continue;
        }

        if (rects_intersect(p->x, p->y, p->w, p->h,
                            o->x, o->y, o->w, o->h)) {
            return 1;
        }
    }

    return 0;
}

/* ---------------------------- Input Handling ----------------------------- */

static void handle_key_down(Game *game, SDL_Keycode key) {
    switch (key) {
        case SDLK_a:
        case SDLK_LEFT:
            game->leftPressed = 1;
            break;
        case SDLK_d:
        case SDLK_RIGHT:
            game->rightPressed = 1;
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (game->state == GAME_STATE_MENU ||
                game->state == GAME_STATE_GAME_OVER) {
                reset_gameplay(game);
                game->state = GAME_STATE_PLAYING;
            }
            break;
        case SDLK_p:
            if (game->state == GAME_STATE_PLAYING) {
                game->state = GAME_STATE_PAUSED;
            } else if (game->state == GAME_STATE_PAUSED) {
                game->state = GAME_STATE_PLAYING;
            }
            break;
        case SDLK_ESCAPE:
            game->running = 0;
            break;
        default:
            break;
    }
}

static void handle_key_up(Game *game, SDL_Keycode key) {
    switch (key) {
        case SDLK_a:
        case SDLK_LEFT:
            game->leftPressed = 0;
            break;
        case SDLK_d:
        case SDLK_RIGHT:
            game->rightPressed = 0;
            break;
        default:
            break;
    }
}

static void process_events(Game *game) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                game->running = 0;
                break;
            case SDL_KEYDOWN:
                if (!e.key.repeat) {
                    handle_key_down(game, e.key.keysym.sym);
                }
                break;
            case SDL_KEYUP:
                handle_key_up(game, e.key.keysym.sym);
                break;
            default:
                break;
        }
    }
}

/* ----------------------------- Game Update ------------------------------- */

static void update_player(Game *game, float dt) {
    float dir = 0.0f;
    if (game->leftPressed) {
        dir -= 1.0f;
    }
    if (game->rightPressed) {
        dir += 1.0f;
    }

    game->player.x += dir * game->player.speed * dt;

    /* Clamp inside screen */
    game->player.x = clampf(
        game->player.x,
        0.0f,
        (float)WINDOW_WIDTH - game->player.w
    );
}

static void update_window_title(Game *game) {
    const char *stateStr = NULL;
    switch (game->state) {
        case GAME_STATE_MENU:
            stateStr = "MENU";
            break;
        case GAME_STATE_PLAYING:
            stateStr = "PLAYING";
            break;
        case GAME_STATE_PAUSED:
            stateStr = "PAUSED";
            break;
        case GAME_STATE_GAME_OVER:
            stateStr = "GAME OVER";
            break;
        default:
            stateStr = "UNKNOWN";
            break;
    }

    char title[128];
    snprintf(
        title,
        sizeof(title),
        "Endless Dodge - Score: %d  High: %d  [%s]",
        game->score,
        game->highScore,
        stateStr
    );
    SDL_SetWindowTitle(game->window, title);
}

static void update_game(Game *game, float dt) {
    if (game->state != GAME_STATE_PLAYING) {
        return;
    }

    game->elapsedTime += dt;

    /* Score increases gradually over time */
    game->score += (int)(dt * 20.0f); /* 20 points per second */

    update_player(game, dt);
    update_obstacles(game, dt);

    /* Spawn new obstacles based on dynamic interval */
    Uint32 now = SDL_GetTicks();
    if ((float)(now - game->lastSpawnTicks) >= game->spawnIntervalMs) {
        spawn_obstacle(game);
    }

    /* Check for game over */
    if (check_collisions(game)) {
        game->state = GAME_STATE_GAME_OVER;
        if (game->score > game->highScore) {
            game->highScore = game->score;
            save_high_score(HIGHSCORE_FILE, game->highScore);
            LOG_INFO("New high score: %d", game->highScore);
        }
    }
}

/* ---------------------------- Rendering ---------------------------------- */

static void draw_filled_rect(SDL_Renderer *renderer,
                             float x, float y, float w, float h,
                             Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    SDL_Rect rect;
    rect.x = (int)roundf(x);
    rect.y = (int)roundf(y);
    rect.w = (int)roundf(w);
    rect.h = (int)roundf(h);

    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_RenderFillRect(renderer, &rect);
}

static void render_game(const Game *game) {
    SDL_Renderer *renderer = game->renderer;

    /* Background */
    SDL_SetRenderDrawColor(renderer,
                           BACKGROUND_COLOR_R,
                           BACKGROUND_COLOR_G,
                           BACKGROUND_COLOR_B,
                           255);
    SDL_RenderClear(renderer);

    /* Player */
    draw_filled_rect(renderer,
                     game->player.x,
                     game->player.y,
                     game->player.w,
                     game->player.h,
                     PLAYER_COLOR_R,
                     PLAYER_COLOR_G,
                     PLAYER_COLOR_B,
                     255);

    /* Obstacles */
    for (int i = 0; i < MAX_OBSTACLES; ++i) {
        const Obstacle *o = &game->obstacles[i];
        if (!o->active) continue;

        draw_filled_rect(renderer,
                         o->x,
                         o->y,
                         o->w,
                         o->h,
                         OBSTACLE_COLOR_R,
                         OBSTACLE_COLOR_G,
                         OBSTACLE_COLOR_B,
                         255);
    }

    /* State overlays (semi-transparent tint) */
    if (game->state == GAME_STATE_MENU) {
        draw_filled_rect(renderer,
                         0, 0, WINDOW_WIDTH, WINDOW_HEIGHT,
                         0, 0, 0, MENU_TINT_ALPHA);
    } else if (game->state == GAME_STATE_PAUSED) {
        draw_filled_rect(renderer,
                         0, 0, WINDOW_WIDTH, WINDOW_HEIGHT,
                         0, 0, 0, PAUSE_TINT_ALPHA);
    } else if (game->state == GAME_STATE_GAME_OVER) {
        draw_filled_rect(renderer,
                         0, 0, WINDOW_WIDTH, WINDOW_HEIGHT,
                         120, 0, 0, GAME_OVER_TINT_ALPHA);
    }

    SDL_RenderPresent(renderer);
}

/* ------------------------------ Main Loop -------------------------------- */

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    Game game;
    if (!init_game(&game)) {
        return EXIT_FAILURE;
    }

    Uint32 lastTicks = SDL_GetTicks();

    while (game.running) {
    Uint32 currentTicks = SDL_GetTicks();
    Uint32 deltaMs = currentTicks - lastTicks;
    lastTicks = currentTicks;
    float dt = deltaMs / 1000.0f;

    if (dt > 0.1f) dt = 0.1f;

    process_events(&game);
    update_game(&game, dt);
    update_window_title(&game);
    render_game(&game);
}


    shutdown_sdl(&game);
    return EXIT_SUCCESS;
}
