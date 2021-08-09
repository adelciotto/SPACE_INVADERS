// adc_8080_cpu Intel 8080 CPU emulation library by Anthony Del Ciotto.
//
// MIT License

// Copyright (c) 2021 Anthony Del Ciotto

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "adc_8080_cpu.h"

#include <assert.h>   // For assert
#include <inttypes.h> // For PRIu8, PRIu16, etc

// LUTs

// clang-format off
static int s_cycles_lut[256] = {
//	 x0  x1  x2  x3  x4  x5  x6  x7  x8  x9  xA  xB  xC  xD  xE  xF
/*x0*/   4,  10, 7,  5,  5,  5,  7,  4,  4,  10, 7,  5,  5,  5,  7,  4,
/*1x*/   4,  10, 7,  5,  5,  5,  7,  4,  4,  10, 7,  5,  5,  5,  7,  4,
/*2x*/   4,  10, 16, 5,  5,  5,  7,  4,  4,  10, 16, 5,  5,  5,  7,  4,
/*3x*/   4,  10, 13, 5,  10, 10, 10, 4,  4,  10, 13, 5,  5,  5,  7,  4,
/*4x*/   5,  5,  5,  5,  5,  5,  7,  5,  5,  5,  5,  5,  5,  5,  7,  5,
/*5x*/   5,  5,  5,  5,  5,  5,  7,  5,  5,  5,  5,  5,  5,  5,  7,  5,
/*6x*/   5,  5,  5,  5,  5,  5,  7,  5,  5,  5,  5,  5,  5,  5,  7,  5,
/*7x*/   7,  7,  7,  7,  7,  7,  7,  7,  5,  5,  5,  5,  5,  5,  7,  5,
/*8x*/   4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
/*9x*/   4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
/*Ax*/   4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
/*Bx*/   4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
/*Cx*/   5,  10, 10, 10, 11, 11, 7,  11, 5,  10, 10, 10, 11, 17, 7, 11,
/*Dx*/   5,  10, 10, 10, 11, 11, 7,  11, 5,  10, 10, 10, 11, 17, 7, 11,
/*Ex*/   5,  10, 10, 18, 11, 11, 7,  11, 5,  5,  10, 4,  11, 17, 7, 11,
/*Fx*/   5,  10, 10, 4,  11, 11, 7,  11, 5,  5,  10, 4,  11, 17, 7, 11
};
// clang-format on

static bool s_parity_lut[256];

// Helper functions and macros

static inline uint16_t word_from_bytes(uint8_t high, uint8_t low) {
  return (uint16_t)((high << 8) | low);
}

static inline void bytes_from_word(uint8_t *high, uint8_t *low, uint16_t w) {
  *high = w >> 8;
  *low = w & 0xFF;
}

static inline uint8_t read_byte(adc_8080_cpu *cpu, uint16_t addr) {
  return cpu->read_byte(cpu->userdata, addr);
}

static inline uint16_t read_word(adc_8080_cpu *cpu, uint16_t addr) {
  return word_from_bytes(cpu->read_byte(cpu->userdata, addr + 1),
                         cpu->read_byte(cpu->userdata, addr));
}

static inline void write_byte(adc_8080_cpu *cpu, uint16_t addr, uint8_t b) {
  cpu->write_byte(cpu->userdata, addr, b);
}

static inline void write_word(adc_8080_cpu *cpu, uint16_t addr, uint16_t w) {
  cpu->write_byte(cpu->userdata, addr, w & 0xFF);
  cpu->write_byte(cpu->userdata, addr + 1, w >> 8);
}

static inline uint8_t next_byte(adc_8080_cpu *cpu) {
  return read_byte(cpu, cpu->pc++);
}

static inline uint16_t next_word(adc_8080_cpu *cpu) {
  uint16_t w = read_word(cpu, cpu->pc);
  cpu->pc += 2;
  return w;
}

// Internal interface

static void exec_next(adc_8080_cpu *cpu, uint8_t opcode);

// Public api implementation

void adc_8080_cpu_init(adc_8080_cpu *cpu) {
  assert(cpu);

  // Init the cpu state to all zero values.
  cpu->ra = 0, cpu->rb = 0, cpu->rc = 0, cpu->rd = 0, cpu->re = 0, cpu->rh = 0,
  cpu->rl = 0;
  cpu->pc = 0;
  cpu->sp = 0;
  cpu->cfs = 0, cpu->cfz = 0, cpu->cfa = 0, cpu->cfp = 0, cpu->cfc = 0;
  cpu->halted = false;
  cpu->interrupt_pending = false;
  cpu->interrupt_opcode = 0x00;
  cpu->interrupt_delay = false;
  cpu->cycles = 0;
  cpu->userdata = NULL;
  cpu->read_byte = NULL;
  cpu->write_byte = NULL;
  cpu->read_device = NULL;
  cpu->write_device = NULL;

  // Populate the parity bit LUT.
  for (int i = 0; i < 256; i++) {
    int ones = 0;
    for (int b = 0; b < 8; b++)
      if (((i >> b) & 1) == 1)
        ones++;
    s_parity_lut[i] = (ones % 2) == 0;
  }
}

int adc_8080_cpu_step(adc_8080_cpu *cpu) {
  assert(cpu);
  assert(cpu->read_byte);
  assert(cpu->write_byte);
  assert(cpu->read_device);
  assert(cpu->write_device);

  // Recognize a interrupt request when all of the following
  // conditions are met:
  // - There is an interrupt pending.
  // - The INTE flip-flop is enabled.
  // - The last instruction being executed has complete.
  if (cpu->interrupt_pending && cpu->inte && !cpu->interrupt_delay) {
    // The following states are reset once an interrupt
    // request is recognized.
    cpu->interrupt_pending = false;
    cpu->inte = false;
    cpu->halted = false;

    // The pc is not incremented here because interrupt
    // opcodes are not read from memory.
    exec_next(cpu, cpu->interrupt_opcode);
  } else if (!cpu->halted) {
    exec_next(cpu, next_byte(cpu));
  }

  // Reset the cycle count and return the consumed cycles this step.
  int cycles = cpu->cycles;
  cpu->cycles = 0;
  return cycles;
}

void adc_8080_cpu_interrupt(adc_8080_cpu *cpu, uint8_t opcode) {
  assert(cpu);

  cpu->interrupt_pending = true;
  cpu->interrupt_opcode = opcode;
}

#define get_rbc() word_from_bytes(cpu->rb, cpu->rc)
#define get_rde() word_from_bytes(cpu->rd, cpu->re)
#define get_rhl() word_from_bytes(cpu->rh, cpu->rl)

void adc_8080_cpu_print(adc_8080_cpu *cpu, FILE *stream) {
#define u8 "0x%02" PRIx8
#define u16 "0x%04" PRIx16
  assert(cpu);
  assert(stream);

  fprintf(stream,
          "a:" u8 ", b:" u8 ", d:" u8 ", d:" u8 ", e:" u8 ", h:" u8 ", l:" u8
          "\n"
          "bc:" u16 ", de:" u16 ", hl:" u16 "\n"
          "pc:" u16 ", sp:" u16 "\n"
          "cfs:%d, cfz:%d, cfa:%d, cfp:%d, cfc:%d\n"
          "inte:%d, interrupt_pending:%d, interrupt_opcode:" u8 "\n"
          "halted: %d\n",
          cpu->ra, cpu->rb, cpu->rc, cpu->rd, cpu->re, cpu->rh, cpu->rl,
          get_rbc(), get_rde(), get_rhl(), cpu->pc, cpu->sp, cpu->cfs, cpu->cfz,
          cpu->cfa, cpu->cfp, cpu->cfc, cpu->inte, cpu->interrupt_pending,
          cpu->interrupt_opcode, cpu->halted);
#undef u8
#undef u16
}

// Internal implementation and helper macros

#define set_rbc(w) bytes_from_word(&cpu->rb, &cpu->rc, w)
#define set_rde(w) bytes_from_word(&cpu->rd, &cpu->re, w)
#define set_rhl(w) bytes_from_word(&cpu->rh, &cpu->rl, w)

#define set_cf_zsp(v)                                                          \
  {                                                                            \
    cpu->cfs = (v) >> 7;                                                       \
    cpu->cfz = (v) == 0;                                                       \
    cpu->cfp = s_parity_lut[(v)];                                              \
  }

static inline void stack_push(adc_8080_cpu *cpu, uint16_t w) {
  cpu->sp -= 2;
  write_word(cpu, cpu->sp, w);
}

static inline uint16_t stack_pop(adc_8080_cpu *cpu) {
  uint16_t w = read_word(cpu, cpu->sp);
  cpu->sp += 2;
  return w;
}

static inline uint8_t op_inr(adc_8080_cpu *cpu, uint8_t val) {
  uint8_t res = val + 1;
  cpu->cfa = (res & 0x0F) == 0;
  set_cf_zsp(res);
  return res;
}

static inline uint8_t op_dcr(adc_8080_cpu *cpu, uint8_t val) {
  uint8_t res = val - 1;
  cpu->cfa = !((res & 0x0F) == 0x0F);
  set_cf_zsp(res);
  return res;
}

static inline void op_add(adc_8080_cpu *cpu, uint8_t val, bool c) {
  uint8_t res = cpu->ra + val + c;
  int16_t sres = cpu->ra + val + c;
  int16_t carry = sres ^ cpu->ra ^ val;
  cpu->cfc = carry & (1 << 8) ? 1 : 0;
  cpu->cfa = carry & (1 << 4) ? 1 : 0;
  set_cf_zsp(res);
  cpu->ra = res;
}

static inline void op_sub(adc_8080_cpu *cpu, uint8_t val, bool c) {
  op_add(cpu, ~val, !c);
  cpu->cfc = !cpu->cfc;
}

static inline void op_ana(adc_8080_cpu *cpu, uint8_t val) {
  uint8_t result = cpu->ra & val;
  cpu->cfc = 0;
  cpu->cfa = ((cpu->ra | val) & 0x08) != 0;
  set_cf_zsp(result);
  cpu->ra = result;
}

static inline void op_xra(adc_8080_cpu *cpu, uint8_t val) {
  cpu->ra = cpu->ra ^ val;
  cpu->cfc = 0;
  cpu->cfa = 0;
  set_cf_zsp(cpu->ra);
}

static inline void op_ora(adc_8080_cpu *cpu, uint8_t val) {
  cpu->ra = cpu->ra | val;
  cpu->cfc = 0;
  cpu->cfa = 0;
  set_cf_zsp(cpu->ra);
}

static inline void op_cmp(adc_8080_cpu *cpu, uint8_t val) {
  int16_t res = cpu->ra - val;
  cpu->cfc = res >> 8;
  cpu->cfa = ~(cpu->ra ^ res ^ val) & 0x10;
  set_cf_zsp(res & 0xFF);
}

static inline void op_jmp_cond(adc_8080_cpu *cpu, uint16_t addr,
                               bool condition) {
  if (condition)
    cpu->pc = addr;
}

static inline void op_call(adc_8080_cpu *cpu, uint16_t addr) {
  stack_push(cpu, cpu->pc);
  cpu->pc = addr;
}

static inline void op_call_cond(adc_8080_cpu *cpu, uint16_t addr,
                                bool condition) {
  if (condition) {
    op_call(cpu, addr);
    cpu->cycles += 6;
  }
}

static inline void op_ret_cond(adc_8080_cpu *cpu, bool condition) {
  if (condition) {
    cpu->pc = stack_pop(cpu);
    cpu->cycles += 6;
  }
}

static inline void op_dad(adc_8080_cpu *cpu, uint16_t val) {
  uint32_t res = get_rhl() + val;
  cpu->cfc = res > 0xFFFF;
  set_rhl(res & 0xFFFF);
}

static inline void op_xchg(adc_8080_cpu *cpu) {
  uint16_t tmp = get_rhl();
  set_rhl(get_rde());
  set_rde(tmp);
}

static inline void op_xthl(adc_8080_cpu *cpu) {
  uint16_t val = read_word(cpu, cpu->sp);
  write_word(cpu, cpu->sp, get_rhl());
  set_rhl(val);
}

static inline void op_rlc(adc_8080_cpu *cpu) {
  cpu->cfc = cpu->ra >> 7;
  cpu->ra = (cpu->ra << 1) | cpu->cfc;
}

static inline void op_rrc(adc_8080_cpu *cpu) {
  cpu->cfc = cpu->ra & 1;
  cpu->ra = (cpu->ra >> 1) | (cpu->cfc << 7);
}

static inline void op_ral(adc_8080_cpu *cpu) {
  bool carrybit = cpu->cfc;
  cpu->cfc = cpu->ra >> 7;
  cpu->ra = (cpu->ra << 1) | carrybit;
}

static inline void op_rar(adc_8080_cpu *cpu) {
  bool carrybit = cpu->cfc;
  cpu->cfc = cpu->ra & 1;
  cpu->ra = (cpu->ra >> 1) | (carrybit << 7);
}

static void op_push_psw(adc_8080_cpu *cpu) {
  uint8_t psw = 0;
  psw |= cpu->cfs << 7;
  psw |= cpu->cfz << 6;
  psw |= cpu->cfa << 4;
  psw |= cpu->cfp << 2;
  psw |= 1 << 1;
  psw |= cpu->cfc << 0;

  stack_push(cpu, word_from_bytes(cpu->ra, psw));
}

static void op_pop_psw(adc_8080_cpu *cpu) {
  uint8_t a, psw;
  bytes_from_word(&a, &psw, stack_pop(cpu));

  cpu->ra = a;
  cpu->cfs = (psw >> 7) & 1;
  cpu->cfz = (psw >> 6) & 1;
  cpu->cfa = (psw >> 4) & 1;
  cpu->cfp = (psw >> 2) & 1;
  cpu->cfc = (psw >> 0) & 1;
}

static void op_daa(adc_8080_cpu *cpu) {
  uint8_t lownib = cpu->ra & 0x0F;
  uint8_t highnib = cpu->ra >> 4;
  bool carrybit = cpu->cfc;
  uint8_t addition = 0;

  if (lownib > 9 || cpu->cfa) {
    addition += 0x06;
  }

  if (highnib > 9 || cpu->cfc || (highnib >= 9 && lownib > 9)) {
    addition += 0x60;
    carrybit = 1;
  }

  op_add(cpu, addition, 0);
  cpu->cfc = carrybit;
}

static void exec_next(adc_8080_cpu *cpu, uint8_t opcode) {
  cpu->cycles = s_cycles_lut[opcode];

  if (cpu->interrupt_delay)
    cpu->interrupt_delay = false;

  switch (opcode) {
  // Carry bit ops
  case 0x37: // STC
    cpu->cfc = 1;
    break;
  case 0x3F: // CMC
    cpu->cfc = !cpu->cfc;
    break;

  // Single register ops
  case 0x04: // INR B
    cpu->rb = op_inr(cpu, cpu->rb);
    break;
  case 0x05: // DCR B
    cpu->rb = op_dcr(cpu, cpu->rb);
    break;
  case 0x0C: // INR C
    cpu->rc = op_inr(cpu, cpu->rc);
    break;
  case 0x0D: // DCR C
    cpu->rc = op_dcr(cpu, cpu->rc);
    break;
  case 0x14: // INR D
    cpu->rd = op_inr(cpu, cpu->rd);
    break;
  case 0x15: // DCR D
    cpu->rd = op_dcr(cpu, cpu->rd);
    break;
  case 0x1C: // INR E
    cpu->re = op_inr(cpu, cpu->re);
    break;
  case 0x1D: // DCR E
    cpu->re = op_dcr(cpu, cpu->re);
    break;
  case 0x24: // INR H
    cpu->rh = op_inr(cpu, cpu->rh);
    break;
  case 0x25: // DCR H
    cpu->rh = op_dcr(cpu, cpu->rh);
    break;
  case 0x2C: // INR L
    cpu->rl = op_inr(cpu, cpu->rl);
    break;
  case 0x2D: // DCR L
    cpu->rl = op_dcr(cpu, cpu->rl);
    break;
  case 0x34: // INR M
    write_byte(cpu, get_rhl(), op_inr(cpu, read_byte(cpu, get_rhl())));
    break;
  case 0x35: // DCR M
    write_byte(cpu, get_rhl(), op_dcr(cpu, read_byte(cpu, get_rhl())));
    break;
  case 0x3C: // INR A
    cpu->ra = op_inr(cpu, cpu->ra);
    break;
  case 0x3D: // DCR A
    cpu->ra = op_dcr(cpu, cpu->ra);
    break;
  case 0X2F: // CMA
    cpu->ra = ~cpu->ra;
    break;
  case 0X27: // DAA
    op_daa(cpu);
    break;

  // NOP ops
  case 0x00: // NOP
  case 0X08: // *NOP
  case 0X10: // *NOP
  case 0X18: // *NOP
  case 0x20: // *NOP
  case 0x28: // *NOP
  case 0x30: // *NOP
  case 0x38: // *NOP
    break;

  // Data transfer ops
  case 0X40: // MOV B,B
    break;
  case 0X41: // MOV B,C
    cpu->rb = cpu->rc;
    break;
  case 0X42: // MOV B,D
    cpu->rb = cpu->rd;
    break;
  case 0X43: // MOV B,E
    cpu->rb = cpu->re;
    break;
  case 0X44: // MOV B,H
    cpu->rb = cpu->rh;
    break;
  case 0X45: // MOV B,L
    cpu->rb = cpu->rl;
    break;
  case 0X46: // MOV B,M
    cpu->rb = read_byte(cpu, get_rhl());
    break;
  case 0X47: // MOV B,A
    cpu->rb = cpu->ra;
    break;
  case 0X48: // MOV C,B
    cpu->rc = cpu->rb;
    break;
  case 0X49: // MOV C,C
    break;
  case 0X4A: // MOV C,D
    cpu->rc = cpu->rd;
    break;
  case 0X4B: // MOV C,E
    cpu->rc = cpu->re;
    break;
  case 0X4C: // MOV C,H
    cpu->rc = cpu->rh;
    break;
  case 0X4D: // MOV C,L
    cpu->rc = cpu->rl;
    break;
  case 0X4E: // MOV C,M
    cpu->rc = read_byte(cpu, get_rhl());
    break;
  case 0X4F: // MOV C,A
    cpu->rc = cpu->ra;
    break;
  case 0X50: // MOV D,B
    cpu->rd = cpu->rb;
    break;
  case 0X51: // MOV D,C
    cpu->rd = cpu->rc;
    break;
  case 0X52: // MOV D,D
    break;
  case 0X53: // MOV D,E
    cpu->rd = cpu->re;
    break;
  case 0X54: // MOV D,H
    cpu->rd = cpu->rh;
    break;
  case 0X55: // MOV D,L
    cpu->rd = cpu->rl;
    break;
  case 0X56: // MOV D,M
    cpu->rd = read_byte(cpu, get_rhl());
    break;
  case 0X57: // MOV D,A
    cpu->rd = cpu->ra;
    break;
  case 0X58: // MOV E,B
    cpu->re = cpu->rb;
    break;
  case 0X59: // MOV E,C
    cpu->re = cpu->rc;
    break;
  case 0X5A: // MOV E,D
    cpu->re = cpu->rd;
    break;
  case 0X5B: // MOV E,E
    break;
  case 0X5C: // MOV E,H
    cpu->re = cpu->rh;
    break;
  case 0X5D: // MOV E,L
    cpu->re = cpu->rl;
    break;
  case 0X5E: // MOV E,M
    cpu->re = read_byte(cpu, get_rhl());
    break;
  case 0X5F: // MOV E,A
    cpu->re = cpu->ra;
    break;
  case 0X60: // MOV H,B
    cpu->rh = cpu->rb;
    break;
  case 0X61: // MOV H,C
    cpu->rh = cpu->rc;
    break;
  case 0X62: // MOV H,D
    cpu->rh = cpu->rd;
    break;
  case 0X63: // MOV H,E
    cpu->rh = cpu->re;
    break;
  case 0X64: // MOV H,H
    break;
  case 0X65: // MOV H,L
    cpu->rh = cpu->rl;
    break;
  case 0X66: // MOV H,M
    cpu->rh = read_byte(cpu, get_rhl());
    break;
  case 0X67: // MOV H,A
    cpu->rh = cpu->ra;
    break;
  case 0X68: // MOV L,B
    cpu->rl = cpu->rb;
    break;
  case 0X69: // MOV L,C
    cpu->rl = cpu->rc;
    break;
  case 0X6A: // MOV L,D
    cpu->rl = cpu->rd;
    break;
  case 0X6B: // MOV L,E
    cpu->rl = cpu->re;
    break;
  case 0X6C: // MOV L,H
    cpu->rl = cpu->rh;
    break;
  case 0X6D: // MOV L,L
    break;
  case 0X6E: // MOV L,M
    cpu->rl = read_byte(cpu, get_rhl());
    break;
  case 0X6F: // MOV L,A
    cpu->rl = cpu->ra;
    break;
  case 0X70: // MOV M,B
    write_byte(cpu, get_rhl(), cpu->rb);
    break;
  case 0X71: // MOV M,C
    write_byte(cpu, get_rhl(), cpu->rc);
    break;
  case 0X72: // MOV M,D
    write_byte(cpu, get_rhl(), cpu->rd);
    break;
  case 0X73: // MOV M,E
    write_byte(cpu, get_rhl(), cpu->re);
    break;
  case 0X74: // MOV M,H
    write_byte(cpu, get_rhl(), cpu->rh);
    break;
  case 0X75: // MOV M,L
    write_byte(cpu, get_rhl(), cpu->rl);
    break;
  case 0X77: // MOV M,A
    write_byte(cpu, get_rhl(), cpu->ra);
    break;
  case 0X78: // MOV A,B
    cpu->ra = cpu->rb;
    break;
  case 0X79: // MOV A,C
    cpu->ra = cpu->rc;
    break;
  case 0X7A: // MOV A,D
    cpu->ra = cpu->rd;
    break;
  case 0X7B: // MOV A,E
    cpu->ra = cpu->re;
    break;
  case 0X7C: // MOV A,H
    cpu->ra = cpu->rh;
    break;
  case 0X7D: // MOV A,L
    cpu->ra = cpu->rl;
    break;
  case 0X7E: // MOV A,M
    cpu->ra = read_byte(cpu, get_rhl());
    break;
  case 0X7F: // MOV A,A
    break;

  // Register or memory to accumulator ops
  case 0X80: // ADD B
    op_add(cpu, cpu->rb, 0);
    break;
  case 0X81: // ADD C
    op_add(cpu, cpu->rc, 0);
    break;
  case 0X82: // ADD D
    op_add(cpu, cpu->rd, 0);
    break;
  case 0X83: // ADD E
    op_add(cpu, cpu->re, 0);
    break;
  case 0X84: // ADD H
    op_add(cpu, cpu->rh, 0);
    break;
  case 0X85: // ADD L
    op_add(cpu, cpu->rl, 0);
    break;
  case 0X86: // ADD M
    op_add(cpu, read_byte(cpu, get_rhl()), 0);
    break;
  case 0X87: // ADD A
    op_add(cpu, cpu->ra, 0);
    break;
  case 0X88: // ADC B
    op_add(cpu, cpu->rb, cpu->cfc);
    break;
  case 0X89: // ADC C
    op_add(cpu, cpu->rc, cpu->cfc);
    break;
  case 0X8A: // ADC D
    op_add(cpu, cpu->rd, cpu->cfc);
    break;
  case 0X8B: // ADC E
    op_add(cpu, cpu->re, cpu->cfc);
    break;
  case 0X8C: // ADC H
    op_add(cpu, cpu->rh, cpu->cfc);
    break;
  case 0X8D: // ADC L
    op_add(cpu, cpu->rl, cpu->cfc);
    break;
  case 0X8E: // ADC M
    op_add(cpu, read_byte(cpu, get_rhl()), cpu->cfc);
    break;
  case 0X8F: // ADC A
    op_add(cpu, cpu->ra, cpu->cfc);
    break;
  case 0X90: // SUB B
    op_sub(cpu, cpu->rb, 0);
    break;
  case 0X91: // SUB C
    op_sub(cpu, cpu->rc, 0);
    break;
  case 0X92: // SUB D
    op_sub(cpu, cpu->rd, 0);
    break;
  case 0X93: // SUB E
    op_sub(cpu, cpu->re, 0);
    break;
  case 0X94: // SUB H
    op_sub(cpu, cpu->rh, 0);
    break;
  case 0X95: // SUB L
    op_sub(cpu, cpu->rl, 0);
    break;
  case 0X96: // SUB M
    op_sub(cpu, read_byte(cpu, get_rhl()), 0);
    break;
  case 0X97: // SUB A
    op_sub(cpu, cpu->ra, 0);
    break;
  case 0X98: // SBB B
    op_sub(cpu, cpu->rb, cpu->cfc);
    break;
  case 0X99: // SBB C
    op_sub(cpu, cpu->rc, cpu->cfc);
    break;
  case 0X9A: // SBB D
    op_sub(cpu, cpu->rd, cpu->cfc);
    break;
  case 0X9B: // SBB E
    op_sub(cpu, cpu->re, cpu->cfc);
    break;
  case 0X9C: // SBB H
    op_sub(cpu, cpu->rh, cpu->cfc);
    break;
  case 0X9D: // SBB L
    op_sub(cpu, cpu->rl, cpu->cfc);
    break;
  case 0X9E: // SBB M
    op_sub(cpu, read_byte(cpu, get_rhl()), cpu->cfc);
    break;
  case 0X9F: // SBB A
    op_sub(cpu, cpu->ra, cpu->cfc);
    break;
  case 0XA0: // ANA B
    op_ana(cpu, cpu->rb);
    break;
  case 0XA1: // ANA C
    op_ana(cpu, cpu->rc);
    break;
  case 0XA2: // ANA D
    op_ana(cpu, cpu->rd);
    break;
  case 0XA3: // ANA E
    op_ana(cpu, cpu->re);
    break;
  case 0XA4: // ANA H
    op_ana(cpu, cpu->rh);
    break;
  case 0XA5: // ANA L
    op_ana(cpu, cpu->rl);
    break;
  case 0XA6: // ANA M
    op_ana(cpu, read_byte(cpu, get_rhl()));
    break;
  case 0XA7: // ANA A
    op_ana(cpu, cpu->ra);
    break;
  case 0XA8: // XRA B
    op_xra(cpu, cpu->rb);
    break;
  case 0XA9: // XRA C
    op_xra(cpu, cpu->rc);
    break;
  case 0XAA: // XRA D
    op_xra(cpu, cpu->rd);
    break;
  case 0XAB: // XRA E
    op_xra(cpu, cpu->re);
    break;
  case 0XAC: // XRA H
    op_xra(cpu, cpu->rh);
    break;
  case 0XAD: // XRA L
    op_xra(cpu, cpu->rl);
    break;
  case 0XAE: // XRA M
    op_xra(cpu, read_byte(cpu, get_rhl()));
    break;
  case 0XAF: // XRA A
    op_xra(cpu, cpu->ra);
    break;
  case 0XB0: // ORA B
    op_ora(cpu, cpu->rb);
    break;
  case 0XB1: // ORA C
    op_ora(cpu, cpu->rc);
    break;
  case 0XB2: // ORA D
    op_ora(cpu, cpu->rd);
    break;
  case 0XB3: // ORA E
    op_ora(cpu, cpu->re);
    break;
  case 0XB4: // ORA H
    op_ora(cpu, cpu->rh);
    break;
  case 0XB5: // ORA L
    op_ora(cpu, cpu->rl);
    break;
  case 0XB6: // ORA M
    op_ora(cpu, read_byte(cpu, get_rhl()));
    break;
  case 0XB7: // ORA A
    op_ora(cpu, cpu->ra);
    break;
  case 0XB8: // CMP B
    op_cmp(cpu, cpu->rb);
    break;
  case 0XB9: // CMP C
    op_cmp(cpu, cpu->rc);
    break;
  case 0XBA: // CMP D
    op_cmp(cpu, cpu->rd);
    break;
  case 0XBB: // CMP E
    op_cmp(cpu, cpu->re);
    break;
  case 0XBC: // CMP H
    op_cmp(cpu, cpu->rh);
    break;
  case 0XBD: // CMP L
    op_cmp(cpu, cpu->rl);
    break;
  case 0XBE: // CMP M
    op_cmp(cpu, read_byte(cpu, get_rhl()));
    break;
  case 0XBF: // CMP A
    op_cmp(cpu, cpu->ra);
    break;

  // Rotate accumulator opts
  case 0X07: // RLC
    op_rlc(cpu);
    break;
  case 0X0F: // RRC
    op_rrc(cpu);
    break;
  case 0X17: // RAL
    op_ral(cpu);
    break;
  case 0X1F: // RAR
    op_rar(cpu);
    break;

  // Register pair ops
  case 0XC5: // PUSH B
    stack_push(cpu, get_rbc());
    break;
  case 0XD5: // PUSH D
    stack_push(cpu, get_rde());
    break;
  case 0XE5: // PUSH H
    stack_push(cpu, get_rhl());
    break;
  case 0XF5: // PUSH PSW
    op_push_psw(cpu);
    break;
  case 0XC1: // POP B
    set_rbc(stack_pop(cpu));
    break;
  case 0XD1: // POP D
    set_rde(stack_pop(cpu));
    break;
  case 0XE1: // POP H
    set_rhl(stack_pop(cpu));
    break;
  case 0XF1: // POP PSW
    op_pop_psw(cpu);
    break;
  case 0X09: // DAD B
    op_dad(cpu, get_rbc());
    break;
  case 0X19: // DAD D
    op_dad(cpu, get_rde());
    break;
  case 0X29: // DAD H
    op_dad(cpu, get_rhl());
    break;
  case 0X39: // DAD SP
    op_dad(cpu, cpu->sp);
    break;
  case 0X03: // INX B
    set_rbc(get_rbc() + 1);
    break;
  case 0X13: // INX D
    set_rde(get_rde() + 1);
    break;
  case 0X23: // INX H
    set_rhl(get_rhl() + 1);
    break;
  case 0X33: // INX SP
    cpu->sp++;
    break;
  case 0x0B: // DCX B
    set_rbc(get_rbc() - 1);
    break;
  case 0x1B: // DCX D
    set_rde(get_rde() - 1);
    break;
  case 0X2B: // DCX H
    set_rhl(get_rhl() - 1);
    break;
  case 0x3B: // DCX SP
    cpu->sp--;
    break;
  case 0XEB: // XCHG
    op_xchg(cpu);
    break;
  case 0XE3: // XTHL
    op_xthl(cpu);
    break;
  case 0XF9: // SPHL
    cpu->sp = get_rhl();
    break;

  // Immediate ops
  case 0X01: // LXI B
    set_rbc(next_word(cpu));
    break;
  case 0X11: // LXI D
    set_rde(next_word(cpu));
    break;
  case 0X21: // LXI H
    set_rhl(next_word(cpu));
    break;
  case 0X31: // LXI SP
    cpu->sp = next_word(cpu);
    break;
  case 0X06: // MVI B
    cpu->rb = next_byte(cpu);
    break;
  case 0X0E: // MVI C
    cpu->rc = next_byte(cpu);
    break;
  case 0X16: // MVI D
    cpu->rd = next_byte(cpu);
    break;
  case 0X1E: // MVI E
    cpu->re = next_byte(cpu);
    break;
  case 0X26: // MVI H
    cpu->rh = next_byte(cpu);
    break;
  case 0X2E: // MVI L
    cpu->rl = next_byte(cpu);
    break;
  case 0X36: // MVI M
    write_byte(cpu, get_rhl(), next_byte(cpu));
    break;
  case 0X3E: // MVI A
    cpu->ra = next_byte(cpu);
    break;
  case 0XC6: // ADI
    op_add(cpu, next_byte(cpu), 0);
    break;
  case 0XCE: // ACI
    op_add(cpu, next_byte(cpu), cpu->cfc);
    break;
  case 0XD6: // SUI
    op_sub(cpu, next_byte(cpu), 0);
    break;
  case 0XDE: // SBI
    op_sub(cpu, next_byte(cpu), cpu->cfc);
    break;
  case 0XE6: // ANI
    op_ana(cpu, next_byte(cpu));
    break;
  case 0XEE: // XRI
    op_xra(cpu, next_byte(cpu));
    break;
  case 0XF6: // ORI
    op_ora(cpu, next_byte(cpu));
    break;
  case 0XFE: // CPI
    op_cmp(cpu, next_byte(cpu));
    break;

  // Direct addressing ops
  case 0X02: // STAX B
    write_byte(cpu, get_rbc(), cpu->ra);
    break;
  case 0X12: // STAX D
    write_byte(cpu, get_rde(), cpu->ra);
    break;
  case 0X32: // STA
    write_byte(cpu, next_word(cpu), cpu->ra);
    break;
  case 0X0A: // LDAX B
    cpu->ra = read_byte(cpu, get_rbc());
    break;
  case 0X1A: // LDAX D
    cpu->ra = read_byte(cpu, get_rde());
    break;
  case 0X3A: // LDA
    cpu->ra = read_byte(cpu, next_word(cpu));
    break;
  case 0X22: // SHLD
    write_word(cpu, next_word(cpu), get_rhl());
    break;
  case 0X2A: // LHLD
    set_rhl(read_word(cpu, next_word(cpu)));
    break;

  // Jump ops
  case 0XE9: // PCHL
    cpu->pc = get_rhl();
    break;
  case 0XC2: // JNZ
    op_jmp_cond(cpu, next_word(cpu), cpu->cfz == 0);
    break;
  case 0XC3: // JMP
  case 0XCB: // *JMP
    cpu->pc = next_word(cpu);
    break;
  case 0XCA: // JZ
    op_jmp_cond(cpu, next_word(cpu), cpu->cfz == 1);
    break;
  case 0XD2: // JNC
    op_jmp_cond(cpu, next_word(cpu), cpu->cfc == 0);
    break;
  case 0XDA: // JC
    op_jmp_cond(cpu, next_word(cpu), cpu->cfc == 1);
    break;
  case 0XE2: // JPO
    op_jmp_cond(cpu, next_word(cpu), cpu->cfp == 0);
    break;
  case 0XEA: // JPE
    op_jmp_cond(cpu, next_word(cpu), cpu->cfp == 1);
    break;
  case 0XF2: // JP
    op_jmp_cond(cpu, next_word(cpu), cpu->cfs == 0);
    break;
  case 0XFA: // JM
    op_jmp_cond(cpu, next_word(cpu), cpu->cfs == 1);
    break;

  // Call ops
  case 0XCD: // CALL
  case 0XDD: // *CALL
  case 0XED: // *CALL
  case 0XFD: // *CALL
    op_call(cpu, next_word(cpu));
    break;
  case 0XDC: // CC
    op_call_cond(cpu, next_word(cpu), cpu->cfc == 1);
    break;
  case 0XD4: // CNC
    op_call_cond(cpu, next_word(cpu), cpu->cfc == 0);
    break;
  case 0XCC: // CZ
    op_call_cond(cpu, next_word(cpu), cpu->cfz == 1);
    break;
  case 0XC4: // CNZ
    op_call_cond(cpu, next_word(cpu), cpu->cfz == 0);
    break;
  case 0XF4: // CP
    op_call_cond(cpu, next_word(cpu), cpu->cfs == 0);
    break;
  case 0XFC: // CM
    op_call_cond(cpu, next_word(cpu), cpu->cfs == 1);
    break;
  case 0XEC: // CPE
    op_call_cond(cpu, next_word(cpu), cpu->cfp == 1);
    break;
  case 0XE4: // CPO
    op_call_cond(cpu, next_word(cpu), cpu->cfp == 0);
    break;

  // Return ops
  case 0XC9: // RET
  case 0XD9: // *RET
    cpu->pc = stack_pop(cpu);
    break;
  case 0XD8: // RC
    op_ret_cond(cpu, cpu->cfc == 1);
    break;
  case 0XD0: // RNC
    op_ret_cond(cpu, cpu->cfc == 0);
    break;
  case 0XC8: // RZ
    op_ret_cond(cpu, cpu->cfz == 1);
    break;
  case 0XC0: // RNZ
    op_ret_cond(cpu, cpu->cfz == 0);
    break;
  case 0XF8: // RM
    op_ret_cond(cpu, cpu->cfs == 1);
    break;
  case 0XF0: // RP
    op_ret_cond(cpu, cpu->cfs == 0);
    break;
  case 0XE8: // RPE
    op_ret_cond(cpu, cpu->cfp == 1);
    break;
  case 0XE0: // RPO
    op_ret_cond(cpu, cpu->cfp == 0);
    break;

  // RST ops
  case 0XC7: // RST 0
    op_call(cpu, 0x00);
    break;
  case 0XCF: // RST 1
    op_call(cpu, 0x08);
    break;
  case 0XD7: // RST 2
    op_call(cpu, 0x10);
    break;
  case 0XDF: // RST 3
    op_call(cpu, 0x18);
    break;
  case 0XE7: // RST 4
    op_call(cpu, 0x20);
    break;
  case 0XEF: // RST 5
    op_call(cpu, 0x28);
    break;
  case 0XF7: // RST 6
    op_call(cpu, 0x30);
    break;
  case 0XFF: // RST 7
    op_call(cpu, 0x38);
    break;

  // INTE flip-flop ops
  case 0XFB: // EI
    cpu->inte = true;
    cpu->interrupt_delay = true;
    break;
  case 0XF3: // DI
    cpu->inte = false;
    break;

  // Device read/write ops
  case 0XDB: // IN
    cpu->ra = cpu->read_device(cpu, next_byte(cpu));
    break;
  case 0XD3: // OUT
    cpu->write_device(cpu, next_byte(cpu), cpu->ra);
    break;

  // HLT ops
  case 0X76: // HLT
    cpu->halted = true;
    break;

  default:
    break;
  }
}
