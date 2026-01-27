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
#include <iostream>

// Which window is being displayed (eg main menu or options menu)
enum Window {MAIN = 0, CONFIG = 1, GAME = 2};

// Choices in main menu
enum Menu {PLAY = 0, OPTIONS = 1, QUIT = 2};

// Choices in option menu
enum Option {RESOLUTION = 0, FULLSCREEN = 1, AUDIO = 2, BALL_SPEED = 3, PADDLE_SPEED = 4, APPLY = 5, BACK = 6};

// List of available resolutions
enum Resolution {VGA = 0, SVGA = 1, HD = 2, XGA = 3, WXGA = 4, SXGA = 5, FHD = 6, QHD = 7};

// Ball speed levels
enum BallSpeed {B_LOW = 0, B_MEDIUM = 1, B_HIGH = 2};

// Paddle speed levels
enum PaddleSpeed {P_LOW = 0, P_MEDIUM = 1, P_HIGH = 2};

// Direction that ball and paddle can go
enum Directions {UP = 1, DOWN = -1, ZERO = 0};

// Corresponding enum variables
static Window window_choice = MAIN;
static Menu menu_choice = PLAY;
static Option options_choice = RESOLUTION;
static Resolution resolution_choice = VGA;
static BallSpeed ball_speed_difficulty = B_MEDIUM;
static PaddleSpeed paddle_speed_difficulty = P_MEDIUM;
static Directions s_direction_player = ZERO;
static Directions s_direction_cpu = UP;
static Directions s_direction_ball_x = UP;
static Directions s_direction_ball_y = DOWN;

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_AudioDeviceID audio_device = 0;

// Current resolution
static int WINDOW_WIDTH = 640;
static int WINDOW_HEIGHT = 480;

// Actual game's resolution
static const int GAME_WIDTH = 640;
static const int GAME_HEIGHT = 480;

static bool is_fullscreen = true;
static bool is_audio_enabled = true;

// Delay between selecting PLAY and loading the actual game; needed to play SFX and flash option
static int play_timer = 0;

// Default for both is MEDIUM
static float ball_speed_multiplier = 0.5;
static float paddle_speed_multiplier = 0.5;

// Place player and CPU paddles little below upper wall
static float s_position_player_y = 100;
static float s_position_cpu_y = 100;

// Place ball in middle of screen
static float s_position_ball_x = GAME_WIDTH/2;
static float s_position_ball_y = GAME_HEIGHT/2;

// Set a random x (and by extension random y) between 0.0 and 1.0
static float s_component_ball_x = SDL_randf();

static int s_score_player = 0;
static int s_score_cpu = 0;

// Get the number of milliseconds elapsed in previous frame
static Uint64 last_time = 0;

/* things that are playing sound (the audiostream itself, plus the original data, so we can refill to loop. */
typedef struct Sound {
    Uint8 *wav_data;
    Uint32 wav_data_len;
    SDL_AudioStream *stream;
} Sound;

static Sound sounds[4];

static bool init_sound(const char *fname, Sound *sound) {
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
    SDL_SetAppMetadata("Pong", "0.8", "gunz-sdl3-pong");

    // We will use this renderer to draw into this window every frame
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("pong", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_FULLSCREEN, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    /* Set a device-independent resolution and presentation mode for rendering. */
    SDL_SetRenderLogicalPresentation(renderer, GAME_WIDTH, GAME_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    /* open the default audio device in whatever format it prefers; our audio streams will adjust to it. */
    audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (audio_device == 0) {
        SDL_Log("Couldn't open audio device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Used to randomly select background music out of 4 tracks
    int random_music_number = SDL_rand(100);

    if (random_music_number < 25) {
        if (!init_sound("bgm.wav", &sounds[0])) {
            return SDL_APP_FAILURE;
        }
    } else if (random_music_number < 50) {
        if (!init_sound("bgm2.wav", &sounds[0])) {
            return SDL_APP_FAILURE;
        }
    } else if (random_music_number < 75) {
        if (!init_sound("bgm3.wav", &sounds[0])) {
            return SDL_APP_FAILURE;
        }
    } else {
        if (!init_sound("bgm4.wav", &sounds[0])) {
            return SDL_APP_FAILURE;
        }
    }

    if (!init_sound("score.wav", &sounds[1])) {
        return SDL_APP_FAILURE;
    }
    if (!init_sound("menu_select.wav", &sounds[2])) {
        return SDL_APP_FAILURE;
    }
    if (!init_sound("start.wav", &sounds[3])) {
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    if (window_choice == GAME) {
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

    } else if (window_choice == MAIN) {
        if (event->type == SDL_EVENT_KEY_DOWN && event->key.repeat == false && play_timer == 0) {
            if (event->key.scancode == SDL_SCANCODE_DOWN) {
                SDL_ClearAudioStream(sounds[2].stream);
                SDL_PutAudioStreamData(sounds[2].stream, sounds[2].wav_data, (int) sounds[2].wav_data_len);
                menu_choice = static_cast<Menu>((menu_choice + 1) % (QUIT+1));
            }

            if (event->key.scancode == SDL_SCANCODE_UP) {
                SDL_ClearAudioStream(sounds[2].stream);
                SDL_PutAudioStreamData(sounds[2].stream, sounds[2].wav_data, (int) sounds[2].wav_data_len);
                if (menu_choice == 0) {
                    menu_choice = QUIT;
                } else {
                    menu_choice = static_cast<Menu>((menu_choice - 1) % (QUIT+1));
                }
            }
            
            if (event->key.scancode == SDL_SCANCODE_RETURN) {
                if (menu_choice == PLAY) {
                    SDL_ClearAudioStream(sounds[3].stream);
                    SDL_PutAudioStreamData(sounds[3].stream, sounds[3].wav_data, (int) sounds[3].wav_data_len);
                    play_timer = 501760;    // The stream data of sounds[3]
                } else if (menu_choice == OPTIONS) {
                    window_choice = CONFIG;
                } else if (menu_choice == QUIT) {
                    event->type = SDL_EVENT_QUIT;
                }
            }
        }

    } else if (window_choice == CONFIG) {
        if (event->type == SDL_EVENT_KEY_DOWN && event->key.repeat == false) {
            if (event->key.scancode == SDL_SCANCODE_DOWN) {
                SDL_ClearAudioStream(sounds[2].stream);
                SDL_PutAudioStreamData(sounds[2].stream, sounds[2].wav_data, (int) sounds[2].wav_data_len);
                options_choice = static_cast<Option>((options_choice + 1) % (BACK+1));
            }

            if (event->key.scancode == SDL_SCANCODE_UP) {
                SDL_ClearAudioStream(sounds[2].stream);
                SDL_PutAudioStreamData(sounds[2].stream, sounds[2].wav_data, (int) sounds[2].wav_data_len);
                if (options_choice == 0) {
                    options_choice = BACK;
                } else {
                    options_choice = static_cast<Option>((options_choice - 1) % (BACK+1));
                }
            }

            if (options_choice == FULLSCREEN) {
                if (event->key.scancode == SDL_SCANCODE_LEFT && is_fullscreen == false) {
                    is_fullscreen = true;
                } else if (event->key.scancode == SDL_SCANCODE_RIGHT && is_fullscreen == true) {
                    is_fullscreen = false;
                }
            } else if (options_choice == RESOLUTION) {
                if (event->key.scancode == SDL_SCANCODE_LEFT) {
                    if (resolution_choice == 0) {
                        resolution_choice = QHD;
                    } else {
                        resolution_choice = static_cast<Resolution>((resolution_choice - 1) % (QHD+1));
                    }
                } else if (event->key.scancode == SDL_SCANCODE_RIGHT) {
                    resolution_choice = static_cast<Resolution>((resolution_choice + 1) % (QHD+1));
                }
            } else if (options_choice == AUDIO) {
                if (event->key.scancode == SDL_SCANCODE_LEFT && is_audio_enabled == false) {
                    is_audio_enabled = true;
                } else if (event->key.scancode == SDL_SCANCODE_RIGHT && is_audio_enabled == true) {
                    is_audio_enabled = false;
                }
            } else if (options_choice == BALL_SPEED) {
                if (event->key.scancode == SDL_SCANCODE_LEFT) {
                    if (ball_speed_difficulty == 0) {
                        ball_speed_difficulty = B_HIGH;
                    } else {
                        ball_speed_difficulty = static_cast<BallSpeed>((ball_speed_difficulty - 1) % (B_HIGH+1));
                    }
                } else if (event->key.scancode == SDL_SCANCODE_RIGHT) {
                    ball_speed_difficulty = static_cast<BallSpeed>((ball_speed_difficulty + 1) % (B_HIGH+1));
                }
            } else if (options_choice == PADDLE_SPEED) {
                if (event->key.scancode == SDL_SCANCODE_LEFT) {
                    if (paddle_speed_difficulty == 0) {
                        paddle_speed_difficulty = P_HIGH;
                    } else {
                        paddle_speed_difficulty = static_cast<PaddleSpeed>((paddle_speed_difficulty - 1) % (P_HIGH+1));
                    }
                } else if (event->key.scancode == SDL_SCANCODE_RIGHT) {
                    paddle_speed_difficulty = static_cast<PaddleSpeed>((paddle_speed_difficulty + 1) % (P_HIGH+1));
                }
            }
            
            if (event->key.scancode == SDL_SCANCODE_RETURN) {
                if (options_choice == APPLY) {
                    SDL_SetWindowFullscreen(window, is_fullscreen);
                    if (resolution_choice == VGA) {
                        WINDOW_WIDTH = 640;
                        WINDOW_HEIGHT = 480;
                    } else if (resolution_choice == SVGA) {
                        WINDOW_WIDTH = 800;
                        WINDOW_HEIGHT = 600;
                    } else if (resolution_choice == HD) {
                        WINDOW_WIDTH = 1280;
                        WINDOW_HEIGHT = 720;
                    } else if (resolution_choice == XGA) {
                        WINDOW_WIDTH = 1024;
                        WINDOW_HEIGHT = 768;
                    } else if (resolution_choice == WXGA) {
                        WINDOW_WIDTH = 1366;
                        WINDOW_HEIGHT = 768;
                    } else if (resolution_choice == SXGA) {
                        WINDOW_WIDTH = 1280;
                        WINDOW_HEIGHT = 1024;
                    } else if (resolution_choice == FHD) {
                        WINDOW_WIDTH = 1920;
                        WINDOW_HEIGHT = 1080;
                    } else if (resolution_choice == QHD) {
                        WINDOW_WIDTH = 2560;
                        WINDOW_HEIGHT = 1440;
                    }
                    SDL_SetWindowSize(window, WINDOW_WIDTH, WINDOW_HEIGHT);

                    if (is_audio_enabled == false) {
                        SDL_PauseAudioStreamDevice(sounds->stream);
                    } else {
                        SDL_ResumeAudioStreamDevice(sounds->stream);
                    }

                    if (ball_speed_difficulty == B_LOW) {
                        ball_speed_multiplier = 0.3;
                    } else if (ball_speed_difficulty == B_MEDIUM) {
                        ball_speed_multiplier = 0.6;
                    } else if (ball_speed_difficulty == B_HIGH) {
                        ball_speed_multiplier = 1.0;
                    }

                    if (paddle_speed_difficulty == P_LOW) {
                        paddle_speed_multiplier = 0.3;
                    } else if (paddle_speed_difficulty == P_MEDIUM) {
                        paddle_speed_multiplier = 0.6;
                    } else if (paddle_speed_difficulty == P_HIGH) {
                        paddle_speed_multiplier = 1.0;
                    }
                } else if (options_choice == BACK) {
                    window_choice = MAIN;
                }
            }
        }
    }

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate) {
    // Tick play_timer down once it starts
    if (play_timer > 0) {
        play_timer -= 30;
    }

    // When timer starts, perform action when reaches certain threshold
    if (play_timer > 0 && play_timer < 1000) {
        window_choice = GAME;
    }

    /* Get the number of milliseconds that have elapsed since the SDL library initialization */
    const Uint64 now = SDL_GetTicks();
    const float deltatime = ((float) (now - last_time)) / 1000.0f;

    if (window_choice == MAIN) {
        SDL_SetRenderDrawColor(renderer, 0, 32, 63, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);

        SDL_SetRenderScale(renderer, 4.0f, 4.0f);
        SDL_RenderDebugText(renderer, GAME_WIDTH/12, GAME_HEIGHT/15, "PONG");

        SDL_SetRenderScale(renderer, 2.0f, 2.0f);
        if (menu_choice == PLAY) {
            if (play_timer == 0 ||
                    (play_timer > 0 && (now / 250) % 2 == 0)) {
                SDL_SetRenderDrawColor(renderer, 214, 237, 23, SDL_ALPHA_OPAQUE);
                SDL_RenderDebugText(renderer, GAME_WIDTH/5, GAME_HEIGHT/5, "PLAY");
            }
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/5, GAME_HEIGHT/5+15, "OPTIONS");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/5, GAME_HEIGHT/5+30, "QUIT");
        } else if (menu_choice == OPTIONS) {
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/5, GAME_HEIGHT/5, "PLAY");
            SDL_SetRenderDrawColor(renderer, 214, 237, 23, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/5, GAME_HEIGHT/5+15, "OPTIONS");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/5, GAME_HEIGHT/5+30, "QUIT");
        } else if (menu_choice == QUIT) {
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/5, GAME_HEIGHT/5, "PLAY");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/5, GAME_HEIGHT/5+15, "OPTIONS");
            SDL_SetRenderDrawColor(renderer, 214, 237, 23, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/5, GAME_HEIGHT/5+30, "QUIT");
        }

    } else if (window_choice == CONFIG) {
        SDL_FRect fullscreen_choice;
        fullscreen_choice.w = 4;
        fullscreen_choice.h = 4;

        SDL_FRect audio_choice;
        audio_choice.w = 4;
        audio_choice.h = 4;

        SDL_FRect ball_speed_choice;
        ball_speed_choice.w = 4;
        ball_speed_choice.h = 4;

        SDL_FRect paddle_speed_choice;
        paddle_speed_choice.w = 4;
        paddle_speed_choice.h = 4;

        SDL_SetRenderScale(renderer, 2.0f, 2.0f);
        if (options_choice == RESOLUTION) {
            SDL_SetRenderDrawColor(renderer, 214, 237, 23, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5-15, "Resolution");
            SDL_SetRenderDrawColor(renderer, 214, 237, 23, SDL_ALPHA_OPAQUE);
            if (resolution_choice == VGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "640x480");
            } else if (resolution_choice == SVGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "800x600");
            } else if (resolution_choice == HD) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1280x720");
            } else if (resolution_choice == XGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1024x768");
            } else if (resolution_choice == WXGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1366x768");
            } else if (resolution_choice == SXGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "12801024");
            } else if (resolution_choice == FHD) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1920x1080");
            } else if (resolution_choice == QHD) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "2560x1440");
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5, "Fullscreen");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5, "ON");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5, "OFF");
            if (is_fullscreen == true) {
                fullscreen_choice.x = 7*GAME_WIDTH/20 - 5;
                fullscreen_choice.y = GAME_HEIGHT/5;
            } else {
                fullscreen_choice.x = 8*GAME_WIDTH/20 - 5;
                fullscreen_choice.y = GAME_HEIGHT/5;
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+15, "Audio");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5+15, "ON");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5+15, "OFF");
            if (is_audio_enabled == true) {
                audio_choice.x = 7*GAME_WIDTH/20 - 5;
                audio_choice.y = GAME_HEIGHT/5+15;
            } else {
                audio_choice.x = 8*GAME_WIDTH/20 - 5;
                audio_choice.y = GAME_HEIGHT/5+15;
            }
            
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+30, "Ball Speed");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5+30, "LOW");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5+30, "MED");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 9*GAME_WIDTH/20, GAME_HEIGHT/5+30, "HIGH");
            if (ball_speed_difficulty == B_LOW) {
                ball_speed_choice.x = 7*GAME_WIDTH/20 - 5;
                ball_speed_choice.y = GAME_HEIGHT/5+30;
            } else if (ball_speed_difficulty == B_MEDIUM) {
                ball_speed_choice.x = 8*GAME_WIDTH/20 - 5;
                ball_speed_choice.y = GAME_HEIGHT/5+30;
            } else if (ball_speed_difficulty == B_HIGH) {
                ball_speed_choice.x = 9*GAME_WIDTH/20 - 5;
                ball_speed_choice.y = GAME_HEIGHT/5+30;
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+45, "Paddle Speed");
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5+45, "LOW");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5+45, "MED");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 9*GAME_WIDTH/20, GAME_HEIGHT/5+45, "HIGH");
            if (paddle_speed_difficulty == P_LOW) {
                paddle_speed_choice.x = 7*GAME_WIDTH/20 - 5;
                paddle_speed_choice.y = GAME_HEIGHT/5+45;
            } else if (paddle_speed_difficulty == P_MEDIUM) {
                paddle_speed_choice.x = 8*GAME_WIDTH/20 - 5;
                paddle_speed_choice.y = GAME_HEIGHT/5+45;
            } else if (paddle_speed_difficulty == P_HIGH) {
                paddle_speed_choice.x = 9*GAME_WIDTH/20 - 5;
                paddle_speed_choice.y = GAME_HEIGHT/5+45;
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+70, "APPLY");

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+80, "BACK");

        } else if (options_choice == FULLSCREEN) {
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5-15, "Resolution");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            if (resolution_choice == VGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "640x480");
            } else if (resolution_choice == SVGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "800x600");
            } else if (resolution_choice == HD) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1280x720");
            } else if (resolution_choice == XGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1024x768");
            } else if (resolution_choice == WXGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1366x768");
            } else if (resolution_choice == SXGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "12801024");
            } else if (resolution_choice == FHD) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1920x1080");
            } else if (resolution_choice == QHD) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "2560x1440");
            }

            SDL_SetRenderDrawColor(renderer, 214, 237, 23, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5, "Fullscreen");
            SDL_SetRenderDrawColor(renderer, 214, 237, 23, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5, "ON");
            SDL_SetRenderDrawColor(renderer, 214, 237, 23, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5, "OFF");
            if (is_fullscreen == true) {
                fullscreen_choice.x = 7*GAME_WIDTH/20 - 5;
                fullscreen_choice.y = GAME_HEIGHT/5;
            } else {
                fullscreen_choice.x = 8*GAME_WIDTH/20 - 5;
                fullscreen_choice.y = GAME_HEIGHT/5;
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+15, "Audio");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5+15, "ON");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5+15, "OFF");
            if (is_audio_enabled == true) {
                audio_choice.x = 7*GAME_WIDTH/20 - 5;
                audio_choice.y = GAME_HEIGHT/5+15;
            } else {
                audio_choice.x = 8*GAME_WIDTH/20 - 5;
                audio_choice.y = GAME_HEIGHT/5+15;
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+30, "Ball Speed");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5+30, "LOW");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5+30, "MED");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 9*GAME_WIDTH/20, GAME_HEIGHT/5+30, "HIGH");
            if (ball_speed_difficulty == B_LOW) {
                ball_speed_choice.x = 7*GAME_WIDTH/20 - 5;
                ball_speed_choice.y = GAME_HEIGHT/5+30;
            } else if (ball_speed_difficulty == B_MEDIUM) {
                ball_speed_choice.x = 8*GAME_WIDTH/20 - 5;
                ball_speed_choice.y = GAME_HEIGHT/5+30;
            } else if (ball_speed_difficulty == B_HIGH) {
                ball_speed_choice.x = 9*GAME_WIDTH/20 - 5;
                ball_speed_choice.y = GAME_HEIGHT/5+30;
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+45, "Paddle Speed");
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5+45, "LOW");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5+45, "MED");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 9*GAME_WIDTH/20, GAME_HEIGHT/5+45, "HIGH");
            if (paddle_speed_difficulty == P_LOW) {
                paddle_speed_choice.x = 7*GAME_WIDTH/20 - 5;
                paddle_speed_choice.y = GAME_HEIGHT/5+45;
            } else if (paddle_speed_difficulty == P_MEDIUM) {
                paddle_speed_choice.x = 8*GAME_WIDTH/20 - 5;
                paddle_speed_choice.y = GAME_HEIGHT/5+45;
            } else if (paddle_speed_difficulty == P_HIGH) {
                paddle_speed_choice.x = 9*GAME_WIDTH/20 - 5;
                paddle_speed_choice.y = GAME_HEIGHT/5+45;
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+70, "APPLY");

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+80, "BACK");

        } else if (options_choice == AUDIO) {
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5-15, "Resolution");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            if (resolution_choice == VGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "640x480");
            } else if (resolution_choice == SVGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "800x600");
            } else if (resolution_choice == HD) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1280x720");
            } else if (resolution_choice == XGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1024x768");
            } else if (resolution_choice == WXGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1366x768");
            } else if (resolution_choice == SXGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "12801024");
            } else if (resolution_choice == FHD) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1920x1080");
            } else if (resolution_choice == QHD) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "2560x1440");
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5, "Fullscreen");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5, "ON");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5, "OFF");
            if (is_fullscreen == true) {
                fullscreen_choice.x = 7*GAME_WIDTH/20 - 5;
                fullscreen_choice.y = GAME_HEIGHT/5;
            } else {
                fullscreen_choice.x = 8*GAME_WIDTH/20 - 5;
                fullscreen_choice.y = GAME_HEIGHT/5;
            }

            SDL_SetRenderDrawColor(renderer, 214, 237, 23, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+15, "Audio");
            SDL_SetRenderDrawColor(renderer, 214, 237, 23, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5+15, "ON");
            SDL_SetRenderDrawColor(renderer, 214, 237, 23, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5+15, "OFF");
            if (is_audio_enabled == true) {
                audio_choice.x = 7*GAME_WIDTH/20 - 5;
                audio_choice.y = GAME_HEIGHT/5+15;
            } else {
                audio_choice.x = 8*GAME_WIDTH/20 - 5;
                audio_choice.y = GAME_HEIGHT/5+15;
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+30, "Ball Speed");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5+30, "LOW");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5+30, "MED");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 9*GAME_WIDTH/20, GAME_HEIGHT/5+30, "HIGH");
            if (ball_speed_difficulty == B_LOW) {
                ball_speed_choice.x = 7*GAME_WIDTH/20 - 5;
                ball_speed_choice.y = GAME_HEIGHT/5+30;
            } else if (ball_speed_difficulty == B_MEDIUM) {
                ball_speed_choice.x = 8*GAME_WIDTH/20 - 5;
                ball_speed_choice.y = GAME_HEIGHT/5+30;
            } else if (ball_speed_difficulty == B_HIGH) {
                ball_speed_choice.x = 9*GAME_WIDTH/20 - 5;
                ball_speed_choice.y = GAME_HEIGHT/5+30;
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+45, "Paddle Speed");
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5+45, "LOW");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5+45, "MED");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 9*GAME_WIDTH/20, GAME_HEIGHT/5+45, "HIGH");
            if (paddle_speed_difficulty == P_LOW) {
                paddle_speed_choice.x = 7*GAME_WIDTH/20 - 5;
                paddle_speed_choice.y = GAME_HEIGHT/5+45;
            } else if (paddle_speed_difficulty == P_MEDIUM) {
                paddle_speed_choice.x = 8*GAME_WIDTH/20 - 5;
                paddle_speed_choice.y = GAME_HEIGHT/5+45;
            } else if (paddle_speed_difficulty == P_HIGH) {
                paddle_speed_choice.x = 9*GAME_WIDTH/20 - 5;
                paddle_speed_choice.y = GAME_HEIGHT/5+45;
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+70, "APPLY");

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+80, "BACK");

        } else if (options_choice == BALL_SPEED) {
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5-15, "Resolution");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            if (resolution_choice == VGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "640x480");
            } else if (resolution_choice == SVGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "800x600");
            } else if (resolution_choice == HD) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1280x720");
            } else if (resolution_choice == XGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1024x768");
            } else if (resolution_choice == WXGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1366x768");
            } else if (resolution_choice == SXGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "12801024");
            } else if (resolution_choice == FHD) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1920x1080");
            } else if (resolution_choice == QHD) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "2560x1440");
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5, "Fullscreen");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5, "ON");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5, "OFF");
            if (is_fullscreen == true) {
                fullscreen_choice.x = 7*GAME_WIDTH/20 - 5;
                fullscreen_choice.y = GAME_HEIGHT/5;
            } else {
                fullscreen_choice.x = 8*GAME_WIDTH/20 - 5;
                fullscreen_choice.y = GAME_HEIGHT/5;
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+15, "Audio");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5+15, "ON");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5+15, "OFF");
            if (is_audio_enabled == true) {
                audio_choice.x = 7*GAME_WIDTH/20 - 5;
                audio_choice.y = GAME_HEIGHT/5+15;
            } else {
                audio_choice.x = 8*GAME_WIDTH/20 - 5;
                audio_choice.y = GAME_HEIGHT/5+15;
            }

            SDL_SetRenderDrawColor(renderer, 214, 237, 23, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+30, "Ball Speed");
            SDL_SetRenderDrawColor(renderer, 214, 237, 23, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5+30, "LOW");
            SDL_SetRenderDrawColor(renderer, 214, 237, 23, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5+30, "MED");
            SDL_SetRenderDrawColor(renderer, 214, 237, 23, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 9*GAME_WIDTH/20, GAME_HEIGHT/5+30, "HIGH");
            if (ball_speed_difficulty == B_LOW) {
                ball_speed_choice.x = 7*GAME_WIDTH/20 - 5;
                ball_speed_choice.y = GAME_HEIGHT/5+30;
            } else if (ball_speed_difficulty == B_MEDIUM) {
                ball_speed_choice.x = 8*GAME_WIDTH/20 - 5;
                ball_speed_choice.y = GAME_HEIGHT/5+30;
            } else if (ball_speed_difficulty == B_HIGH) {
                ball_speed_choice.x = 9*GAME_WIDTH/20 - 5;
                ball_speed_choice.y = GAME_HEIGHT/5+30;
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+45, "Paddle Speed");
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5+45, "LOW");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5+45, "MED");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 9*GAME_WIDTH/20, GAME_HEIGHT/5+45, "HIGH");
            if (paddle_speed_difficulty == P_LOW) {
                paddle_speed_choice.x = 7*GAME_WIDTH/20 - 5;
                paddle_speed_choice.y = GAME_HEIGHT/5+45;
            } else if (paddle_speed_difficulty == P_MEDIUM) {
                paddle_speed_choice.x = 8*GAME_WIDTH/20 - 5;
                paddle_speed_choice.y = GAME_HEIGHT/5+45;
            } else if (paddle_speed_difficulty == P_HIGH) {
                paddle_speed_choice.x = 9*GAME_WIDTH/20 - 5;
                paddle_speed_choice.y = GAME_HEIGHT/5+45;
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+70, "APPLY");

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+80, "BACK");

        } else if (options_choice == PADDLE_SPEED) {
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5-15, "Resolution");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            if (resolution_choice == VGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "640x480");
            } else if (resolution_choice == SVGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "800x600");
            } else if (resolution_choice == HD) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1280x720");
            } else if (resolution_choice == XGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1024x768");
            } else if (resolution_choice == WXGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1366x768");
            } else if (resolution_choice == SXGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "12801024");
            } else if (resolution_choice == FHD) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1920x1080");
            } else if (resolution_choice == QHD) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "2560x1440");
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5, "Fullscreen");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5, "ON");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5, "OFF");
            if (is_fullscreen == true) {
                fullscreen_choice.x = 7*GAME_WIDTH/20 - 5;
                fullscreen_choice.y = GAME_HEIGHT/5;
            } else {
                fullscreen_choice.x = 8*GAME_WIDTH/20 - 5;
                fullscreen_choice.y = GAME_HEIGHT/5;
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+15, "Audio");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5+15, "ON");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5+15, "OFF");
            if (is_audio_enabled == true) {
                audio_choice.x = 7*GAME_WIDTH/20 - 5;
                audio_choice.y = GAME_HEIGHT/5+15;
            } else {
                audio_choice.x = 8*GAME_WIDTH/20 - 5;
                audio_choice.y = GAME_HEIGHT/5+15;
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+30, "Ball Speed");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5+30, "LOW");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5+30, "MED");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 9*GAME_WIDTH/20, GAME_HEIGHT/5+30, "HIGH");
            if (ball_speed_difficulty == B_LOW) {
                ball_speed_choice.x = 7*GAME_WIDTH/20 - 5;
                ball_speed_choice.y = GAME_HEIGHT/5+30;
            } else if (ball_speed_difficulty == B_MEDIUM) {
                ball_speed_choice.x = 8*GAME_WIDTH/20 - 5;
                ball_speed_choice.y = GAME_HEIGHT/5+30;
            } else if (ball_speed_difficulty == B_HIGH) {
                ball_speed_choice.x = 9*GAME_WIDTH/20 - 5;
                ball_speed_choice.y = GAME_HEIGHT/5+30;
            }

            SDL_SetRenderDrawColor(renderer, 214, 237, 23, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+45, "Paddle Speed");
            SDL_SetRenderDrawColor(renderer, 214, 237, 23, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5+45, "LOW");
            SDL_SetRenderDrawColor(renderer, 214, 237, 23, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5+45, "MED");
            SDL_SetRenderDrawColor(renderer, 214, 237, 23, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 9*GAME_WIDTH/20, GAME_HEIGHT/5+45, "HIGH");
            if (paddle_speed_difficulty == P_LOW) {
                paddle_speed_choice.x = 7*GAME_WIDTH/20 - 5;
                paddle_speed_choice.y = GAME_HEIGHT/5+45;
            } else if (paddle_speed_difficulty == P_MEDIUM) {
                paddle_speed_choice.x = 8*GAME_WIDTH/20 - 5;
                paddle_speed_choice.y = GAME_HEIGHT/5+45;
            } else if (paddle_speed_difficulty == P_HIGH) {
                paddle_speed_choice.x = 9*GAME_WIDTH/20 - 5;
                paddle_speed_choice.y = GAME_HEIGHT/5+45;
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+70, "APPLY");

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+80, "BACK");

        } else if (options_choice == APPLY) {
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5-15, "Resolution");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            if (resolution_choice == VGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "640x480");
            } else if (resolution_choice == SVGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "800x600");
            } else if (resolution_choice == HD) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1280x720");
            } else if (resolution_choice == XGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1024x768");
            } else if (resolution_choice == WXGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1366x768");
            } else if (resolution_choice == SXGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "12801024");
            } else if (resolution_choice == FHD) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1920x1080");
            } else if (resolution_choice == QHD) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "2560x1440");
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5, "Fullscreen");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5, "ON");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5, "OFF");
            if (is_fullscreen == true) {
                fullscreen_choice.x = 7*GAME_WIDTH/20 - 5;
                fullscreen_choice.y = GAME_HEIGHT/5;
            } else {
                fullscreen_choice.x = 8*GAME_WIDTH/20 - 5;
                fullscreen_choice.y = GAME_HEIGHT/5;
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+15, "Audio");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5+15, "ON");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5+15, "OFF");
            if (is_audio_enabled == true) {
                audio_choice.x = 7*GAME_WIDTH/20 - 5;
                audio_choice.y = GAME_HEIGHT/5+15;
            } else {
                audio_choice.x = 8*GAME_WIDTH/20 - 5;
                audio_choice.y = GAME_HEIGHT/5+15;
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+30, "Ball Speed");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5+30, "LOW");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5+30, "MED");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 9*GAME_WIDTH/20, GAME_HEIGHT/5+30, "HIGH");
            if (ball_speed_difficulty == B_LOW) {
                ball_speed_choice.x = 7*GAME_WIDTH/20 - 5;
                ball_speed_choice.y = GAME_HEIGHT/5+30;
            } else if (ball_speed_difficulty == B_MEDIUM) {
                ball_speed_choice.x = 8*GAME_WIDTH/20 - 5;
                ball_speed_choice.y = GAME_HEIGHT/5+30;
            } else if (ball_speed_difficulty == B_HIGH) {
                ball_speed_choice.x = 9*GAME_WIDTH/20 - 5;
                ball_speed_choice.y = GAME_HEIGHT/5+30;
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+45, "Paddle Speed");
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5+45, "LOW");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5+45, "MED");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 9*GAME_WIDTH/20, GAME_HEIGHT/5+45, "HIGH");
            if (paddle_speed_difficulty == P_LOW) {
                paddle_speed_choice.x = 7*GAME_WIDTH/20 - 5;
                paddle_speed_choice.y = GAME_HEIGHT/5+45;
            } else if (paddle_speed_difficulty == P_MEDIUM) {
                paddle_speed_choice.x = 8*GAME_WIDTH/20 - 5;
                paddle_speed_choice.y = GAME_HEIGHT/5+45;
            } else if (paddle_speed_difficulty == P_HIGH) {
                paddle_speed_choice.x = 9*GAME_WIDTH/20 - 5;
                paddle_speed_choice.y = GAME_HEIGHT/5+45;
            }

            SDL_SetRenderDrawColor(renderer, 214, 237, 23, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+70, "APPLY");

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+80, "BACK");

        } else if (options_choice == BACK) {
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5-15, "Resolution");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            if (resolution_choice == VGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "640x480");
            } else if (resolution_choice == SVGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "800x600");
            } else if (resolution_choice == HD) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1280x720");
            } else if (resolution_choice == XGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1024x768");
            } else if (resolution_choice == WXGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1366x768");
            } else if (resolution_choice == SXGA) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "12801024");
            } else if (resolution_choice == FHD) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "1920x1080");
            } else if (resolution_choice == QHD) {
                SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5-15, "2560x1440");
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5, "Fullscreen");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5, "ON");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5, "OFF");
            if (is_fullscreen == true) {
                fullscreen_choice.x = 7*GAME_WIDTH/20 - 5;
                fullscreen_choice.y = GAME_HEIGHT/5;
            } else {
                fullscreen_choice.x = 8*GAME_WIDTH/20 - 5;
                fullscreen_choice.y = GAME_HEIGHT/5;
            }
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+15, "Audio");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5+15, "ON");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5+15, "OFF");
            if (is_audio_enabled == true) {
                audio_choice.x = 7*GAME_WIDTH/20 - 5;
                audio_choice.y = GAME_HEIGHT/5+15;
            } else {
                audio_choice.x = 8*GAME_WIDTH/20 - 5;
                audio_choice.y = GAME_HEIGHT/5+15;
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+30, "Ball Speed");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5+30, "LOW");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5+30, "MED");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 9*GAME_WIDTH/20, GAME_HEIGHT/5+30, "HIGH");
            if (ball_speed_difficulty == B_LOW) {
                ball_speed_choice.x = 7*GAME_WIDTH/20 - 5;
                ball_speed_choice.y = GAME_HEIGHT/5+30;
            } else if (ball_speed_difficulty == B_MEDIUM) {
                ball_speed_choice.x = 8*GAME_WIDTH/20 - 5;
                ball_speed_choice.y = GAME_HEIGHT/5+30;
            } else if (ball_speed_difficulty == B_HIGH) {
                ball_speed_choice.x = 9*GAME_WIDTH/20 - 5;
                ball_speed_choice.y = GAME_HEIGHT/5+30;
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+45, "Paddle Speed");
            SDL_RenderDebugText(renderer, 7*GAME_WIDTH/20, GAME_HEIGHT/5+45, "LOW");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 8*GAME_WIDTH/20, GAME_HEIGHT/5+45, "MED");
            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, 9*GAME_WIDTH/20, GAME_HEIGHT/5+45, "HIGH");
            if (paddle_speed_difficulty == P_LOW) {
                paddle_speed_choice.x = 7*GAME_WIDTH/20 - 5;
                paddle_speed_choice.y = GAME_HEIGHT/5+45;
            } else if (paddle_speed_difficulty == P_MEDIUM) {
                paddle_speed_choice.x = 8*GAME_WIDTH/20 - 5;
                paddle_speed_choice.y = GAME_HEIGHT/5+45;
            } else if (paddle_speed_difficulty == P_HIGH) {
                paddle_speed_choice.x = 9*GAME_WIDTH/20 - 5;
                paddle_speed_choice.y = GAME_HEIGHT/5+45;
            }

            SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+70, "APPLY");

            SDL_SetRenderDrawColor(renderer, 214, 237, 23, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, GAME_WIDTH/7, GAME_HEIGHT/5+80, "BACK");

        }

        SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(renderer, &fullscreen_choice);
        SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(renderer, &audio_choice);
        SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(renderer, &ball_speed_choice);
        SDL_SetRenderDrawColor(renderer, 173, 239, 209, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(renderer, &paddle_speed_choice);

    } else if (window_choice == GAME) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 242, 170, 76, SDL_ALPHA_OPAQUE);
        SDL_SetRenderScale(renderer, 1.0f, 1.0f);

        /* If less than a full copy of the audio is queued for playback, put another copy in there.
            This is overkill, but easy when lots of RAM is cheap. One could be more careful and
            queue less at a time, as long as the stream doesn't run dry.  */
        if (SDL_GetAudioStreamQueued(sounds[0].stream) < ((int) sounds[0].wav_data_len)) {
            SDL_PutAudioStreamData(sounds[0].stream, sounds[0].wav_data, (int) sounds[0].wav_data_len);
        }
        
        SDL_FRect background;
        SDL_FRect paddle_player;
        SDL_FRect paddle_cpu;
        SDL_FRect ball;

        // Background covering only viewport and not entire screen
        background.x = 0;
        background.y = 0;
        background.w = GAME_WIDTH;
        background.h = GAME_HEIGHT;

        // Left edge of screen
        paddle_player.x = 5;
        paddle_player.y = s_position_player_y;
        paddle_player.w = 10;
        paddle_player.h = 60;


        // Right edge of screen
        paddle_cpu.x = GAME_WIDTH - 15;
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
        } else if (s_position_player_y > GAME_HEIGHT-paddle_player.h) {
            s_position_player_y = GAME_HEIGHT-paddle_player.h;
        } else {
            s_position_player_y -= 300*s_direction_player*deltatime*paddle_speed_multiplier;
        }

        // Move CPU vertically in ping-pong motion
        s_position_cpu_y -= 250*s_direction_cpu*deltatime*paddle_speed_multiplier;
        if (s_position_cpu_y < 0) {
            s_direction_cpu = DOWN;
        }
        
        if (s_position_cpu_y > GAME_HEIGHT-paddle_cpu.h) {
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

        s_position_ball_x -= 400*s_direction_ball_x*s_component_ball_x*deltatime*ball_speed_multiplier;
        s_position_ball_y -= 400*s_direction_ball_y*s_component_ball_y*deltatime*ball_speed_multiplier;

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
        if (s_position_ball_y >= GAME_HEIGHT) {
            s_direction_ball_y = UP;
        }

        // When ball crosses left or right side of screen and someone scores
        if (s_position_ball_x < -20) {
            s_position_ball_x = GAME_WIDTH/2;
            s_position_ball_y = SDL_rand(GAME_HEIGHT);
            s_component_ball_x = SDL_randf();
            s_direction_ball_x = UP;
            SDL_PutAudioStreamData(sounds[1].stream, sounds[1].wav_data, (int) sounds[1].wav_data_len);
            s_score_cpu++;
        }

        if (s_position_ball_x > GAME_WIDTH + 10) {
            s_position_ball_x = GAME_WIDTH/2;
            s_position_ball_y = SDL_rand(GAME_HEIGHT);
            s_component_ball_x = SDL_randf();
            s_direction_ball_x = UP;
            SDL_PutAudioStreamData(sounds[1].stream, sounds[1].wav_data, (int) sounds[1].wav_data_len);
            s_score_player++;
        }

        SDL_SetRenderDrawColor(renderer, 16, 24, 32, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(renderer, &background);

        // Rendering paddles and ball
        SDL_SetRenderDrawColor(renderer, 242, 170, 76, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(renderer, &paddle_player);
        SDL_RenderFillRect(renderer, &paddle_cpu);

        SDL_SetRenderDrawColor(renderer, 233, 75, 60, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(renderer, &ball);

        // Display scores
        SDL_SetRenderScale(renderer, 1.0f, 1.0f);
        SDL_SetRenderDrawColor(renderer, 151, 188, 98, SDL_ALPHA_OPAQUE);
        SDL_RenderDebugTextFormat(renderer, GAME_WIDTH/4, 100, "%d", s_score_player);
        SDL_RenderDebugTextFormat(renderer, 3*GAME_WIDTH/4, 100, "%d", s_score_cpu);

        // Middle partition
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
        for (int i = 0; i < GAME_HEIGHT; i += 10) {
            SDL_RenderPoint(renderer, GAME_WIDTH/2, i);
        }
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
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDL_Quit();
}