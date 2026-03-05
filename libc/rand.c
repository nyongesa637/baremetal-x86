#include "rand.h"

static uint32_t state = 1;

void rand_seed(uint32_t seed) {
    state = seed ? seed : 1;
}

// xorshift32
uint32_t rand_next(void) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

uint32_t rand_range(uint32_t min, uint32_t max) {
    if (min >= max) return min;
    return min + (rand_next() % (max - min + 1));
}
