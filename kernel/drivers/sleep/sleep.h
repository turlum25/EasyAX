#ifndef SLEEP_H
#define SLEEP_H

#include "headers/stdint.h"

#define PIT_BASE_FREQUENCY 1193182
#define PIT_COMMAND_PORT   0x43
#define PIT_CHANNEL0_PORT  0x40

static volatile uint64_t system_ticks = 0;

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void io_wait(void) {
    asm volatile("outb %%al, $0x80" : : "a"(0));
}

static inline void timer_init(uint32_t frequency) {
    uint32_t divisor = PIT_BASE_FREQUENCY / frequency;

    outb(PIT_COMMAND_PORT, 0x36);
    outb(PIT_CHANNEL0_PORT, (uint8_t)(divisor & 0xFF));
    io_wait();
    outb(PIT_CHANNEL0_PORT, (uint8_t)((divisor >> 8) & 0xFF));
}

static inline void timer_handler(void) {
    system_ticks++;
}

static inline uint64_t get_ticks(void) {
    return system_ticks;
}

static inline void sleep(uint32_t ms) {
    uint64_t target = system_ticks + ms;
    while (system_ticks < target) {
        asm volatile("hlt");
    }
}

#endif /* SLEEP_H */