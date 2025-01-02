// File: receive/src/direwolf/c/include/demod_afsk.h
// Where it goes in your directory tree:
//   receive/src/direwolf/c/include/demod_afsk.h

#ifndef DEMOD_AFSK_H
#define DEMOD_AFSK_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <ctype.h>

#include "direwolf.h"          // For MAX_CHANS, etc.
#include "fsk_demod_state.h"   // struct demodulator_state_s
#include "textcolor.h"
#include "dsp.h"

// ------------------------------------------------------------------
// demod_afsk.h: Declarations for AFSK demod functions.
//
// This header only has prototypes (no function bodies).
// The corresponding .c file implements them.
// ------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the AFSK demodulator:
void demod_afsk_init(int samples_per_sec, 
                     int baud,
                     int mark_freq,
                     int space_freq,
                     char profile,
                     struct demodulator_state_s *D);

// Process a single audio sample through the demodulator:
void demod_afsk_process_sample(int chan,
                               int subchan,
                               int sam,
                               struct demodulator_state_s *D);

#ifdef __cplusplus
}
#endif

#endif /* DEMOD_AFSK_H */
