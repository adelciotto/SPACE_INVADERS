#ifndef _OPENGL_SPINVADERS_SHADERS_H_
#define _OPENGL_SPINVADERS_SHADERS_H_

#include <glad/glad.h>
#include <string>
#include <unordered_map>

#include "spinvaders_renderer.h"

struct OpenGLShader {
  const char *name;
  GLuint program;
  GLuint vert_shader;
  GLuint frag_shader;
  std::unordered_map<std::string, GLuint> uniform_locations;
};

struct OpenGLShaderContext {
  Shader active_shader;
  // Shaders that can be set by calling renderer_set_shader()
  OpenGLShader shaders[SHADER_MAX];
  // Custom shaders for specific drawing functions. e.g renderer_draw_texture_with_colormap()
  OpenGLShader colormap_shader;
};

int opengl_shaders_setup(OpenGLShaderContext *ctx);

void opengl_shaders_shutdown(OpenGLShaderContext *ctx);

void opengl_shaders_set_shader(OpenGLShaderContext *ctx, Shader shader);

#endif // _OPENGL_SPINVADERS_SHADERS_H_
