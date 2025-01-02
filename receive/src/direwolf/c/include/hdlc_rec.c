#include "hdlc_rec.h"
#include <string.h>  // for memset, memcpy
#include <stdio.h>   // if you need printf/debug
#include <assert.h>  // if you need asserts

/*
 * If you want normal AX.25 style CRC checks, keep "fcs_calc.h" included,
 * but the minimal code below does not actually call fcs_calc().
 * Feel free to remove it if unused.
 */
#include "fcs_calc.h"

/**
 * @brief The global HDLC states for each possible (chan, subchan, slicer).
 * In minimal usage, you’ll typically just have (chan=0, subchan=0, slice=0).
 */
static struct hdlc_state_s hdlc_state[MAX_CHANS][MAX_SUBCHANS][MAX_SLICERS];

/*
 * Optionally, we track one captured frame in static memory:
 *   have_frame  -> indicates if a new complete frame is ready
 *   last_frame_buf[] + last_frame_len -> store the most recent frame
 */
static int have_frame = 0;
static unsigned char last_frame_buf[MAX_FRAME_LEN];
static int last_frame_len = 0;

void hdlc_rec_init(void)
{
    // Clear all states to zero
    memset(hdlc_state, 0, sizeof(hdlc_state));

    // Clear out the “latest frame” info
    have_frame = 0;
    last_frame_len = 0;
    memset(last_frame_buf, 0, sizeof(last_frame_buf));
}

void hdlc_rec_bit(int chan, int subchan, int slice,
                  int raw, int is_scrambled, int not_used_remove)
{
    // Minimal approach: no scramble logic, no bit-stuff logic,
    // just raw or NRZI decode. 
    // "raw" > 0 => '1' bit, else => '0' bit.

    struct hdlc_state_s *H = &hdlc_state[chan][subchan][slice];

    /*
     * NRZI decoding: 
     *    '0' bit => invert from previous
     *    '1' bit => same as previous
     * If you want a direct “raw = demod_out>0 ? 1 : 0”, do no decode:
     *    int dbit = raw; // direct
     */
    int dbit = (raw == H->prev_raw) ? 1 : 0;
    H->prev_raw = raw;

    // Shift pattern-detector by 1 bit; load in the new bit
    H->pat_det >>= 1;
    if (dbit)
        H->pat_det |= 0x80; // sets top bit if '1'

    // If we see the flag pattern 0x7E (01111110 in LSB-first),
    // then we reset the receiving of a new frame.
    if (H->pat_det == 0x7E) {
        // Start new frame
        H->olen = 0;
        H->frame_len = 0;
        return;
    }

    // Accumulate 1 bit into the “oacc” (octet accumulator).
    // LSB-first => shift right each time
    H->oacc >>= 1;
    if (dbit)
        H->oacc |= 0x80;

    H->olen++;
    if (H->olen == 8) {
        // We have 8 bits => 1 full byte
        if (H->frame_len < MAX_FRAME_LEN) {
            H->frame_buf[H->frame_len++] = H->oacc;
        }
        H->olen = 0;
    }

    // Minimal logic: once we have >= MIN_FRAME_LEN,
    // consider it a "valid" partial frame. 
    // In a real HDLC approach, you’d do more advanced checks,
    // or wait for another 0x7E “flag” to finalize the frame, etc.
    if (H->frame_len >= MIN_FRAME_LEN) {
        // Save a copy as “last frame”
        memcpy(last_frame_buf, H->frame_buf, H->frame_len);
        last_frame_len = H->frame_len;
        have_frame = 1;
    }
}

int hdlc_get_frame(unsigned char *out)
{
    // If no new frame is ready, return -1
    if (!have_frame)
        return -1;

    // Copy the last captured frame into user’s buffer
    memcpy(out, last_frame_buf, last_frame_len);
    int len = last_frame_len;

    // Clear the “have_frame” flag so we only return it once
    have_frame = 0;

    return len;
}
