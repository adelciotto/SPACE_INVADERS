// adc_8080_cpu Intel 8080 CPU emulation library by Anthony Del Ciotto.
// This emulator uses the following resources as the main references:
// - https://altairclone.com/downloads/manuals/8080%20Programmers%20Manual.pdf
// - https://pastraiser.com/cpu/i8080/i8080_opcodes.html

#ifndef _ADC_8080_CPU_H_
#define _ADC_8080_CPU_H_

#include <stdint.h> // For int types
#include <stdio.h>  // For FILE*

#ifndef __cplusplus
#include <stdbool.h> // For the bool type
#endif

// 0.4.1
#define ADC_8080_CPU_VERSION_MAJOR 0
#define ADC_8080_CPU_VERSION_MINOR 4
#define ADC_8080_CPU_VERSION_PATCH 1

typedef struct {
  // 7 8-bit registers (accum and scratch).
  uint8_t ra, rb, rc, rd, re, rh, rl;

  // 16-bit program counter.
  uint16_t pc;

  // 16-bit stack pointer.
  uint16_t sp;

  // Condition flags (sign, zero, aux, parity, carry).
  bool cfs, cfz, cfa, cfp, cfc;

  // Interrupt and halt state variables.
  bool halted;
  bool inte; // Interrupt Enable flip-flop
  bool interrupt_pending;
  uint8_t interrupt_opcode;
  bool interrupt_delay;

  // Cycles the cpu has consumed in the latest step.
  int cycles;

  // Custom user data for function handlers.
  void *userdata;

  // Memory read and write function handlers.
  uint8_t (*read_byte)(void *userdata, uint16_t addr);
  void (*write_byte)(void *userdata, uint16_t addr, uint8_t val);

  // Device read and write function handlers.
  uint8_t (*read_device)(void *userdata, uint8_t device);
  void (*write_device)(void *userdata, uint8_t device, uint8_t val);
} adc_8080_cpu;

#ifdef __cpluscplus
extern "C" {
#endif

// adc_8080_cpu_init() - Init the 8080 cpu.
void adc_8080_cpu_init(adc_8080_cpu *cpu);

// adc_8080_cpu_step() - Decode and execute the next instruction.
//
// Returns the number of cycles consumed from this step.
int adc_8080_cpu_step(adc_8080_cpu *cpu);

// adc_8080_cpu_interrupt() - Request an interrupt with the given opcode.
void adc_8080_cpu_interrupt(adc_8080_cpu *cpu, uint8_t opcode);

// adc_8080_cpu_print() - Print the state of the cpu in a readable form to the
// given stream.
void adc_8080_cpu_print(adc_8080_cpu *cpu, FILE *stream);

#ifdef __cpluscplus
}
#endif

#endif // _ADC_8080_CPU_H_
