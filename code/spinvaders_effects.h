#ifndef _SPINVADERS_EFFECTS_H_
#define _SPINVADERS_EFFECTS_H_

// Space Invaders effects interface. Handles drawing more involved pixel shader effects.

struct Texture;

int effects_setup(int width, int height);

void effects_shutdown();

const Texture *effects_glow_draw(const Texture *scene);

const Texture *effects_crt_draw(const Texture *scene);

#endif // _SPINVADERS_EFFECTS_H_
