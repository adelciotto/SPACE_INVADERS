// Space Invaders service implementation.

#include "spinvaders.h"
#include "spinvaders_effects.h"
#include "spinvaders_machine.h"
#include "spinvaders_renderer.h"
#include "spinvaders_sound.h"

#define STB_IMAGE_IMPLEMENTATION
#include "lib/stb_image.h"

#define DRAWT_MAIN_W 1280
#define DRAWT_MAIN_H 1024
#define DRAWT_CRT_W 256 * 4
#define DRAWT_CRT_H 224 * 3

struct SpaceInvaders {
  Texture tex_background;
  Texture tex_overlay;
  Texture drawt_machinefb_with_overlay;
  Texture drawt_main;
  Texture drawt_final_upscale;
  Rect upscale_rect;
  int max_texture_width;
  int max_texture_height;
  int last_sx, last_sy;
};

static SpaceInvaders s_spinvaders = {};

// Texture utilities
//

int load_texture_from_file(Texture *texture, const char *filepath);
int setup_upscale_draw_target(int device_width, int device_height);
void setup_upscale_dest_rect(int device_width, int device_height);

// Space Invaders implementation
//

int spinvaders_setup() {
  // Setup the renderer.
  if (renderer_setup() != 0) {
    adc_log_error("Failed to setup spinvaders_renderer!");
    return -1;
  }
  renderer_get_max_texture_size(&s_spinvaders.max_texture_width, &s_spinvaders.max_texture_height);
  adc_log_info("Max texture size %dx%d", s_spinvaders.max_texture_width,
               s_spinvaders.max_texture_height);

  // Setup the sound player.
  if (sound_setup() != 0) {
    adc_log_error("Failed to setup spinvaders_sound!");
    return -1;
  }

  // Setup the machine.
  if (machine_setup() != 0) {
    adc_log_error("Failed to setup the spinvaders_machine!");
    return -1;
  }

  // Create all required draw targets.
  //

  TextureParams dtparams = {TEXTURE_TYPE_DRAWTARGET};
  if (renderer_create_texture(&s_spinvaders.drawt_machinefb_with_overlay, DRAWT_CRT_W, DRAWT_CRT_H,
                              nullptr, dtparams) != 0) {
    adc_log_error("Failed to create drawt_machinefb_with_overlay!");
    return -1;
  }
  if (renderer_create_texture(&s_spinvaders.drawt_main, DRAWT_MAIN_W, DRAWT_MAIN_H, nullptr,
                              dtparams) != 0) {
    adc_log_error("Failed to create drawt_main!");
    return -1;
  }

  // Load all images.
  //

  if (load_texture_from_file(&s_spinvaders.tex_overlay, "data/overlay.png") != 0) {
    adc_log_error("Failed to load texture data/overlay.png!");
    return -1;
  }
  Texture tex_background_original;
  if (load_texture_from_file(&tex_background_original, "data/background_hires.png") != 0) {
    adc_log_error("Failed to load texture data/background.png!");
    return -1;
  }

  // Draw the original background to a new texture with vignette
  if (renderer_create_texture(&s_spinvaders.tex_background, tex_background_original.width,
                              tex_background_original.height, nullptr, dtparams) != 0) {
    adc_log_error("Failed to create tex_background!");
    return -1;
  }
  renderer_set_shader(SHADER_VIGNETTE);
  renderer_set_draw_target(&s_spinvaders.tex_background);
  renderer_draw_texture(&tex_background_original);
  renderer_destroy_texture(&tex_background_original);

  // Setup the effects.
  if (effects_setup(DRAWT_CRT_W, DRAWT_CRT_H) != 0) {
    adc_log_error("Failed to setup the spinvaders_effects!");
    return -1;
  }

  renderer_set_shader();
  renderer_set_draw_target(nullptr);
  return 0;
}

void spinvaders_shutdown() {
  effects_shutdown();

  renderer_destroy_texture(&s_spinvaders.tex_background);
  renderer_destroy_texture(&s_spinvaders.tex_overlay);
  renderer_destroy_texture(&s_spinvaders.drawt_final_upscale);
  renderer_destroy_texture(&s_spinvaders.drawt_main);
  renderer_destroy_texture(&s_spinvaders.drawt_machinefb_with_overlay);

  machine_shutdown();
  renderer_shutdown();
}

void spinvaders_tick(const InputState *input) {
  machine_tick(input);
}

bool spinvaders_paused() {
  return machine_paused();
}

void spinvaders_set_pause(bool pause) {
  machine_set_pause(pause);
}

void spinvaders_draw() {
  Texture *drawt_machinefb_with_overlay = &s_spinvaders.drawt_machinefb_with_overlay;
  Texture *drawt_main = &s_spinvaders.drawt_main;
  Texture *drawt_final_upscale = &s_spinvaders.drawt_final_upscale;
  Texture *tex_background = &s_spinvaders.tex_background;
  Texture *tex_overlay = &s_spinvaders.tex_overlay;

  // Draw the machine framebuffer with color overlay at 1024x672.
  renderer_set_draw_target(drawt_machinefb_with_overlay);
  renderer_clear();
  renderer_draw_texture_with_colormap(machine_get_display_texture(), tex_overlay);

  // Draw the display with crt scanlines and barrel distortion.
  const Texture *display_with_crt = effects_crt_draw(drawt_machinefb_with_overlay);

  // Draw the display with glow.
  const Texture *display_with_glow = effects_glow_draw(display_with_crt);

  renderer_set_draw_target(drawt_main);

  // Draw the background.
  renderer_draw_texture(tex_background);

  // Draw the display with all effects.
  float rot = -90.0f;
  float w = DRAWT_CRT_H;
  float h = DRAWT_CRT_W;
  Rect dest = {DRAWT_MAIN_W / 2 - w / 2, DRAWT_MAIN_H / 2 + h / 2, h, w};
  renderer_draw_texture(display_with_glow, &dest, rot);

  // Upscale the main game area.
  renderer_set_shader();
  renderer_set_draw_target(drawt_final_upscale);
  renderer_draw_texture(drawt_main);

  // Draw the final upscaled texture onto the screen.
  Rect upscale_rect = s_spinvaders.upscale_rect;
  renderer_set_draw_target(nullptr);
  renderer_clear(0.0f, 0.0f, 0.0f, 1.0f);
  renderer_draw_texture(drawt_final_upscale, &upscale_rect);
}

void spinvaders_resize(int device_width, int device_height) {
  adc_log_info("Window resized to %dx%d", device_width, device_height);
  if (setup_upscale_draw_target(device_width, device_height) != 0) {
    adc_log_error("Failed to setup the upscale draw target!");
  }
  renderer_resize((float)device_width, (float)device_height);
  setup_upscale_dest_rect(device_width, device_height);
}

// Texture utilities implementation
//

int load_texture_from_file(Texture *texture, const char *filepath) {
  int w, h, n;
  unsigned char *data = stbi_load(filepath, &w, &h, &n, 4);
  if (!data) {
    adc_log_error("stbi_load() failed!");
    return -1;
  }

  int result = renderer_create_texture(texture, w, h, data);
  free(data);
  return result;
}

void setup_upscale_dest_rect(int device_width, int device_height) {
  int w = device_width;
  int h = device_height;
  float want_aspect = (float)DRAWT_MAIN_W / DRAWT_MAIN_H;
  float real_aspect = (float)w / h;
  float scale = 1.0f;
  Rect dest;

  // Aspect ratios are the same, just scale appropriately.
  if (fabs(want_aspect - real_aspect) < 0.0001f) {
    dest.x = 0;
    dest.y = 0;
    dest.w = w;
    dest.h = h;
  }
  // We want a wider aspect ratio than is available, so letterbox it.
  else if (want_aspect > real_aspect) {
    scale = (float)w / DRAWT_MAIN_W;
    dest.x = 0;
    dest.w = w;
    dest.h = (int)floor(DRAWT_MAIN_H * scale);
    dest.y = (int)(h - dest.h) / 2;
  }
  // We want a narrower aspect ratio than is available, so use side bars.
  else {
    scale = (float)h / DRAWT_MAIN_H;
    dest.y = 0;
    dest.h = h;
    dest.w = (int)floor(DRAWT_MAIN_W * scale);
    dest.x = (int)(w - dest.w) / 2;
  }
  s_spinvaders.upscale_rect = dest;
}

static int get_upscale(int *sx, int *sy, int device_width, int device_height);

int setup_upscale_draw_target(int device_width, int device_height) {
  int sx = 1;
  int sy = 1;
  if (get_upscale(&sx, &sy, device_width, device_height) != 0) {
    return -1;
  }

  if (sx == s_spinvaders.last_sx && sy == s_spinvaders.last_sy) {
    return 0;
  }

  Texture *drawt_final_upscale = &s_spinvaders.drawt_final_upscale;

  if (drawt_final_upscale->active()) {
    renderer_destroy_texture(drawt_final_upscale);
  }

  int w = DRAWT_MAIN_W * sx;
  int h = DRAWT_MAIN_H * sy;
  TextureParams params;
  params.type = TEXTURE_TYPE_DRAWTARGET;
  params.min_filter = params.mag_filter = TEXTURE_FILTER_LINEAR;
  if (renderer_create_texture(drawt_final_upscale, w, h, nullptr, params) != 0) {
    adc_log_error("Failed to create the upscale draw target!");
    return -1;
  }
  adc_log_info("Upscale texture dimensions: %dx%d", w, h);

  s_spinvaders.last_sx = sx;
  s_spinvaders.last_sy = sy;
  return 0;
}

// Max resolution 2560x1440
// TODO: Runtime calculation?
#define MAX_SCREEN_TEXTURE_PIXELS 3686400

static int limit_upscale(int *sx, int *sy) {
  int maxw = s_spinvaders.max_texture_width;
  int maxh = s_spinvaders.max_texture_height;

  while (*sx * DRAWT_MAIN_W > maxw) {
    --*sx;
  }
  while (*sy * DRAWT_MAIN_H > maxh) {
    --*sy;
  }

  if ((*sx < 1 && maxw > 0) || (*sy < 1 && maxh > 0)) {
    adc_log_error("Unable to create a texture big enough for the whole "
                  "screen! "
                  "Maximum texture size %dx%d\n",
                  maxw, maxw);
    return -1;
  }

  // We limit the amount of texture memory used for the screen texture,
  // since beyond a certain point there are diminishing returns. Also,
  // depending on the hardware there may be performance problems with very
  // huge textures.
  while (*sx * *sy * DRAWT_MAIN_W * DRAWT_MAIN_H > MAX_SCREEN_TEXTURE_PIXELS) {
    if (*sx > *sy) {
      --*sx;
    } else {
      --*sy;
    }
  }

  return 0;
}

static int get_upscale(int *sx, int *sy, int device_width, int device_height) {
  int w = device_width;
  int h = device_height;

  if (w > h) {
    // Wide window.
    w = MAX(h, h * (DRAWT_MAIN_W / DRAWT_MAIN_H));
  } else {
    // Tall window.
    h = MAX(w, w * (DRAWT_MAIN_H / DRAWT_MAIN_W));
  }

  *sx = (w + DRAWT_MAIN_W - 1) / DRAWT_MAIN_W;
  *sy = (h + DRAWT_MAIN_H - 1) / DRAWT_MAIN_H;

  if (*sx < 1)
    *sx = 1;
  if (*sy < 1)
    *sy = 1;

  return limit_upscale(sx, sy);
}
