#ifndef _SPINVADERS_SOUND_H_
#define _SPINVADERS_SOUND_H_

enum Sound
{
  SOUND_UFO = 0,
  SOUND_UFO_HIT,
  SOUND_FIRE,
  SOUND_EXPLOSION,
  SOUND_INVADER_DIE,
  SOUND_FLEET_MOVE_1,
  SOUND_FLEET_MOVE_2,
  SOUND_FLEET_MOVE_3,
  SOUND_FLEET_MOVE_4,
  SOUND_COIN_INSERTED,
  SOUND_COUNT
};

//
// sound_setup()
//
// Description: Setup the sound player.
// Returns 0 on success, -1 on failure.
//
int sound_setup();

//
// sound_shutdown()
//
// Description: Shutdown the sound player.
//
void sound_shutdown();

//
// sound_play()
//
// Description: Play the given sound once.
// loop - If true will forever loop the sound.
//
void sound_play(Sound id, bool loop = false);

//
// sound_stop()
//
// Description: Stop the given sound.
//
void sound_stop(Sound id);

#endif // _SPINVADERS_SOUND_H_
