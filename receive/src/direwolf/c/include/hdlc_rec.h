//
// hdlc_rec.c (minimal AFSK-only version)
//
#include "direwolf.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>  // for uint64_t

// We keep fcs_calc if you want a normal AX.25 HDLC approach. 
// If you do NOT want to do CRC checks, remove references to it as well.
#include "fcs_calc.h"

/*
 * We remove everything referencing EAS, AIS, scramble, 9600, multi_modem, etc.
 * This file just accumulates bits into HDLC frames.
 */

#define MIN_FRAME_LEN 4   // or AX25_MIN_PACKET_LEN + 2, if you want
#define MAX_FRAME_LEN 330 // or AX25_MAX_PACKET_LEN + 2

struct hdlc_state_s {
    int prev_raw;        // used for NRZI decoding
    unsigned char pat_det;   // pattern detector
    unsigned char oacc;      // octet accumulator
    int olen;                // bits in oacc
    unsigned char frame_buf[MAX_FRAME_LEN];
    int frame_len;
};

static struct hdlc_state_s hdlc_state[MAX_CHANS][MAX_SUBCHANS][MAX_SLICERS];

// If you want to do a "data detect" approach for DCD, 
// you can keep a per-slice or per-subchannel approach. 
// We'll skip that for minimal code.

void hdlc_rec_init(void)
{
    memset(hdlc_state, 0, sizeof(hdlc_state));
    // If you have anything else to init, do it here.
}

// Minimal function to feed bits:
void hdlc_rec_bit(int chan, int subchan, int slice,
                  int raw, int is_scrambled, int not_used_remove)
{
    // Use NRZI decoding: '0' => invert prev bit, '1' => same as prev.
    // For minimal approach, let's just do raw (no scramble logic).
    // If you want real NRZI: 
    struct hdlc_state_s *H = &hdlc_state[chan][subchan][slice];

    int dbit = (raw == H->prev_raw) ? 1 : 0;
    H->prev_raw = raw;

    // pattern detect, see if it's 0x7E (01111110)
    H->pat_det >>= 1;
    if(dbit)
        H->pat_det |= 0x80;

    // For a minimal approach, let's parse frames if 0x7E is found:
    if(H->pat_det == 0x7e) {
        // check if we have enough for a frame
        // ...
        // For minimal code, let's just reset:
        H->olen = 0;
        H->frame_len = 0;
        return;
    }

    // bit stuffing check? If needed, etc.
    // For minimal code, skip it or implement it.

    // accumulate bits
    if(H->olen < 0) {
        // means we are ignoring bits if not in a frame
        return;
    }

    H->oacc >>= 1;
    if(dbit) H->oacc |= 0x80;
    H->olen++;

    if(H->olen == 8) {
        if(H->frame_len < MAX_FRAME_LEN) {
            H->frame_buf[H->frame_len++] = H->oacc;
        }
        H->olen = 0;
    }
}

/* 
 * If you want to finalize the frame (e.g. after seeing a flag),
 * you could compute FCS here or pass up the data to some callback.
 */

