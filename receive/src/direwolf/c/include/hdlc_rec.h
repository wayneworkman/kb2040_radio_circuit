#ifndef HDLC_REC_H
#define HDLC_REC_H

#include <stdint.h>
#include "direwolf.h"   // for MAX_CHANS, MAX_SUBCHANS, MAX_SLICERS
#include "fcs_calc.h"   // if you need FCS/CRC functions (optional)

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Minimal AFSK-only approach:
 * We define a single "hdlc_state_s" struct to hold per-subchannel decode state.
 */

#define MIN_FRAME_LEN 4   // e.g. minimal HDLC frame (or AX.25 + 2 bytes FCS)
#define MAX_FRAME_LEN 330 // e.g. maximum AX.25 (+ FCS). Adjust if needed.

/**
 * @struct hdlc_state_s
 * @brief  Holds HDLC decoder state for a single subchannel/slice.
 *
 * If you only do AFSK, you typically have 1 subchannel (0) and 1 slicer (0).
 * This minimal approach uses NRZI decoding but does not do fancy bit-stuff checks.
 */
struct hdlc_state_s {
    int prev_raw;              // for NRZI decoding: "previous raw bit"
    unsigned char pat_det;     // pattern detector shift register
    unsigned char oacc;        // octet accumulator (8 bits => 1 byte)
    int olen;                  // how many bits we have in 'oacc'
    unsigned char frame_buf[MAX_FRAME_LEN];
    int frame_len;
};

/**
 * @brief Initialize all HDLC states (clears buffers, etc.).
 *
 * Typically called once during startup.
 */
void hdlc_rec_init(void);

/**
 * @brief Process one raw bit into the HDLC decoder.
 *
 * @param chan         Audio channel index (0..MAX_CHANS-1).
 * @param subchan      Subchannel index (0..MAX_SUBCHANS-1).
 * @param slice        Slicer index if you have multiple slicing thresholds (0..MAX_SLICERS-1).
 * @param raw          The “raw” bit after your demodulator (>0 => ‘1’ else ‘0’).
 * @param is_scrambled For 9600 scramble logic or similar (unused in minimal code).
 * @param not_used_remove A placeholder for future expansion (unused).
 */
void hdlc_rec_bit(int chan, int subchan, int slice,
                  int raw, int is_scrambled, int not_used_remove);

/**
 * @brief Retrieve the most recently captured frame, if any.
 *
 * If a valid HDLC frame was detected, it copies the data to @p out
 * and returns the length in bytes. If no new frame is available, returns -1.
 *
 * @param[out] out Pointer to user buffer to store the frame bytes.
 * @return Length of the frame in bytes, or -1 if none available.
 */
int hdlc_get_frame(unsigned char *out);

#ifdef __cplusplus
}
#endif

#endif /* HDLC_REC_H */
