/*
 * File: my_fsk.c
 * Where it goes: receive/src/direwolf/c/my_fsk.c
 *
 * This file implements a ring buffer to capture raw bits from
 * my_fsk_rec_bit(). Then we expose a function to retrieve them
 * from Python via CFFI.
 */

#include <string.h>
#include "my_fsk.h"

// A small ring buffer for bits.
#define MY_FSK_RING_SIZE 8192
static int s_bit_ring[MY_FSK_RING_SIZE];
static int s_head = 0;
static int s_tail = 0;

/*
 * Called by demod_afsk_process_sample(...) whenever we have a new bit.
 * 'bit' is 1 or 0.
 */
void my_fsk_rec_bit(int bit)
{
    int next_head = (s_head + 1) % MY_FSK_RING_SIZE;
    if (next_head == s_tail) {
        // Buffer is full; you could overwrite or drop. We'll drop for safety.
        // If you want to overwrite, do s_tail = (s_tail + 1) % MY_FSK_RING_SIZE
        return;
    }
    s_bit_ring[s_head] = bit;
    s_head = next_head;
}

/*
 * Retrieve up to 'max_bits' bits from the ring buffer.
 * Returns how many bits were actually copied into out_bits.
 */
int my_fsk_get_bits(int *out_bits, int max_bits)
{
    int count = 0;
    while ((count < max_bits) && (s_tail != s_head)) {
        out_bits[count] = s_bit_ring[s_tail];
        s_tail = (s_tail + 1) % MY_FSK_RING_SIZE;
        count++;
    }
    return count;
}

/*
 * Clear the ring buffer, e.g. if you want to do a fresh capture.
 */
void my_fsk_clear_buffer(void)
{
    s_head = 0;
    s_tail = 0;
    memset(s_bit_ring, 0, sizeof(s_bit_ring));
}
