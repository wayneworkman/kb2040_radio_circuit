/*
 * fdetect.c
 *
 * Demonstration of reading raw audio from stdin (16-bit mono),
 * measuring energy at two frequencies (1200 & 2200 Hz),
 * and printing the amplitude & ratio for debugging.
 *
 * Usage example:
 *    arecord -f S16_LE -c1 -r48000 | ./fdetect
 *
 * (Or you can redirect from a file containing 16-bit samples.)
 *
 * Compile:
 *    gcc -O2 -o fdetect fdetect.c -lm
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

/* You can tweak this chunk size if you like.
   1024 is a common "power of two" choice. */
#define CHUNK_LEN 1024

/* For a 48 kHz input rate, each chunk of 1024 samples is:
      1024 / 48000.0 = ~0.0213 seconds
   We'll accumulate that in 'timestamp'. */
static const float SAMPLE_RATE = 48000.0f;

/*
 * Implementation of a simple Goertzel-based function to measure
 * the "energy" at a specific target frequency, for a block of samples.
 *
 * Reference: https://en.wikipedia.org/wiki/Goertzel_algorithm
 *
 * Inputs:
 *   samples[]: array of CHUNK_LEN floats in range ~[-1.0,+1.0]
 *   numSamples: how many samples (here always CHUNK_LEN)
 *   freqHz: which frequency to measure
 *
 * Returns:
 *   Some measure of energy or magnitude squared at that frequency.
 *   Larger returned value means stronger presence of that frequency.
 */
float get_energy_for_freq(const float *samples, int numSamples, float freqHz)
{
    /* The normalized frequency in radians per sample: */
    float fTerm = 2.0f * (float)M_PI * freqHz / SAMPLE_RATE;

    /* Goertzel state variables: */
    float coeff = 2.0f * cosf(fTerm);
    float s1 = 0.0f;
    float s2 = 0.0f;

    for (int i = 0; i < numSamples; i++) {
        float x = samples[i];
        float s0 = x + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    /* At the end, we can compute the power (magnitude squared): */
    float power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    return power; /* This is a rough measure of "energy" at freqHz. */
}


int main(void)
{
    const float freqMark  = 1200.0f;  /* Mark freq */
    const float freqSpace = 2200.0f;  /* Space freq */

    float chunkData[CHUNK_LEN];
    float timestamp = 0.0f;

    fprintf(stderr, "Frequency Detector Demo: Checking energies at %.1f Hz and %.1f Hz.\n",
            freqMark, freqSpace);

    /* We'll read 16-bit samples from stdin, so 2 bytes per sample. */
    const size_t bytesPerSample = 2;
    const size_t bytesPerChunk  = CHUNK_LEN * bytesPerSample;

    /* Main processing loop: read raw samples from stdin in blocks of CHUNK_LEN. */
    while (1) {
        /* Buffer to hold the raw 16-bit data before converting to float. */
        int16_t tempBuf[CHUNK_LEN];

        /* Attempt to read one chunk worth of bytes. */
        size_t nRead = fread(tempBuf, 1, bytesPerChunk, stdin);

        if (nRead < bytesPerChunk) {
            /* If we didn't get a full chunk, either EOF or partial read => break. */
            if (nRead > 0) {
                /* Optionally handle partial chunk... but we'll just stop. */
            }
            break; /* end of file or read error => exit loop */
        }

        /* Convert the int16_t samples to float in range ~[-1.0, +1.0]. */
        for (int i = 0; i < CHUNK_LEN; i++) {
            chunkData[i] = (float)tempBuf[i] / 32768.0f;
        }

        /* measure energies at 1200 Hz and 2200 Hz */
        float magMark  = get_energy_for_freq(chunkData, CHUNK_LEN, freqMark);
        float magSpace = get_energy_for_freq(chunkData, CHUNK_LEN, freqSpace);

        /* compute ratio, be cautious about dividing by zero */
        float ratio = 0.0f;
        if (magSpace > 1e-12f) {
            ratio = magMark / magSpace;
        }

        /* Decide which freq is stronger. */
        const char *freqStr = (magMark > magSpace) ? "MARK freq" : "SPACE freq";

        /* Print results: timestamp, amplitude for each freq, ratio, etc. */
        printf("%.3f %s (%.1f Hz) is stronger. ratio=%.2f  [magMark=%.2f  magSpace=%.2f]\n",
               timestamp,
               freqStr,
               (magMark > magSpace) ? freqMark : freqSpace,
               ratio,
               magMark,
               magSpace
        );

        /* Advance the timestamp by chunk duration in seconds. */
        timestamp += (float)CHUNK_LEN / SAMPLE_RATE;
    }

    return 0;
}
