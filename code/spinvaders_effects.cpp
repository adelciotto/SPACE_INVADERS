#include "spinvaders_effects.h"

#include "spinvaders_renderer.h"
#include "spinvaders_shared.h"

struct GlowEffect {
  Texture front, back;
};

struct Effects {
  int width, height;
  Texture glow_front;
  Texture glow_back;
  Texture crt_scanlines;
  Texture crt_barrel;
};

static Effects s_effects = {};

static int create_draw_target(Texture *texture, int width, int height) {
  if (renderer_create_texture(texture, width, height, nullptr, {TEXTURE_TYPE_DRAWTARGET}) != 0) {
    adc_log_error("Failed to create effect draw target!");
    return -1;
  }
  return 0;
}

int effects_setup(int width, int height) {
  TextureParams params = {TEXTURE_TYPE_DRAWTARGET};
  if (renderer_create_texture(&s_effects.glow_front, width, height, nullptr, params) != 0) {
    adc_log_error("Failed to create glow effect front draw target!");
    return -1;
  }
  if (renderer_create_texture(&s_effects.glow_back, width, height, nullptr, params) != 0) {
    adc_log_error("Failed to create glow effect back draw target!");
    return -1;
  }
  if (renderer_create_texture(&s_effects.crt_scanlines, width, height, nullptr, params) != 0) {
    adc_log_error("Failed to create crt effect scanlines draw target!");
    return -1;
  }
  if (renderer_create_texture(&s_effects.crt_barrel, width, height, nullptr, params) != 0) {
    adc_log_error("Failed to create crt effect barrel draw target!");
    return -1;
  }

  s_effects.width = width;
  s_effects.height = height;
  return 0;
}

void effects_shutdown() {
  renderer_destroy_texture(&s_effects.crt_barrel);
  renderer_destroy_texture(&s_effects.crt_scanlines);
  renderer_destroy_texture(&s_effects.glow_back);
  renderer_destroy_texture(&s_effects.glow_front);
}

const Texture *effects_glow_draw(const Texture *scene) {
  Texture *front = &s_effects.glow_front;
  Texture *back = &s_effects.glow_back;

  // 1st pass, draw scene with brightness threshold to front.
  renderer_set_draw_target(front);
  renderer_set_shader(SHADER_GLOW_THRESHOLD);
  renderer_clear();
  renderer_draw_texture(scene);

  // 2nd pass, draw front with x blur to back.
  renderer_set_draw_target(back);
  renderer_set_shader(SHADER_GLOW_BLUR);
  renderer_update_shader_uniform2f("u_direction", 1.0f, 0.0f);
  renderer_clear();
  renderer_draw_texture(front);

  // 3rd pass
  //

  renderer_set_draw_target(front);
  renderer_clear();

  // Draw original scene without blur.
  renderer_set_shader();
  renderer_set_blend_mode(BLEND_ADD);
  renderer_draw_texture(scene);

  // Draw y blurred back on top with additive blending for lighting.
  renderer_set_shader(SHADER_GLOW_BLUR);
  renderer_update_shader_uniform2f("u_direction", 0.0f, 1.0f);
  renderer_draw_texture(back);

  // Reset blend mode and shader.
  renderer_set_shader();
  renderer_set_blend_mode(BLEND_ALPHA);

  return front;
}

const Texture *effects_crt_draw(const Texture *scene) {
  Texture *scanlines = &s_effects.crt_scanlines;
  Texture *barrel = &s_effects.crt_barrel;

  // 1st pass, draw the scene with scanlines.
  renderer_set_draw_target(scanlines);
  renderer_set_shader(SHADER_SCANLINES);
  renderer_clear();
  renderer_draw_texture(scene);

  // 2nd pass, draw the scanlines scene with barrel distortion.
  renderer_set_draw_target(barrel);
  renderer_set_shader(SHADER_CRT);
  renderer_clear();
  renderer_draw_texture(scanlines);

  // Reset the shader.
  renderer_set_shader();

  return barrel;
}
