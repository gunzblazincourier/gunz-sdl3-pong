#define SDL_MAIN_USE_CALLBACKS 1    /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <iostream>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static Uint64 last_time = 0;

static const int WINDOW_WIDTH = 640;
static const int WINDOW_HEIGHT = 480;

enum Directions {UP = 1, DOWN = -1, ZERO = 0};
static Directions dir = ZERO;
static Directions dir2 = UP;
static Directions dirball_x = UP;
static Directions dirball_y = DOWN;

static float s_player_y_coordinate = 100;
static float s_cpu_y_coordinate = 100;
static float s_ball_x_coordinate = WINDOW_WIDTH/2;
static float s_ball_y_coordinate = WINDOW_HEIGHT/2;
static float ballspeedratio_x = SDL_randf();

static int score_player = 0;
static int score_cpu = 0;

void flip_direction(Directions& d) {
    if (d == UP) {
        d = DOWN;
    } else if (d == DOWN) {
        d = UP;
    }
}

/* We will use this renderer to draw into this window every frame. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    SDL_SetAppMetadata("Pong", "1.0", "gunz-sdl3-pong");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("pong", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    /* Set a device-independent resolution and presentation mode for rendering. */
    SDL_SetRenderLogicalPresentation(renderer, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_KEY_DOWN) {
        if (event->key.scancode == SDL_SCANCODE_UP) {
            dir = UP;
        } else if (event->key.scancode == SDL_SCANCODE_DOWN) {
            dir = DOWN;
        }
        // else {
        //     dir = ZERO;
        // }
    }

    if (event->type == SDL_EVENT_KEY_UP) {
        if (event->key.scancode == SDL_SCANCODE_UP) {
            dir = ZERO;
        } else if (event->key.scancode == SDL_SCANCODE_DOWN) {
            dir = ZERO;
        }
        // else {
        //     dir = ZERO;
        // }
    }

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    std::cout << ballspeedratio_x << std::endl;
    const Uint64 now = SDL_GetTicks();
    const float elapsed = ((float) (now - last_time)) / 1000.0f;

    SDL_FRect rect;
    SDL_FRect rect2;
    SDL_FRect rect3;
    // std::cout << dir << std::endl;
    // std::cout << std::endl;

    /* as you can see from this, rendering draws over whatever was drawn before it. */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);  /* black, full alpha */
    SDL_RenderClear(renderer);  /* start with a blank canvas. */

    /* draw player 1 rectangle. */
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);  /* white, full alpha */
    rect.x = 20;
    rect.y = s_player_y_coordinate;
    rect.w = 10;
    rect.h = 60;

    float rect_start = rect.y + 4;
    float rect_end = rect_start + rect.h - 4;
    // std::cout << rect.y << std::endl;

    // if (dir == UP) {
    //     rect.y += 10;
    // } else if (dir == DOWN) {
    //     rect.y -= 10;
    // }

    if (s_player_y_coordinate < 0) {
        s_player_y_coordinate = 0;
    } else if (s_player_y_coordinate > WINDOW_HEIGHT-rect.h) {
        s_player_y_coordinate = WINDOW_HEIGHT-rect.h;
    } else {
        s_player_y_coordinate -= 300*dir*elapsed;
    }
    SDL_RenderFillRect(renderer, &rect);

    /* draw player 2 rectangle. */
    rect2.x = WINDOW_WIDTH - 20;
    rect2.y = s_cpu_y_coordinate;
    rect2.w = 10;
    rect2.h = 60;

    float rect2_start = rect2.y + 4;
    float rect2_end = rect2_start + rect2.h - 4;

    s_cpu_y_coordinate -= 250*dir2*elapsed;
    if (s_cpu_y_coordinate < 0) {
        dir2 = DOWN;
    }
    
    if (s_cpu_y_coordinate > WINDOW_HEIGHT-rect2.h) {
        dir2 = UP;
    }
    SDL_RenderFillRect(renderer, &rect2);

    /* Draw ball */
    rect3.x = s_ball_x_coordinate;
    rect3.y = s_ball_y_coordinate;
    rect3.w = 10;
    rect3.h = 10;

    if (ballspeedratio_x < 0.3 or ballspeedratio_x > 0.7) {
        ballspeedratio_x = SDL_randf();
    }
    float ballspeedratio_y = 1 - ballspeedratio_x;
    s_ball_x_coordinate -= 400*dirball_x*ballspeedratio_x*elapsed;
    s_ball_y_coordinate -= 400*dirball_y*ballspeedratio_y*elapsed;
    // std::cout << dirball_x << std::endl;
    // std::cout << dirball_y << std::endl;
    // std::cout << std::endl;

    if (s_ball_x_coordinate <= rect.x + rect.w) {
        if (s_ball_y_coordinate > rect_start && s_ball_y_coordinate < rect_end) {
            // dirball_x = DOWN;
            // dirball_y = UP;
            dirball_x = DOWN;
        }
    }

    if (s_ball_x_coordinate >= rect2.x - rect2.w) {
        if (s_ball_y_coordinate > rect2_start && s_ball_y_coordinate < rect2_end) {
            // dirball_x = DOWN;
            // dirball_y = UP;
            dirball_x = UP;
        }
    }

    if (s_ball_y_coordinate <= 0) {
        flip_direction(dirball_y);
    }

    if (s_ball_y_coordinate >= WINDOW_HEIGHT) {
        flip_direction(dirball_y);
    }

    if (s_ball_x_coordinate < -20) {
        s_ball_x_coordinate = WINDOW_WIDTH/2;
        s_ball_y_coordinate = WINDOW_HEIGHT/2;
        ballspeedratio_x = SDL_randf();
        dirball_x = UP;
        score_cpu++;
    }

    if (s_ball_x_coordinate > WINDOW_WIDTH + 20) {
        s_ball_x_coordinate = WINDOW_WIDTH/2;
        s_ball_y_coordinate = WINDOW_HEIGHT/2;
        ballspeedratio_x = SDL_randf();
        dirball_x = UP;
        score_player++;
    }

    // if (s_cpu_y_coordinate < 0) {
    //     dir2 = DOWN;
    // }
    
    // if (s_cpu_y_coordinate > WINDOW_HEIGHT-rect2.h) {
    //     dir2 = UP;
    // }
    SDL_RenderFillRect(renderer, &rect3);

    // /* draw a unfilled rectangle in-set a little bit. */
    // SDL_SetRenderDrawColor(renderer, 0, 255, 0, SDL_ALPHA_OPAQUE);  /* green, full alpha */
    // rect.x += 30;
    // rect.y += 30;
    // rect.w -= 60;
    // rect.h -= 60;
    // SDL_RenderRect(renderer, &rect);



    // if (s_ball_x_coordinate < 0) {
    //     score_player++;
    // }

    // if (s_ball_x_coordinate > WINDOW_WIDTH) {
    //     score_cpu++;
    // }

    SDL_SetRenderScale(renderer, 1.0f, 1.0f);
    SDL_RenderDebugTextFormat(renderer, WINDOW_WIDTH/4, 100, "%d", score_player);
    SDL_RenderDebugTextFormat(renderer, 3*WINDOW_WIDTH/4, 100, "%d", score_cpu);

    for (int i = 0; i < WINDOW_HEIGHT; i += 10) {
        SDL_RenderPoint(renderer, WINDOW_WIDTH/2, i);
    }

    last_time = now;
    SDL_RenderPresent(renderer);  /* put it all on the screen! */
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    /* SDL will clean up the window/renderer for us. */
}