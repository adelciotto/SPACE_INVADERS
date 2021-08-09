#ifndef _SPINVADERS_H_
#define _SPINVADERS_H_

#include "spinvaders_shared.h"

// Space Invaders application service interface.
// The spinvaders service is responsible for:
// - Emulating the game.
// - Responding to input.
// - Submitting draw commands to renderer.
//
// The platform layer will consume this service to get the application running
// on the target plaform.

enum Button
{
  BUTTON_LEFT = 0,
  BUTTON_RIGHT,
  BUTTON_FIRE,
  BUTTON_START_1P,
  BUTTON_START_2P,
  BUTTON_INSERT_CREDIT,
  BUTTON_TILT
};

#define BUTTON_DOWN(input, btn) (((input)->buttons >> (btn)) & 1)

struct InputState {
  uint32_t buttons;
};

int spinvaders_setup();

void spinvaders_shutdown();

void spinvaders_tick(const InputState *input);

bool spinvaders_paused();

void spinvaders_set_pause(bool pause);

void spinvaders_draw();

void spinvaders_resize(int device_width, int device_height);

#endif // _SPINVADERS_H_
