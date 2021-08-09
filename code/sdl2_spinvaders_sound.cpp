#include "spinvaders_sound.h"

#include <SDL_mixer.h>

#include "spinvaders_shared.h"

struct SDL2SoundData {
  Sound id;
  const char *filepath;
  Mix_Chunk *chunk;
  int channel;
};

struct SDL2SoundContext {
  SDL2SoundData sounds[SOUND_COUNT];
};

static const char *s_sound_filenames[] = {
    "0.wav",         // SOUND_UFO
    "8.wav",         // SOUND_UFO_HIT
    "1.wav",         // SOUND_FIRE
    "2.wav",         // SOUND_EXPLOSION_DIE
    "3.wav",         // SOUND_INVADER_DIE
    "4.wav",         // SOUND_FLEET_MOVE_1
    "5.wav",         // SOUND_FLEET_MOVE_2
    "6.wav",         // SOUND_FLEET_MOVE_3
    "7.wav",         // SOUND_FLEET_MOVE_4
    "insertcoin.wav" // SOUND_COIN_INSERTED
};

static SDL2SoundContext s_ctx = {};

// SDL2SoundData implementation
//

static int load_sound(Sound id) {
  const char *filename = s_sound_filenames[id];
  assert(filename);

  char filepath[1024];
  snprintf(filepath, 1024, "data/%s", filename);

  s_ctx.sounds[id].chunk = Mix_LoadWAV(filepath);
  if (!s_ctx.sounds[id].chunk) {
    adc_log_error("Failed to load sound %s! %s", filepath, Mix_GetError());
    return -1;
  }
  s_ctx.sounds[id].id = id;
  return 0;
}

int sound_setup() {
  if (Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 1024) != 0) {
    adc_log_error("Failed to init SDL_mixer! %s", Mix_GetError());
    return -1;
  }
  Mix_AllocateChannels(16);

  for (int i = 0; i < SOUND_COUNT; i++) {
    if (load_sound((Sound)i) != 0) {
      return -1;
    }
  }
  return 0;
}

void sound_destroy() {
  for (int i = 0; i < SOUND_COUNT; i++) {
    if (s_ctx.sounds[i].chunk) {
      Mix_FreeChunk(s_ctx.sounds[i].chunk);
    }
  }
}

void sound_play(Sound id, bool loop) {
  assert(id >= 0 && id < SOUND_COUNT);

  SDL2SoundData *snd = &s_ctx.sounds[id];
  int loops = loop ? -1 : 0;
  snd->channel = Mix_PlayChannel(-1, snd->chunk, loops);
}

void sound_stop(Sound id) {
  assert(id >= 0 && id < SOUND_COUNT);

  SDL2SoundData *snd = &s_ctx.sounds[id];
  Mix_HaltChannel(snd->channel);
  snd->channel = -1;
}
