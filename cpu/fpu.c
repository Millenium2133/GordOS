// FPU enablement and per-process state save/restore. GordOS's kernel
// had no FPU initialization at all before this, any floating point
// instruction (any function returning/taking a double or float,
// including strtod once it's added) would either fault or, worse,
// silently corrupt state across a context switch. This fixes both:
// CR0 is set up so real x87 instructions run instead of faulting,
// and every context switch now saves/restores the outgoing and
// incoming process's FPU registers independently.

#include "fpu.h"

static uint8_t default_state[FPU_STATE_SIZE];

void fpu_init(void)
{
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));

    cr0 &= ~(1u << 2); // clear EM - allow real x87 instructions instead of trapping
    cr0 |=  (1u << 1); // set MP (Monitor Coprocessor)
    cr0 |=  (1u << 5); // set NE - modern (non-legacy-IRQ13) FP exception reporting

    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));

    __asm__ volatile("fninit");
    __asm__ volatile("fsave (%0)" :: "r"(default_state) : "memory");
    __asm__ volatile("fninit"); // fsave already resets the FPU, but be explicit
}

void fpu_seed_default(uint8_t out[FPU_STATE_SIZE])
{
    for (int i = 0; i < FPU_STATE_SIZE; i++)
        out[i] = default_state[i];
}

void fpu_save(uint8_t state[FPU_STATE_SIZE])
{
    __asm__ volatile("fsave (%0)" :: "r"(state) : "memory");
}

void fpu_restore(uint8_t state[FPU_STATE_SIZE])
{
    __asm__ volatile("frstor (%0)" :: "r"(state) : "memory");
}