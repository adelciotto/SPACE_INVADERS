// SDL2 platform layer implementation.
#define ADC_LOG_IMPLEMENTATION

#include <glad/glad.h>

#include <SDL.h>

#include "spinvaders.h"

#include "lib/imgui/imgui.h"
#include "lib/imgui/imgui_impl_opengl3.h"
#include "lib/imgui/imgui_impl_sdl.h"
#include "spinvaders_imgui.h"

struct SDL2Window {
  int width;
  int height;
  bool fullscreen;
  bool close_requested;
  SDL_Window *sdlwindow;
  SDL_GLContext glcontext;
};

struct SDL2Context {
  SDL2Window window;
  InputState input;
  SDL_GameController *controller;
};

static SDL2Context s_ctx = {};

static int sdl2_setup_window() {
  SDL2Window *window = &s_ctx.window;

  window->width = 640;
  window->height = 512;

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  uint32_t win_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
  window->sdlwindow =
      SDL_CreateWindow("Space Invaders", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                       window->width, window->height, win_flags);
  if (!window->sdlwindow) {
    adc_log_error("Failed to create SDL_Window! %s", SDL_GetError());
    return -1;
  }

  window->glcontext = SDL_GL_CreateContext(window->sdlwindow);
  if (!window->glcontext) {
    adc_log_error("Failed to create SDL_GLContext! %s", SDL_GetError());
    return -1;
  }

  SDL_GL_MakeCurrent(window->sdlwindow, window->glcontext);
  SDL_GL_SetSwapInterval(1);

  window->close_requested = false;
  return 0;
}

static int sdl2_setup() {
  // Check the SDL version we compile and link against, and log some useful
  // information.
  SDL_version compiled, linked;
  SDL_VERSION(&compiled);
  SDL_GetVersion(&linked);
  adc_log_info("Compiled against SDL version %d.%d.%d", compiled.major, compiled.minor,
               compiled.patch);
  if (compiled.major != linked.major || compiled.minor != linked.minor ||
      compiled.patch != linked.patch)
    adc_log_warn("Linked against different SDL version %d.%d.%d", linked.major, linked.minor,
                 linked.patch);

  uint32_t init_flags = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER;
  if (SDL_Init(init_flags) != 0) {
    adc_log_error("Failed to init SDL! %s", SDL_GetError());
    return -1;
  }

  if (sdl2_setup_window() != 0)
    return -1;

  if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
    adc_log_error("Failed to setup OpenGL loader!");
    return -1;
  }
  adc_log_info("OpenGL version %d.%d loaded", GLVersion.major, GLVersion.minor);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui_ImplSDL2_InitForOpenGL(s_ctx.window.sdlwindow, s_ctx.window.glcontext);
  ImGui_ImplOpenGL3_Init();

  return 0;
}

static int sdl2_shutdown(int exit_code) {
  spinvaders_shutdown();

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  imgui_shutdown();

  if (s_ctx.controller) {
    SDL_GameControllerClose(s_ctx.controller);
  }
  if (s_ctx.window.glcontext) {
    SDL_GL_DeleteContext(s_ctx.window.glcontext);
  }
  if (s_ctx.window.sdlwindow) {
    SDL_DestroyWindow(s_ctx.window.sdlwindow);
  }
  SDL_Quit();

  return exit_code;
}

static void resize(SDL2Window *win) {
  if (!win->fullscreen) {
    SDL_GetWindowSize(win->sdlwindow, &win->width, &win->height);
  }

  int w, h;
  SDL_GL_GetDrawableSize(win->sdlwindow, &w, &h);
  spinvaders_resize(w, h);
}

static void toggle_fullscreen(SDL2Window *win) {
  win->fullscreen = !win->fullscreen;

  if (win->fullscreen) {
    SDL_SetWindowFullscreen(win->sdlwindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
  } else {
    SDL_SetWindowFullscreen(win->sdlwindow, false);
    SDL_SetWindowSize(win->sdlwindow, win->width, win->height);
    SDL_SetWindowPosition(win->sdlwindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  }
}

static void handle_window_event(SDL_Event *ev) {
  SDL2Window *win = &s_ctx.window;

  switch (ev->window.event) {
  case SDL_WINDOWEVENT_SIZE_CHANGED:
    // Fallthrough
  case SDL_WINDOWEVENT_RESIZED: {
    resize(win);
  } break;
  case SDL_WINDOWEVENT_SHOWN:
    // Fallthrough
  case SDL_WINDOWEVENT_FOCUS_GAINED: {
    spinvaders_set_pause(false);
  } break;
  case SDL_WINDOWEVENT_HIDDEN:
    // Fallthrough
  case SDL_WINDOWEVENT_FOCUS_LOST: {
    spinvaders_set_pause(true);
  } break;
  }
}

static bool check_fullscreen_toggle(SDL_Keysym sym) {
  uint16_t flags = (KMOD_LALT | KMOD_RALT);
#if defined(__MACOSX__)
  flags |= (KMOD_LGUI | KMOD_RGUI);
#endif
  return (sym.scancode == SDL_SCANCODE_RETURN || sym.scancode == SDL_SCANCODE_KP_ENTER) &&
         (sym.mod & flags) != 0;
}

static int map_key(SDL_Keycode key) {
  switch (key) {
  case SDLK_LEFT:
    return BUTTON_LEFT;
  case SDLK_RIGHT:
    return BUTTON_RIGHT;
  case SDLK_1:
    return BUTTON_START_1P;
  case SDLK_2:
    return BUTTON_START_2P;
  case SDLK_z:
    return BUTTON_FIRE;
  case SDLK_i:
    return BUTTON_INSERT_CREDIT;
  case SDLK_RSHIFT:
    return BUTTON_TILT;
  default:
    return -1;
  }
}

static int map_controller_button(uint8_t button) {
  switch (button) {
  case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
    return BUTTON_LEFT;
  case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
    return BUTTON_RIGHT;
  case SDL_CONTROLLER_BUTTON_START:
    return BUTTON_START_1P;
  case SDL_CONTROLLER_BUTTON_BACK:
    return BUTTON_START_2P;
  case SDL_CONTROLLER_BUTTON_A:
    // Fallthrough
  case SDL_CONTROLLER_BUTTON_X:
    return BUTTON_FIRE;
  case SDL_CONTROLLER_BUTTON_Y:
    return BUTTON_INSERT_CREDIT;
  case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
    return BUTTON_TILT;
  default:
    return -1;
  }
}

#define BUTTON_SET(input, btn) ((input->buttons) |= (1UL << btn))
#define BUTTON_CLEAR(input, btn) ((input->buttons) &= ~(1UL << btn))

static void handle_keyboard_event(SDL_Event *ev) {
  InputState *input = &s_ctx.input;

  switch (ev->type) {
  case SDL_KEYDOWN: {
    if (check_fullscreen_toggle(ev->key.keysym)) {
      spinvaders_set_pause(true);
      toggle_fullscreen(&s_ctx.window);
      spinvaders_set_pause(false);
      return;
    }

    int btn = map_key(ev->key.keysym.sym);
    if (btn >= 0) {
      BUTTON_SET(input, btn);
    }
  } break;
  case SDL_KEYUP: {
    int btn = map_key(ev->key.keysym.sym);
    if (btn >= 0) {
      BUTTON_CLEAR(input, btn);
    }
  } break;
  }
}

static void handle_controller_event(SDL_Event *ev) {
  InputState *input = &s_ctx.input;

  switch (ev->type) {
  case SDL_CONTROLLERDEVICEADDED:
    if (s_ctx.controller) {
      break;
    }

    if (SDL_IsGameController(ev->cdevice.which)) {
      s_ctx.controller = SDL_GameControllerOpen(ev->cdevice.which);
      adc_log_info("SDL_GameController %s connected!", SDL_GameControllerName(s_ctx.controller));
    }
    break;
  case SDL_CONTROLLERDEVICEREMOVED: {
    if (!s_ctx.controller) {
      break;
    }

    SDL_Joystick *joy = SDL_GameControllerGetJoystick(s_ctx.controller);
    SDL_JoystickID joyid = SDL_JoystickInstanceID(joy);
    if (ev->cdevice.which != joyid) {
      break;
    }
    adc_log_info("SDL_GameController %s removed!", SDL_GameControllerName(s_ctx.controller));
    SDL_GameControllerClose(s_ctx.controller);
    s_ctx.controller = NULL;
  } break;
  case SDL_CONTROLLERBUTTONDOWN: {
    int btn = map_controller_button(ev->cbutton.button);
    if (btn >= 0) {
      BUTTON_SET(input, btn);
    }
  } break;
  case SDL_CONTROLLERBUTTONUP: {
    int btn = map_controller_button(ev->cbutton.button);
    if (btn >= 0) {
      BUTTON_CLEAR(input, btn);
    }
  } break;
  }
}

static void poll_events() {
  SDL_Event ev;
  while (SDL_PollEvent(&ev)) {
    switch (ev.type) {
    case SDL_QUIT: {
      s_ctx.window.close_requested = true;
    } break;
    case SDL_WINDOWEVENT: {
      handle_window_event(&ev);
    } break;
    }

    handle_keyboard_event(&ev);
    handle_controller_event(&ev);
    ImGui_ImplSDL2_ProcessEvent(&ev);
  }
}

uint64_t get_performance_counter() {
  return SDL_GetPerformanceCounter();
}

uint64_t get_performance_freq() {
  return SDL_GetPerformanceFrequency();
}

#define DELTA_TIME_HISTORY_MAX 4
#define DELTA_TIME_SNAP_FREQS 4

int main(int argc, char *argv[]) {
  if (sdl2_setup() != 0) {
    adc_log_error("Failed to setup SDL2 platform!");
    return sdl2_shutdown(EXIT_FAILURE);
  }
  imgui_setup();

  SDL2Window *win = &s_ctx.window;
  InputState *input = &s_ctx.input;

  if (spinvaders_setup() != 0) {
    adc_log_error("Failed to setup space invaders!");
    return sdl2_shutdown(EXIT_FAILURE);
  }
  resize(win);

  // Main loop
  //

  // Credit to TylerGlaiel for the frame timing code that was used as a
  // reference.
  // https://github.com/TylerGlaiel/FrameTimingControl
  int64_t target_time_per_tick = get_performance_freq() / 60;
  int64_t tick_accum = 0;
  int64_t last_counter = get_performance_counter();
  int64_t vsync_maxerr = get_performance_freq() * 0.0002;
  int64_t time_60hz = get_performance_freq() / 60;
  int64_t snap_freqs[DELTA_TIME_SNAP_FREQS] = {
      time_60hz,          // 60fps
      time_60hz * 3,      // 20fps
      time_60hz * 4,      // 15fps
      (time_60hz + 1) / 2 // 120ps
  };
  int64_t delta_time_hist[DELTA_TIME_HISTORY_MAX];
  for (int i = 0; i < DELTA_TIME_HISTORY_MAX; i++)
    delta_time_hist[i] = target_time_per_tick;
  unsigned int hist_indx = 0;

  while (!s_ctx.window.close_requested) {
    int64_t current_counter = get_performance_counter();
    int64_t delta_time = current_counter - last_counter;
    last_counter = current_counter;

    // Handle unexpected delta time anomalies.
    if (delta_time > target_time_per_tick * 8) {
      delta_time = target_time_per_tick;
    }
    if (delta_time < 0) {
      delta_time = 0;
    }

    // VSync time snapping.
    for (int i = 0; i < DELTA_TIME_SNAP_FREQS; i++) {
      if (llabs(delta_time - snap_freqs[i]) < vsync_maxerr) {
        delta_time = snap_freqs[i];
        break;
      }
    }

    // Average the delta time.
    delta_time_hist[hist_indx] = delta_time;
    hist_indx = (hist_indx + 1) % DELTA_TIME_HISTORY_MAX;
    delta_time = 0;
    for (int i = 0; i < DELTA_TIME_HISTORY_MAX; i++) {
      delta_time += delta_time_hist[i];
    }
    delta_time /= DELTA_TIME_HISTORY_MAX;

    tick_accum += delta_time;

    // Spiral of death protection.
    if (tick_accum > target_time_per_tick * 8) {
      tick_accum = 0;
      delta_time = target_time_per_tick;
    }

    poll_events();

    // Ensure the machine ticks at the correct frequency.
    while (tick_accum >= target_time_per_tick) {
      spinvaders_tick(input);
      tick_accum -= target_time_per_tick;
    }

    // Draw the game.
    spinvaders_draw();

    // Draw the ImGui powered ui.
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    imgui_draw();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(win->sdlwindow);
  }

  // Application exit
  //

  return sdl2_shutdown(EXIT_SUCCESS);
}
