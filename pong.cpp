/* Components of Pong:
 * - Player paddle ('Up' key to move up, 'Down' key to move down)
 * - CPU paddle
 * - Ball which ping-pongs between paddles and walls
 * - Middle partition
 * - Score
 */

#define SDL_MAIN_USE_CALLBACKS 1    /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_AudioDeviceID audio_device = 0;
static Uint64 last_time = 0;    // The time deltatime before the current frame

static const int WINDOW_WIDTH = 640;
static const int WINDOW_HEIGHT = 480;

enum Directions {UP = 1, DOWN = -1, ZERO = 0};
static Directions s_direction_player = ZERO;
static Directions s_direction_cpu = UP;
static Directions s_direction_ball_x = UP;
static Directions s_direction_ball_y = DOWN;

// Place player and CPU paddles little below upper wall
static float s_position_player_y = 100;
static float s_position_cpu_y = 100;

// Place ball in middle of screen
static float s_position_ball_x = WINDOW_WIDTH/2;
static float s_position_ball_y = WINDOW_HEIGHT/2;

// Set a random x (and by extension random y) between 0.0 and 1.0
static float s_component_ball_x = SDL_randf();

static int s_score_player = 0;
static int s_score_cpu = 0;

/* things that are playing sound (the audiostream itself, plus the original data, so we can refill to loop. */
typedef struct Sound {
    Uint8 *wav_data;
    Uint32 wav_data_len;
    SDL_AudioStream *stream;
} Sound;

static Sound sounds[2];

static bool init_sound(const char *fname, Sound *sound)
{
    bool retval = false;
    SDL_AudioSpec spec;
    char *wav_path = NULL;

    /* Load the .wav files from wherever the app is being run from. */
    SDL_asprintf(&wav_path, "%s%s", SDL_GetBasePath(), fname);  /* allocate a string of the full file path */
    if (!SDL_LoadWAV(wav_path, &spec, &sound->wav_data, &sound->wav_data_len)) {
        SDL_Log("Couldn't load .wav file: %s", SDL_GetError());
        return false;
    }

    /* Create an audio stream. Set the source format to the wav's format (what
       we'll input), leave the dest format NULL here (it'll change to what the
       device wants once we bind it). */
    sound->stream = SDL_CreateAudioStream(&spec, NULL);
    if (!sound->stream) {
        SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
    } else if (!SDL_BindAudioStream(audio_device, sound->stream)) {  /* once bound, it'll start playing when there is data available! */
        SDL_Log("Failed to bind '%s' stream to device: %s", fname, SDL_GetError());
    } else {
        retval = true;  /* success! */
    }

    SDL_free(wav_path);  /* done with this string. */
    return retval;
}

/* This function runs once at startup */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    SDL_SetAppMetadata("Pong", "1.0", "gunz-sdl3-pong");

    // We will use this renderer to draw into this window every frame
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("pong", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    /* Set a device-independent resolution and presentation mode for rendering. */
    SDL_SetRenderLogicalPresentation(renderer, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

    /* open the default audio device in whatever format it prefers; our audio streams will adjust to it. */
    audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (audio_device == 0) {
        SDL_Log("Couldn't open audio device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!init_sound("bgm.wav", &sounds[0])) {
        return SDL_APP_FAILURE;
    } else if (!init_sound("score.wav", &sounds[1])) {
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    // For when key is pressed
    if (event->type == SDL_EVENT_KEY_DOWN) {
        // Up key pressed
        if (event->key.scancode == SDL_SCANCODE_UP) {
            s_direction_player = UP;
        // Down key pressed
        } else if (event->key.scancode == SDL_SCANCODE_DOWN) {
            s_direction_player = DOWN;
        }
    }

    // For when key is released
    if (event->type == SDL_EVENT_KEY_UP) {
        // Up key released
        if (event->key.scancode == SDL_SCANCODE_UP) {
            s_direction_player = ZERO;
        // Down key released
        } else if (event->key.scancode == SDL_SCANCODE_DOWN) {
            s_direction_player = ZERO;
        }
    }

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    // int i;

    // for (i = 0; i < SDL_arraysize(sounds); i++) {
    //     /* If less than a full copy of the audio is queued for playback, put another copy in there.
    //        This is overkill, but easy when lots of RAM is cheap. One could be more careful and
    //        queue less at a time, as long as the stream doesn't run dry.  */
    //     if (SDL_GetAudioStreamQueued(sounds[i].stream) < ((int) sounds[i].wav_data_len)) {
    //         SDL_PutAudioStreamData(sounds[i].stream, sounds[i].wav_data, (int) sounds[i].wav_data_len);
    //     }
    // }

    if (SDL_GetAudioStreamQueued(sounds[0].stream) < ((int) sounds[0].wav_data_len)) {
        SDL_PutAudioStreamData(sounds[0].stream, sounds[0].wav_data, (int) sounds[0].wav_data_len);
    }
    /* Get the number of milliseconds that have elapsed since the SDL library initialization */
    const Uint64 now = SDL_GetTicks();

    const float deltatime = ((float) (now - last_time)) / 1000.0f;

    SDL_FRect paddle_player;
    SDL_FRect paddle_cpu;
    SDL_FRect ball;

    /* as you can see from this, rendering draws over whatever was drawn before it. */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);  /* black, full alpha */
    SDL_RenderClear(renderer);  /* start with a blank canvas. */

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);  /* white, full alpha */

    // Left edge of screen
    paddle_player.x = 10;
    paddle_player.y = s_position_player_y;
    paddle_player.w = 10;
    paddle_player.h = 60;


    // Right edge of screen
    paddle_cpu.x = WINDOW_WIDTH - 20;
    paddle_cpu.y = s_position_cpu_y;
    paddle_cpu.w = 10;
    paddle_cpu.h = 60;

    // Top and bottom of each paddles to assist in ball collisions
    float paddle_player_top = paddle_player.y + 4;
    float paddle_player_bottom = paddle_player_top + paddle_player.h - 4;
    float paddle_cpu_top = paddle_cpu.y + 4;
    float paddle_cpu_bottom = paddle_cpu_top + paddle_cpu.h - 4;

    // Bound player to screen if exceeding window limit, else move player paddle
    if (s_position_player_y < 0) {
        s_position_player_y = 0;
    } else if (s_position_player_y > WINDOW_HEIGHT-paddle_player.h) {
        s_position_player_y = WINDOW_HEIGHT-paddle_player.h;
    } else {
        s_position_player_y -= 300*s_direction_player*deltatime;
    }

    // Move CPU vertically in ping-pong motion
    s_position_cpu_y -= 250*s_direction_cpu*deltatime;
    if (s_position_cpu_y < 0) {
        s_direction_cpu = DOWN;
    }
    
    if (s_position_cpu_y > WINDOW_HEIGHT-paddle_cpu.h) {
        s_direction_cpu = UP;
    }

    ball.x = s_position_ball_x;
    ball.y = s_position_ball_y;
    ball.w = 10;
    ball.h = 10;

    // Reset x component to prevent ball getting stuck in one dimension
    if (s_component_ball_x < 0.3 or s_component_ball_x > 0.7) {
        s_component_ball_x = SDL_randf();
    }
    float s_component_ball_y = 1 - s_component_ball_x;

    s_position_ball_x -= 400*s_direction_ball_x*s_component_ball_x*deltatime;
    s_position_ball_y -= 400*s_direction_ball_y*s_component_ball_y*deltatime;

    // Handling ball collision with player
    if (s_position_ball_x <= paddle_player.x + paddle_player.w) {
        if (s_position_ball_y > paddle_player_top && s_position_ball_y < paddle_player_bottom) {
            s_direction_ball_x = DOWN;
            // Change direction of ball to direction of paddle
            if (s_direction_player == UP) {
                s_direction_ball_y = UP;
            } else if (s_direction_player == DOWN) {
                s_direction_ball_y = DOWN;
            }
        }
    }

    // Handling ball collision with CPU
    if (s_position_ball_x >= paddle_cpu.x - paddle_cpu.w) {
        if (s_position_ball_y > paddle_cpu_top && s_position_ball_y < paddle_cpu_bottom) {
            s_direction_ball_x = UP;
            // Change direction of ball to direction of paddle
            if (s_direction_cpu == UP) {
                s_direction_ball_y = UP;
            } else if (s_direction_cpu == DOWN) {
                s_direction_ball_y = DOWN;
            }
        }
    }

    // Handling collision with top and bottom wall respectively
    if (s_position_ball_y <= 0) {
        s_direction_ball_y = DOWN;
    }
    if (s_position_ball_y >= WINDOW_HEIGHT) {
        s_direction_ball_y = UP;
    }

    // When ball crosses left or right side of screen and someone scores
    if (s_position_ball_x < -20) {
        s_position_ball_x = WINDOW_WIDTH/2;
        s_position_ball_y = WINDOW_HEIGHT/2;
        s_component_ball_x = SDL_randf();
        s_direction_ball_x = UP;
        SDL_PutAudioStreamData(sounds[1].stream, sounds[1].wav_data, (int) sounds[1].wav_data_len);
        s_score_cpu++;
    }

    if (s_position_ball_x > WINDOW_WIDTH + 20) {
        s_position_ball_x = WINDOW_WIDTH/2;
        s_position_ball_y = WINDOW_HEIGHT/2;
        s_component_ball_x = SDL_randf();
        s_direction_ball_x = UP;
        SDL_PutAudioStreamData(sounds[1].stream, sounds[1].wav_data, (int) sounds[1].wav_data_len);
        s_score_player++;
    }

    // Rendering paddles and ball
    SDL_RenderFillRect(renderer, &paddle_player);
    SDL_RenderFillRect(renderer, &paddle_cpu);
    SDL_RenderFillRect(renderer, &ball);

    // Display scores
    SDL_SetRenderScale(renderer, 1.0f, 1.0f);
    SDL_RenderDebugTextFormat(renderer, WINDOW_WIDTH/4, 100, "%d", s_score_player);
    SDL_RenderDebugTextFormat(renderer, 3*WINDOW_WIDTH/4, 100, "%d", s_score_cpu);

    // Middle partition
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
    int i;

    for (i = 0; i < SDL_arraysize(sounds); i++) {
        /* If less than a full copy of the audio is queued for playback, put another copy in there.
           This is overkill, but easy when lots of RAM is cheap. One could be more careful and
           queue less at a time, as long as the stream doesn't run dry.  */
        if (SDL_GetAudioStreamQueued(sounds[i].stream) < ((int) sounds[i].wav_data_len)) {
            SDL_PutAudioStreamData(sounds[i].stream, sounds[i].wav_data, (int) sounds[i].wav_data_len);
        }
    }
    /* SDL will clean up the window/renderer for us. */
}