#ifndef FPU_H
#define FPU_H

#include <stdint.h>

// Legacy 32-bit protected-mode FSAVE/FRSTOR memory image size. Using
// the older FSAVE/FRSTOR pair rather than FXSAVE/FXRSTOR, slower,
// but universally supported since the 387 with no CPUID feature
// check needed and no 16-byte alignment requirement on the save
// buffer. Correctness/simplicity first; can move to FXSAVE/FXRSTOR
// later if performance ever actually matters.
#define FPU_STATE_SIZE 108

// Enable the FPU (clear CR0.EM, set CR0.MP/NE) and capture a
// "freshly initialized" state snapshot for seeding new processes.
// Call once at boot, before any process is created.
void fpu_init(void);

// Copy the captured default (freshly-initialized) FPU state into a
// new process's save area. Call from process_create().
void fpu_seed_default(uint8_t out[FPU_STATE_SIZE]);

// Save/restore the FPU register state to/from a 108-byte buffer.
// Called around every context switch so each process's floating
// point state is preserved independently, GordOS uses simple eager
// (always save/restore) switching rather than the lazy TS-trap
// scheme, trading a little performance for a lot of simplicity.
void fpu_save(uint8_t state[FPU_STATE_SIZE]);
void fpu_restore(uint8_t state[FPU_STATE_SIZE]);

#endif