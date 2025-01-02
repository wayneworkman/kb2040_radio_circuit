// File: receive/src/viperwolf/c/demod_factory.c
//
// This file provides a factory function that allocates a new
// 'demodulator_state_s' from the heap and returns it as an opaque pointer.
// Also provides a matching free function.

#include <stdlib.h>
#include <string.h>
#include "demod_afsk.h"  // So we know the type struct demodulator_state_s

// Create & zero out a new demodulator_state_s, returning pointer.
struct demodulator_state_s * create_demodulator_state(void)
{
    struct demodulator_state_s *p = malloc(sizeof(*p));
    if (p) {
        memset(p, 0, sizeof(*p));
    }
    return p;
}

// Free an existing demodulator_state_s.
void free_demodulator_state(struct demodulator_state_s *p)
{
    if (p) {
        free(p);
    }
}
