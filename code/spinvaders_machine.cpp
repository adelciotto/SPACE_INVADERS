#include "spinvaders_machine.h"

#include "lib/adc_8080_cpu.h"

#include "spinvaders.h"
#include "spinvaders_renderer.h"
#include "spinvaders_shared.h"
#include "spinvaders_sound.h"

#define CYCLES_HZ 2000000UL
#define CYCLES_PER_TICK CYCLES_HZ / 60
#define CYCLES_PER_SCANLINE CYCLES_PER_TICK / 224
#define CYCLES_HVBLANK CYCLES_PER_SCANLINE * 112
#define CYCLES_VBLANK CYCLES_PER_SCANLINE * 224

#define MEMORY_SIZE 0x4000
#define MEMORY_WORK_RAM_START 0x2000
#define MEMORY_VIDEO_RAM_START 0x2400
#define MEMORY_MIRROR_RAM_START 0x4000
#define MEMORY_MIRROR_RAM_END 0x5FFF

#define ROM_SIZE 0x0800

#define DIP_SHIPS_3 0x00
#define DIP_SHIPS_4 0x01
#define DIP_SHIPS_5 0x02
#define DIP_SHIPS_6 0x03

struct Processor {
  adc_8080_cpu cpu;
  uint64_t cycles_this_tick;
  bool hvblank_triggered;
  bool vblank_triggered;
};

struct ShiftRegister {
  uint8_t low;
  uint8_t high;
  uint8_t offset;
};

struct Display {
  const int width = 256;
  const int height = 224;
  uint32_t *pixels;
  Texture texture;
};

struct Machine {
  Processor processor;
  uint8_t *memory;
  ShiftRegister shift_register;
  Display display;
  const InputState *input;
  uint8_t device1_last_read;
  uint8_t device3_last_write;
  uint8_t device5_last_write;
  bool paused;
  // Dip switch settings.
  uint8_t dip_ships;
  bool dip_extra_ship;
  bool dip_display_coin;
};

static Machine s_machine = {};

// CPU and display handlers
//

static uint8_t handle_memory_read(void *userdata, uint16_t addr);
static void handle_memory_write(void *userdata, uint16_t addr, uint8_t value);
static uint8_t handle_device_read(void *userdata, uint8_t device);
static void handle_device_write(void *userdata, uint8_t device, uint8_t output);

static void handle_vsync();

// Rom helpers
//

static int load_roms();
static int load_rom(const char *filepath, uint16_t addr);

// Machine implementation
//

int machine_setup() {
  // Setup the memory and load roms.
  s_machine.memory = (uint8_t *)calloc(MEMORY_SIZE, 1);
  if (!s_machine.memory) {
    adc_log_error("Failed to malloc() machine memory!");
    return -1;
  }
  if (load_roms() != 0) {
    adc_log_error("Failed to load roms into machine memory!");
    return -1;
  }

  // Default dip switch values.
  s_machine.dip_ships = DIP_SHIPS_3;
  s_machine.dip_extra_ship = 0;
  s_machine.dip_display_coin = 0;

  // Setup the 8080 processor.
  Processor *processor = &s_machine.processor;
  adc_8080_cpu_init(&processor->cpu);
  processor->cpu.read_byte = handle_memory_read;
  processor->cpu.write_byte = handle_memory_write;
  processor->cpu.read_device = handle_device_read;
  processor->cpu.write_device = handle_device_write;

  // Setup the display.
  Display *display = &s_machine.display;
  display->pixels = (uint32_t *)calloc(display->width * display->height, 4);
  if (!display->pixels) {
    adc_log_error("Failed to malloc() machine display stencil pixels!");
    return -1;
  }
  TextureParams params = {TEXTURE_TYPE_PIXEL_ACCESS};
  renderer_create_texture(&display->texture, display->width, display->height, display->pixels,
                          params);

  return 0;
}

void machine_shutdown() {
  Display *display = &s_machine.display;
  if (display->pixels) {
    free(display->pixels);
  }
  renderer_destroy_texture(&display->texture);

  if (s_machine.memory) {
    free(s_machine.memory);
  }
}

void machine_tick(const InputState *input) {
  assert(input);

  if (s_machine.paused) {
    return;
  }

  s_machine.input = input;

  // Execute correct number of cycles per tick.
  Processor *processor = &s_machine.processor;
  while (processor->cycles_this_tick <= CYCLES_PER_TICK) {
    uint64_t cycles = adc_8080_cpu_step(&processor->cpu);
    processor->cycles_this_tick += cycles;

    // Send cpu the middle and end vblank interrupts.
    if (processor->cycles_this_tick >= CYCLES_HVBLANK && !processor->hvblank_triggered) {
      adc_8080_cpu_interrupt(&processor->cpu, 0xCF);
      processor->hvblank_triggered = true;
    }
    if (processor->cycles_this_tick >= CYCLES_VBLANK && !processor->vblank_triggered) {
      adc_8080_cpu_interrupt(&processor->cpu, 0xD7);
      handle_vsync();
      processor->vblank_triggered = true;
    }
  }
  // Adjust the amount of cycles run next tick if we exceeded the maximum.
  if (processor->cycles_this_tick >= CYCLES_PER_TICK) {
    processor->cycles_this_tick -= CYCLES_PER_TICK;
  }
  processor->hvblank_triggered = false;
  processor->vblank_triggered = false;
}

bool machine_paused() {
  return s_machine.paused;
}

void machine_set_pause(bool pause) {
  s_machine.paused = pause;
}

const Texture *machine_get_display_texture() {
  return &s_machine.display.texture;
}

// CPU handlers implementation
//

static uint8_t handle_memory_read(void *userdata, uint16_t addr) {
  if (addr > MEMORY_MIRROR_RAM_END) {
    adc_log_warn("Attempt to read beyond memory bounds!");
    return 0;
  }

  if (addr >= MEMORY_MIRROR_RAM_START) {
    addr -= 0x2000;
  }

  return s_machine.memory[addr];
}

static void handle_memory_write(void *userdata, uint16_t addr, uint8_t value) {
  if (addr > MEMORY_MIRROR_RAM_END) {
    adc_log_warn("Attempt to write beyond memory bounds!");
    return;
  }

  if (addr < MEMORY_WORK_RAM_START) {
    adc_log_warn("Attempt to write to rom memory!!");
    return;
  }

  if (addr >= MEMORY_MIRROR_RAM_START) {
    addr -= 0x2000;
  }

  s_machine.memory[addr] = value;
}

static uint8_t handle_device_read(void *userdata, uint8_t device) {
  // Read controls.
  //

  if (device == 0) {
    return 0x70; // 0b01110000;
  }

  const InputState *input = s_machine.input;
  if (device == 1) {
    uint8_t res = 0;
    res |= BUTTON_DOWN(input, BUTTON_INSERT_CREDIT) << 0;
    res |= BUTTON_DOWN(input, BUTTON_START_2P) << 1;
    res |= BUTTON_DOWN(input, BUTTON_START_1P) << 2;
    res |= 1 << 3;
    res |= BUTTON_DOWN(input, BUTTON_FIRE) << 4;
    res |= BUTTON_DOWN(input, BUTTON_LEFT) << 5;
    res |= BUTTON_DOWN(input, BUTTON_RIGHT) << 6;

    if (BUTTON_DOWN(input, BUTTON_INSERT_CREDIT) && !((s_machine.device1_last_read >> 0) & 1)) {
      sound_play(SOUND_COIN_INSERTED);
    }

    s_machine.device1_last_read = res;
    return res;
  }
  if (device == 2) {
    uint8_t res = 0;
    res |= s_machine.dip_ships;
    res |= BUTTON_DOWN(input, BUTTON_TILT) << 2;
    res |= s_machine.dip_extra_ship << 3;
    res |= BUTTON_DOWN(input, BUTTON_FIRE) << 4;
    res |= BUTTON_DOWN(input, BUTTON_LEFT) << 5;
    res |= BUTTON_DOWN(input, BUTTON_RIGHT) << 6;
    res |= s_machine.dip_display_coin << 7;
    return res;
  }

  // Read the 8-bit result from the shift register.
  if (device == 3) {
    ShiftRegister *shiftreg = &s_machine.shift_register;
    uint16_t shift_word = (uint16_t)((shiftreg->high << 8) | shiftreg->low);
    return (shift_word >> (8 - shiftreg->offset)) & 0xFF;
  }

  return 0;
}

static void handle_device_write(void *userdata, uint8_t device, uint8_t output) {
  // Write to shift register.
  //

  ShiftRegister *shiftreg = &s_machine.shift_register;
  // Shift high into low, and the new value into high.
  if (device == 4) {
    shiftreg->low = shiftreg->high;
    shiftreg->high = output;
  }
  // Set the offset for the 8-bit result.
  else if (device == 2) {
    shiftreg->offset = output & 0x07;
  }

  // Handle sounds.
  // Play a sound if the bit flag is toggled to 1.
  //

#define on(v, b) (((v) & (1 << (b))) != 0)
#define off(v, b) !on(v, b)
  else if (device == 3) {
    uint8_t last_write = s_machine.device3_last_write;
    if (output != last_write) {
      if (on(output, 0) && off(last_write, 0)) {
        sound_play(SOUND_UFO, true);
      }
      if (off(output, 0) && on(last_write, 0)) {
        sound_stop(SOUND_UFO);
      }
      if (on(output, 1) && off(last_write, 1)) {
        sound_play(SOUND_FIRE);
      }
      if (on(output, 2) && off(last_write, 2)) {
        sound_play(SOUND_EXPLOSION);
      }
      if (on(output, 3) && off(last_write, 3)) {
        sound_play(SOUND_INVADER_DIE);
      }
      s_machine.device3_last_write = output;
    }
  } else if (device == 5) {
    uint8_t last_write = s_machine.device5_last_write;
    if (output != last_write) {
      if (on(output, 0) && off(last_write, 0)) {
        sound_play(SOUND_FLEET_MOVE_1);
      }
      if (on(output, 1) && off(last_write, 1)) {
        sound_play(SOUND_FLEET_MOVE_2);
      }
      if (on(output, 2) && off(last_write, 2)) {
        sound_play(SOUND_FLEET_MOVE_3);
      }
      if (on(output, 3) && off(last_write, 3)) {
        sound_play(SOUND_FLEET_MOVE_4);
      }
      if (on(output, 4) && off(last_write, 4)) {
        sound_play(SOUND_UFO_HIT);
      }
      s_machine.device5_last_write = output;
    }
  }
#undef on
#undef off
}

static void handle_vsync() {
  uint8_t *vram = &s_machine.memory[MEMORY_VIDEO_RAM_START];
  Display *display = &s_machine.display;
  uint32_t *pixels = display->pixels;
  int w = display->width;
  int h = display->height;

  // Update the pixels with vram framebuffer.
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x += 8) {
      unsigned int idx = y * w + x;
      uint8_t dispbit = vram[idx >> 3];
      for (int i = 0; i < 8; i++) {
        if (dispbit & (0x80 >> i)) {
          pixels[idx + (7 - i)] = 0xFFFFFFFF;
        } else {
          pixels[idx + (7 - i)] = 0;
        }
      }
    }
  }

  // Update the texture with new pixels.
  renderer_update_texture(&display->texture, pixels);
}

// Rom helpers implementation
//

static int load_roms() {
  // Load the space invaders roms into the correct parts of memory.
  // invaders.h 0x0000 - 0x07FF
  // invaders.g 0x0800 - 0x0FFF
  // invaders.f 0x1000 - 0x17FF
  // invaders.e 0x1800 - 0x1FFF
  if (load_rom("data/invaders.h", 0x0000) != 0) {
    return -1;
  }
  if (load_rom("data/invaders.g", 0x0800) != 0) {
    return -1;
  }
  if (load_rom("data/invaders.f", 0x1000) != 0) {
    return -1;
  }
  if (load_rom("data/invaders.e", 0x1800) != 0) {
    return -1;
  }
  return 0;
}

static int load_rom(const char *filepath, uint16_t addr) {
  assert(filepath);
  assert(addr < MEMORY_SIZE);

  FILE *file = fopen(filepath, "rb");
  if (!file) {
    adc_log_error("Failed to fopen() the rom file at %s!\n", filepath);
    return -1;
  }
  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  rewind(file);

  assert(addr + size < MEMORY_SIZE);

  if (size != ROM_SIZE) {
    adc_log_error("Rom %s size is incorrect! Expected %d, got %ld\n", filepath, ROM_SIZE, size);
    fclose(file);
    return -1;
  }

  size_t bytes_read = fread(s_machine.memory + addr, 1, size, file);
  if (bytes_read != size) {
    fprintf(stderr,
            "Failed to read the rom file into memory! Read %zu "
            "bytes, total is %zu bytes\n",
            bytes_read, size);
    fclose(file);
    return -1;
  }

  fclose(file);
  return 0;
}
