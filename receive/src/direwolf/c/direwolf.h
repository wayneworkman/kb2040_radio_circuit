# src/direwolf/c/direwolf.h
#ifndef DIREWOLF_H
#define DIREWOLF_H

#include <stdint.h>

// Core FSK parameters
#define MAX_SUBCHANS 1
#define MAX_SLICERS 1
#define MAX_FILTER_SIZE 1024

// Structs needed for demodulation
struct demodulator_state_s {
    float m_peak;
    float m_valley;
    float s_peak;
    float s_valley;
    // Add other needed fields from original direwolf.h
};

// Function declarations
void demod_afsk_init(int samples_per_sec, int baud, int mark_freq,
                     int space_freq, char profile, 
                     struct demodulator_state_s *D);

void demod_afsk_process_sample(int chan, int subchan, int sam,
                             struct demodulator_state_s *D);

#endif

# src/direwolf/c/demod_afsk.h
#ifndef DEMOD_AFSK_H
#define DEMOD_AFSK_H

#include "direwolf.h"

// Add any additional AFSK-specific declarations

#endif