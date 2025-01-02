// File: receive/src/direwolf/c/include/dsp.h

#ifndef DSP_H
#define DSP_H

#include "fsk_demod_state.h"

// Minimal stubs for bandpass, lowpass, RRC:
float window (bp_window_t type, int size, int j);

void gen_lowpass(float fc, float *lp_filter, int filter_size, bp_window_t wtype);

void gen_bandpass(float f1, float f2, float *bp_filter, int filter_size, bp_window_t wtype);

float rrc(float t, float a);

void gen_rrc_lowpass(float *pfilter, int taps, float rolloff, float sps);

#endif
