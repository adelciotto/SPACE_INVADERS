#ifndef _SPINVADERS_MACHINE_H_
#define _SPINVADERS_MACHINE_H_

struct InputState;
struct Texture;

int machine_setup();

void machine_shutdown();

void machine_tick(const InputState *input);

bool machine_paused();

void machine_set_pause(bool pause);

const Texture *machine_get_display_texture();

#endif // _SPINVADERS_MACHINE_H_
