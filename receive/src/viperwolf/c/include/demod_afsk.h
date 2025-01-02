// File: receive/src/direwolf/c/include/demod_afsk.h

#ifndef DEMOD_AFSK_H
#define DEMOD_AFSK_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

#include "viperwolf.h"
#include "fsk_demod_state.h"
#include "dsp.h"
#include "textcolor.h"

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

// Process a single audio sample:
void demod_afsk_process_sample(int chan,
                               int subchan,
                               int sam,
                               struct demodulator_state_s *D);

#ifdef __cplusplus
}
#endif

#endif /* DEMOD_AFSK_H */
