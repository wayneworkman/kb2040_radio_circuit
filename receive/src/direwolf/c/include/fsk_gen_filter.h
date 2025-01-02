// File: receive/src/direwolf/c/include/fsk_gen_filter.h

#ifndef FSK_GEN_FILTER_H
#define FSK_GEN_FILTER_H

#include "fsk_demod_state.h"

// Stub if you want. Our code calls gen_bandpass, gen_lowpass, etc. from dsp.h
void fsk_gen_filter(int samples_per_sec,
                    int baud,
                    int mark_freq,
                    int space_freq,
                    char profile,
                    struct demodulator_state_s *D);

#endif
