#ifndef _SPINVADERS_RENDERER_H_
#define _SPINVADERS_RENDERER_H_

// Space Invaders renderer interface. The renderer is responsible for:
// - Managing textures.
// - Drawing textures to screen.
// - Post processing effects.

enum Shader
{
  SHADER_NORMAL = 0,
  SHADER_VIGNETTE,
  SHADER_CRT,
  SHADER_SCANLINES,
  SHADER_GLOW_THRESHOLD,
  SHADER_GLOW_BLUR,
  SHADER_MAX
};

enum BlendMode
{
  BLEND_ADD,
  BLEND_ALPHA
};

enum TextureType
{
  TEXTURE_TYPE_STATIC_IMAGE = 0,
  TEXTURE_TYPE_PIXEL_ACCESS,
  TEXTURE_TYPE_DRAWTARGET
};

enum TextureFilter
{
  TEXTURE_FILTER_NEAREST = 0,
  TEXTURE_FILTER_LINEAR
};

struct TextureParams {
  TextureType type;
  TextureFilter min_filter;
  TextureFilter mag_filter;

  TextureParams(TextureType type = TEXTURE_TYPE_STATIC_IMAGE,
                TextureFilter min = TEXTURE_FILTER_LINEAR,
                TextureFilter mag = TEXTURE_FILTER_NEAREST)
      : type(type), min_filter(min), mag_filter(mag) {
  }
};

struct BackendData;

struct Texture {
  TextureParams params;
  int width;
  int height;
  int pitch;
  BackendData *backend_data;

  bool active() const {
    return backend_data != nullptr;
  }
};

struct Rect {
  float x;
  float y;
  float w;
  float h;
};

//
// renderer_setup()
//
// Description: Setup the renderer.
// Returns 0 on success, -1 on failure.
//
int renderer_setup();

//
// renderer_shutdown()
//
// Description: Shutdown the renderer and free all associated resources.
//
void renderer_shutdown();

//
// renderer_clear()
//
// Description: Clear the current draw target with the given rgba color.
//
void renderer_clear(float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 0.0f);

//
// renderer_resize()
//
// Description: Resize the renderer viewport. To be called when the window is resized.
//
void renderer_resize(float width, float height);

//
// renderer_create_texture()
//
// Description: Create a new static texture with the given dimensions and pixel data.
// Initializes the given texture handle.
// Returns 0 on success, -1 on failure.
//
int renderer_create_texture(Texture *texture, int width, int height, void *pixels = nullptr,
                            TextureParams = {});

//
// renderer_destroy_texture()
//
// Description: Destroys the given texture resources.
//
void renderer_destroy_texture(Texture *texture);

//
// renderer_set_draw_target()
//
// Description: Set the current draw target to the given texture.
// All draw calls will be performed onto this given target.
// The draw target will be reset if nullptr is given.
//
void renderer_set_draw_target(Texture *texture);

//
// renderer_set_shader()
//
// Description: Set the current pixel shader to use when drawing textures.
// Defaults to SHADER_NORMAL.
//
void renderer_set_shader(Shader shader = SHADER_NORMAL);

//
// renderer_update_shader_uniform1f()
//
// Description: Update the currently bound shader uniform with single float value.
//
void renderer_update_shader_uniform1f(const char *uniform, float v1);

//
// renderer_update_shader_uniform2f()
//
// Description: Update the current bound shader uniform with two float values.
//
void renderer_update_shader_uniform2f(const char *uniform, float v1, float v2);

//
// renderer_draw_texture()
//
// Description: Draw a texture to the screen.
// destrect - The destination Rect structure or nullptr for the entire rendering target; the texture
// will be stretched to fit the given rectangle.
// angledeg - The angle in degrees to rotate the destrect in a clockwise direction.
//
void renderer_draw_texture(const Texture *texture, const Rect *destrect = nullptr,
                           float angledeg = 0.0f);

//
// renderer_draw_texture_with_colormap()
//
// Description: Draw a texture to the screen with a colormap.
// colormap - Color map texture to sample from to determine colors in drawn texture.
//
void renderer_draw_texture_with_colormap(const Texture *texture, const Texture *colormap);

//
// renderer_update_texture()
//
// Description: Update a textures pixels or dimensions.
//
void renderer_update_texture(Texture *texture, void *pixels);

//
// renderer_set_blend_mode()
// Description: Set the alpha blend mode.
//
void renderer_set_blend_mode(BlendMode mode);

//
// renderer_get_max_texture_size()
//
// Description: Get the maximum texture size supported by the renderer.
//
void renderer_get_max_texture_size(int *w, int *h);

#endif // _SPINVADERS_RENDERER_H_
