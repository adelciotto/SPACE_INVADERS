#include <glad/glad.h>

#define HANDMADE_MATH_IMPLEMENTATION
#include "lib/HandmadeMath.h"

#include "opengl_spinvaders_shaders.h"
#include "spinvaders_renderer.h"
#include "spinvaders_shared.h"

struct BackendData {
  GLuint id;
  GLuint fbo;
  GLenum format;
  GLenum format_type;
};

struct OpenGLRenderer {
  float device_width;
  float device_height;
  float current_draw_width;
  float current_draw_height;
  Texture *current_draw_target;
  hmm_mat4 ortho_projection;
  GLuint vao;
  OpenGLShaderContext shader_ctx;
};

static OpenGLRenderer s_renderer = {};

static GLenum s_gl_filters[] = {
    GL_NEAREST, // TEXTURE_FILTER_NEAREST
    GL_LINEAR   // TEXTURE_FILTER_LINEAR
};

// OpenGLRenderer implementation
//

static GLuint create_vertex_array(const float *verts, int verts_size, int components);
static GLuint create_texture(TextureParams params, int width, int height, GLenum format,
                             GLenum format_type, GLvoid *data, int pitch);
static GLuint create_fbo(GLuint texture);

int renderer_setup() {
  // Setup the shaders.
  if (opengl_shaders_setup(&s_renderer.shader_ctx) != 0) {
    adc_log_error("Failed to setup the OpenGL shaders");
  }

  // Setup the ortho projection matrix.
  renderer_set_draw_target(nullptr);

  // Setup the vertex array for quad.
  //

  // clang-format off
  const float verts[] = {
  	// pos      // tex
	  0.0f, 1.0f, 0.0f, 1.0f,
	  1.0f, 0.0f, 1.0f, 0.0f,
	  0.0f, 0.0f, 0.0f, 0.0f, 

	  0.0f, 1.0f, 0.0f, 1.0f,
	  1.0f, 1.0f, 1.0f, 1.0f,
	  1.0f, 0.0f, 1.0f, 0.0f
  };
  // clang-format on
  s_renderer.vao = create_vertex_array(verts, sizeof(verts), 4);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_SCISSOR_TEST);
  glEnable(GL_BLEND);
  renderer_set_blend_mode(BLEND_ALPHA);
  renderer_set_shader();

  return 0;
}

void renderer_shutdown() {
  opengl_shaders_shutdown(&s_renderer.shader_ctx);
  glDeleteVertexArrays(1, &s_renderer.vao);
  adc_log_info("OpenGLRenderer resources destroyed");
}

void renderer_clear(float r, float g, float b, float a) {
  glClearColor(r, g, b, a);
  glClear(GL_COLOR_BUFFER_BIT);
}

void renderer_resize(float width, float height) {
  s_renderer.device_width = width;
  s_renderer.device_height = height;
}

int renderer_create_texture(Texture *texture, int width, int height, void *pixels,
                            TextureParams params) {
  BackendData *backend_data = (BackendData *)malloc(sizeof(BackendData));
  if (!texture) {
    adc_log_error("Failed to malloc() memory for texture BackendData struct!");
    return -1;
  }

  GLenum format = GL_RGBA;
  GLenum format_type = GL_UNSIGNED_BYTE;
  if (params.type == TEXTURE_TYPE_PIXEL_ACCESS) {
    backend_data->format = GL_BGRA;
    backend_data->format_type = GL_UNSIGNED_INT_8_8_8_8_REV;
  }

  int pitch = width * 4;
  backend_data->id = create_texture(params, width, height, format, format_type, pixels, pitch);
  if (params.type == TEXTURE_TYPE_DRAWTARGET) {
    backend_data->fbo = create_fbo(backend_data->id);
  }

  texture->params = params;
  texture->width = width;
  texture->height = height;
  texture->pitch = pitch;
  texture->backend_data = backend_data;
  return 0;
}

void renderer_destroy_texture(Texture *texture) {
  if (texture && texture->backend_data) {
    glDeleteFramebuffers(1, &texture->backend_data->fbo);
    glDeleteTextures(1, &texture->backend_data->id);
    free(texture->backend_data);
  }
  texture->width = 0;
  texture->height = 0;
  texture->backend_data = nullptr;
}

void renderer_set_draw_target(Texture *texture) {
  float bottom, top;
  float width;
  float height;
  GLuint fbo;

  // Caller uses nullptr to reset the draw target. When resetting we need to update the
  // ortho projection and viewport back to the original device (window) dimensions.
  if (!texture) {
    s_renderer.current_draw_target = nullptr;
    width = s_renderer.device_width;
    height = s_renderer.device_height;
    bottom = height;
    top = 0.0f;
    fbo = 0;
  }
  // When caller passes a valid texture as the draw target we need to update the ortho projection
  // and viewport to match the dimensions of this new target.
  else {
    s_renderer.current_draw_target = texture;
    width = texture->width;
    height = texture->height;
    bottom = 0.0f;
    top = height;
    fbo = texture->backend_data->fbo;
  }

  s_renderer.ortho_projection = HMM_Orthographic(0.0f, width, bottom, top, -1.0f, 1.0f);
  glViewport(0, 0, width, height);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  s_renderer.current_draw_width = width;
  s_renderer.current_draw_height = height;
}

void renderer_set_shader(Shader shader) {
  opengl_shaders_set_shader(&s_renderer.shader_ctx, shader);
}

void renderer_update_shader_uniform1f(const char *uniform, float v1) {
  OpenGLShaderContext *ctx = &s_renderer.shader_ctx;
  OpenGLShader *shader_data = &ctx->shaders[ctx->active_shader];
  glUniform1f(shader_data->uniform_locations[std::string(uniform)], v1);
}

void renderer_update_shader_uniform2f(const char *uniform, float v1, float v2) {
  OpenGLShaderContext *ctx = &s_renderer.shader_ctx;
  OpenGLShader *shader_data = &ctx->shaders[ctx->active_shader];
  glUniform2f(shader_data->uniform_locations[std::string(uniform)], v1, v2);
}

void renderer_draw_texture(const Texture *texture, const Rect *destrect, float angledeg) {
  // Provide default destrect if nullptr given.
  Rect dest;
  if (destrect == nullptr) {
    dest = {0.0f, 0.0f, s_renderer.current_draw_width, s_renderer.current_draw_height};
  } else {
    dest = *destrect;
  }

  // Update any uniforms for shaders that need them.
  OpenGLShaderContext *ctx = &s_renderer.shader_ctx;
  OpenGLShader *shader_data = &ctx->shaders[ctx->active_shader];
  switch (ctx->active_shader) {
  case SHADER_VIGNETTE: {
    glUniform1f(shader_data->uniform_locations["u_aspect"],
                (float)texture->width / (float)texture->height);
  } break;
  case SHADER_SCANLINES:
    // Fallthrough
  case SHADER_GLOW_BLUR: {
    glUniform2f(shader_data->uniform_locations["u_resolution"], texture->width, texture->height);
  } break;
  }

  // Update the transform uniform.
  hmm_mat4 translate = HMM_Translate(HMM_Vec3(dest.x, dest.y, 0.0f));
  hmm_mat4 rotate = HMM_Rotate(angledeg, HMM_Vec3(0.0f, 0.0f, 1.0f));
  hmm_mat4 scale = HMM_Scale(HMM_Vec3(dest.w, dest.h, 1.0f));
  hmm_mat4 model = translate * rotate * scale;
  hmm_mat4 transform = s_renderer.ortho_projection * model;
  glUniformMatrix4fv(shader_data->uniform_locations["u_transform"], 1, false, *transform.Elements);

  // Bind the texture.
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture->backend_data->id);

  // Draw the textured quad.
  glBindVertexArray(s_renderer.vao);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);
}

void renderer_draw_texture_with_colormap(const Texture *texture, const Texture *colormap) {
  // Calculate the transform matrix.
  hmm_mat4 model =
      HMM_Scale(HMM_Vec3(s_renderer.current_draw_width, s_renderer.current_draw_height, 1.0f));
  hmm_mat4 transform = s_renderer.ortho_projection * model;

  // Bind shader and update the transform uniform.
  OpenGLShaderContext *ctx = &s_renderer.shader_ctx;
  OpenGLShader *shader_data = &ctx->colormap_shader;
  glUseProgram(shader_data->program);
  glUniformMatrix4fv(shader_data->uniform_locations["u_transform"], 1, false, *transform.Elements);

  // Bind the required textures (0 for texture, 1 for colormap).
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture->backend_data->id);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, colormap->backend_data->id);

  // Draw the textured quad.
  glBindVertexArray(s_renderer.vao);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);

  // Reset the shader.
  renderer_set_shader(ctx->active_shader);
}

void renderer_update_texture(Texture *texture, void *pixels) {
  if (texture->params.type == TEXTURE_TYPE_PIXEL_ACCESS) {
    glBindTexture(GL_TEXTURE_2D, texture->backend_data->id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, texture->pitch / 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture->width, texture->height,
                    texture->backend_data->format, texture->backend_data->format_type, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
  }
}

void renderer_set_blend_mode(BlendMode mode) {
  GLenum func = GL_FUNC_ADD;
  GLenum src_rgb = GL_ONE;
  GLenum src_a = GL_ONE;
  GLenum dst_rgb = GL_ZERO;
  GLenum dst_a = GL_ZERO;

  switch (mode) {
  case BLEND_ALPHA: {
    src_rgb = src_a = GL_ONE;
    dst_rgb = dst_a = GL_ONE_MINUS_SRC_ALPHA;
  } break;
  case BLEND_ADD: {
    src_rgb = GL_ONE;
    src_a = GL_ZERO;
    dst_rgb = dst_a = GL_ONE;
  } break;
  default: {
    src_rgb = src_a = GL_ONE;
    dst_rgb = dst_a = GL_ZERO;
  } break;
  }

  glBlendEquation(func);
  glBlendFuncSeparate(src_rgb, dst_rgb, src_a, dst_a);
}

void renderer_get_max_texture_size(int *w, int *h) {
  GLint size;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
  *w = size;
  *h = size;
}

// OpenGL utilities implementation
//

static GLuint create_vertex_array(const float *verts, int verts_size, int components) {
  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, verts_size, verts, GL_STATIC_DRAW);

  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, components, GL_FLOAT, GL_FALSE, components * sizeof(float), (void *)0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
  return vao;
}

static GLuint create_texture(TextureParams params, int width, int height, GLenum format,
                             GLenum format_type, GLvoid *data, int pitch) {
  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

// https://developer.apple.com/library/archive/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_texturedata/opengl_texturedata.html
#ifdef __MACOSX__
#ifndef GL_TEXTURE_STORAGE_HINT_APPLE
#define GL_TEXTURE_STORAGE_HINT_APPLE 0x85BC
#endif
#ifndef STORAGE_CACHED_APPLE
#define STORAGE_CACHED_APPLE 0x85BE
#endif
#ifndef STORAGE_SHARED_APPLE
#define STORAGE_SHARED_APPLE 0x85BF
#endif
  if (type == TEXTURE_TYPE_PIXEL_ACCESS) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_SHARED_APPLE);
  } else {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_CACHED_APPLE);
  }

  if (type == TEXTURE_TYPE_PIXEL_ACCESS && (texture->width % 8) == 0) {
    glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, texture->pitch / 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, format, format_type, data);
    glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_FALSE);
  } else
#endif
  {
    GLenum min_filter = s_gl_filters[params.min_filter];
    GLenum mag_filter = s_gl_filters[params.mag_filter];
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, format, format_type, data);
  }
  return texture;
}

static GLuint create_fbo(GLuint texture) {
  GLuint fbo;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
  GLenum buffer = GL_COLOR_ATTACHMENT0;
  glDrawBuffers(1, &buffer);
  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    adc_log_error("GL framebuffer status not complete! %u", status);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return fbo;
}
