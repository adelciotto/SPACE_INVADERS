#include "opengl_spinvaders_shaders.h"

#include "spinvaders_shared.h"

// Shader sources.
//

// 2D vertex shader, used by all the shader programs.
static const char *s_vert_src = R"(
#version 330 core

layout (location = 0) in vec4 vertex;
out vec2 texcoord;
uniform mat4 u_transform;

void main() {
	texcoord = vertex.zw;
	gl_Position = u_transform * vec4(vertex.xy, 0.0, 1.0);
}
)";

// Default fragment shader. The default shader used when drawing textures.
static const char *s_frag_src = R"(
#version 330 core

in vec2 texcoord;
out vec4 fragcolor;
uniform sampler2D u_texture;

void main() {
	fragcolor = texture(u_texture, texcoord);
}
)";

// Colormap fragment shader. Like the default shader, but color is sampled from a separate colormap.
static const char *s_frag_colormap_src = R"(
#version 330 core

in vec2 texcoord;
out vec4 fragcolor;
uniform sampler2D u_texture;
uniform sampler2D u_colormap;

void main() {
  vec4 color = texture(u_colormap, texcoord);
  fragcolor = texture(u_texture, texcoord) * color;
}
)";

// Vignette fragment shader. Applies a vignette to texture.
// Reference: https://github.com/vrld/moonshine/blob/master/vignette.lua
static const char *s_frag_vignette_src = R"(
#version 330 core

in vec2 texcoord;
out vec4 fragcolor;
uniform sampler2D u_texture;
uniform float u_aspect;

const float radius = 0.6;
const float softness = 0.5;
const float opacity = 0.6;
void main()
{
  vec4 color = vec4(vec3(0), 1.0);
  float aspect = max(u_aspect, 1.0 / u_aspect);
  float v = 1.0 - smoothstep(radius, radius-softness, length((texcoord - vec2(0.5, 0.7)) * aspect));
  fragcolor = mix(texture(u_texture, texcoord), color, v*opacity);
}  
)";

// CRT fragment shader. Applies a crt barrel distortion to texture.
// Reference: https://github.com/vrld/moonshine/blob/master/crt.lua
static const char *s_frag_crt_src = R"(
#version 330 core

in vec2 texcoord;
out vec4 fragcolor;
uniform sampler2D u_texture;
uniform vec2 u_resolution;

const float scale = 1;
const float feather = 0.02;
void main()
{
  vec2 uv = texcoord * 2.0 - vec2(1.0);
  vec2 distortion = vec2(1.02, 1.065);

  uv *= scale;
  uv += (uv.yx*uv.yx) * uv * (distortion - 1.0);
  float mask = (1.0 - smoothstep(1.0 - feather, 1.0, abs(uv.x)))
             * (1.0 - smoothstep(1.0 - feather, 1.0, abs(uv.y)));

  uv = (uv + vec2(1.0)) / 2.0;
  fragcolor = texture(u_texture, uv) * mask;
}  
)";

// Scanlines fragment shader. Applies scanlines to texture.
// Reference: https://github.com/vrld/moonshine/blob/master/scanlines.lua
static const char *s_frag_scanlines_src = R"(
#version 330 core

in vec2 texcoord;
out vec4 fragcolor;
uniform sampler2D u_texture;
uniform vec2 u_resolution;

const float width = 1.5;
const float phase = 0;
const float thickness = 1;
const float opacity = 0.7;
const float pi = 3.14159;
void main()
{
  vec3 scanlines_color = vec3(0);
  float v = 0.5 * (sin(texcoord.y * pi / width * u_resolution.y + phase) + 1.0);
  vec4 color = texture(u_texture, texcoord);
  color.rgb -= (scanlines_color - color.rgb) * (pow(v, thickness) - 1.0) * opacity;
  fragcolor = color;
}
)";

// Threshold fragment shader. Apply brighness threshold to texture.
// Reference: https://github.com/vrld/moonshine/blob/master/glow.lua#L45
static const char *s_frag_glow_threshold_src = R"(
#version 330 core

in vec2 texcoord;
out vec4 fragcolor;
uniform sampler2D u_texture;

const float min_luma = 0.4;
void main() {
  vec4 color = texture(u_texture, texcoord);
  float luma = dot(vec3(0.299, 0.587, 0.114), color.rgb);
  fragcolor = color * step(min_luma, luma);
}
)";

// Blue shader. Apply a blur to texture.
// Reference: https://github.com/Jam3/glsl-fast-gaussian-blur
static const char *s_frag_glow_blur_src = R"(
#version 330 core

in vec2 texcoord;
out vec4 fragcolor;
uniform sampler2D u_texture;
uniform vec2 u_resolution;
uniform vec2 u_direction;

void main() {
  vec4 color = vec4(0.0);
  vec2 off1 = (vec2(1.411764705882353) * u_direction) / u_resolution;
  vec2 off2 = (vec2(3.2941176470588234) * u_direction) / u_resolution;
  vec2 off3 = (vec2(5.176470588235294) * u_direction) / u_resolution;
  color += texture(u_texture, texcoord) * 0.1964825501511404;
  color += texture(u_texture, texcoord + off1) * 0.2969069646728344;
  color += texture(u_texture, texcoord - off1) * 0.2969069646728344;
  color += texture(u_texture, texcoord + off2) * 0.09447039785044732;
  color += texture(u_texture, texcoord - off2) * 0.09447039785044732;
  color += texture(u_texture, texcoord + off3) * 0.010381362401148057;
  color += texture(u_texture, texcoord - off3) * 0.010381362401148057;
  fragcolor = color;
}
)";

static const char *s_shader_frag_srcs[] = {
    s_frag_src,                // SHADER_NORMAL
    s_frag_vignette_src,       // SHADER_VIGNETTE
    s_frag_crt_src,            // SHADER_CRT
    s_frag_scanlines_src,      // SHADER_SCANLINES
    s_frag_glow_threshold_src, // SHADER_THRESHOLD
    s_frag_glow_blur_src       // SHADER_BLUR
};
static const char *s_shader_names[] = {
    "normal",    // SHADER_NORMAL
    "vignette",  // SHADER_VIGNETTE
    "crt",       // SHADER_CRT
    "scanline",  // SHADER_SCANLINES
    "threshold", // SHADER_THRESHOLD
    "blur"       // SHADER_BLUR
};

// OpenGL shaders implementation.
//

static int compile_shader(GLuint shader, const char *name, const char *src) {
  glShaderSource(shader, 1, &src, nullptr);
  glCompileShader(shader);

  GLint success = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    GLint length;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    char *info = (char *)malloc(length + 1);
    glGetShaderInfoLog(shader, length, nullptr, info);
    adc_log_error("Failed to compile '%s' shader:\n%s\n%s", name, src, info);
    free(info);
    return -1;
  }

  return 0;
}

static int compile_shader_program(OpenGLShader *shader_data, const char *name, const char *vert_src,
                                  const char *frag_src) {
  shader_data->program = glCreateProgram();

  // Compile the shaders.
  shader_data->vert_shader = glCreateShader(GL_VERTEX_SHADER);
  if (compile_shader(shader_data->vert_shader, name, s_vert_src) != 0) {
    return -1;
  }
  shader_data->frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
  if (compile_shader(shader_data->frag_shader, name, frag_src) != 0) {
    return -1;
  }

  // Bind the shaders and link program.
  glAttachShader(shader_data->program, shader_data->vert_shader);
  glAttachShader(shader_data->program, shader_data->frag_shader);
  glLinkProgram(shader_data->program);

  // Check for success and log any errors.
  GLint success = GL_FALSE;
  glGetProgramiv(shader_data->program, GL_LINK_STATUS, &success);
  if (!success) {
    GLint length;
    glGetProgramiv(shader_data->program, GL_INFO_LOG_LENGTH, &length);
    char *info = (char *)malloc(length + 1);
    glGetProgramInfoLog(shader_data->program, length, nullptr, info);
    adc_log_error("Failed to link '%s' shader:\n%s", name, info);
    free(info);
    return -1;
  }

  glUseProgram(shader_data->program);

  // Cache all the uniform locations.
  const GLsizei length = 64;
  GLint count;
  char uniform_name[length];
  GLint size;
  GLenum type;
  glGetProgramiv(shader_data->program, GL_ACTIVE_UNIFORMS, &count);
  for (int i = 0; i < count; i++) {
    glGetActiveUniform(shader_data->program, (GLuint)i, length, nullptr, &size, &type,
                       uniform_name);
    shader_data->uniform_locations[std::string(uniform_name)] =
        glGetUniformLocation(shader_data->program, uniform_name);
  }

  // Set the u_texture uniform.
  glUniform1i(shader_data->uniform_locations["u_texture"], 0);

  glUseProgram(0);

  return 0;
}

int opengl_shaders_setup(OpenGLShaderContext *ctx) {
  // Compile all the shader programs.
  //

  for (int i = 0; i < SHADER_MAX; i++) {
    Shader shader = (Shader)i;
    const char *name = s_shader_names[shader];
    const char *frag_src = s_shader_frag_srcs[shader];
    if (compile_shader_program(&ctx->shaders[i], name, s_vert_src, frag_src) != 0) {
      return -1;
    }
  }

  if (compile_shader_program(&ctx->colormap_shader, "colormap", s_vert_src, s_frag_colormap_src) !=
      0) {
    return -1;
  }
  // Set the u_colormap uniform.
  glUseProgram(ctx->colormap_shader.program);
  glUniform1i(ctx->colormap_shader.uniform_locations["u_colormap"], 1);
  glUseProgram(0);

  ctx->active_shader = SHADER_NORMAL;
  return 0;
}

void destroy_shader(OpenGLShader *shader_data) {
  glDeleteShader(shader_data->vert_shader);
  glDeleteShader(shader_data->frag_shader);
  glDeleteProgram(shader_data->program);
}

void opengl_shaders_shutdown(OpenGLShaderContext *ctx) {
  for (int i = 0; i < SHADER_MAX; i++) {
    destroy_shader(&ctx->shaders[(Shader)i]);
  }
  destroy_shader(&ctx->colormap_shader);
}

void opengl_shaders_set_shader(OpenGLShaderContext *ctx, Shader shader) {
  assert(shader >= SHADER_NORMAL && shader < SHADER_MAX);

  glUseProgram(ctx->shaders[shader].program);
  ctx->active_shader = shader;
}
