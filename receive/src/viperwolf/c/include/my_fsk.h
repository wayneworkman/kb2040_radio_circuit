// File: receive/src/direwolf/c/include/my_fsk.h

#ifndef MY_FSK_H
#define MY_FSK_H

#ifdef __cplusplus
extern "C" {
#endif

// Called once per demodulated bit. 'bit' is 0 or 1.
void my_fsk_rec_bit(int bit);

// Retrieve up to 'max_bits' from ring buffer
int my_fsk_get_bits(int *out_bits, int max_bits);

// Clear the ring buffer
void my_fsk_clear_buffer(void);

#ifdef __cplusplus
}
#endif

#endif
