# File: receive/src/viperwolf/python/viperwolf_wrapper.py

import os
from cffi import FFI

class ViperwolfFSKDecoder:
    def __init__(self, sample_rate=48000, baud_rate=300,
                 mark_freq=1200, space_freq=2200):
        self.ffi = FFI()
        self._init_ffi()

        # Allocate demodulator state
        self.demod_state = self.ffi.new("struct demodulator_state_s *")

        # Initialize the demod
        self.lib.demod_afsk_init(
            sample_rate,
            baud_rate,
            mark_freq,
            space_freq,
            ord('A'),  # or 'B'
            self.demod_state
        )

    def _init_ffi(self):
        # Provide the function prototypes
        self.ffi.cdef("""
            typedef struct demodulator_state_s demodulator_state_s;

            void demod_afsk_init(int, int, int, int, char, demodulator_state_s*);
            void demod_afsk_process_sample(int, int, int, demodulator_state_s*);

            void my_fsk_rec_bit(int bit);
            int my_fsk_get_bits(int *out_bits, int max_bits);
            void my_fsk_clear_buffer(void);
        """)

        # Figure out the absolute path to _viperwolf_demod.so next to this file
        current_dir = os.path.dirname(__file__)
        so_path = os.path.join(current_dir, "_viperwolf_demod.so")

        # Now open that .so
        self.lib = self.ffi.dlopen(so_path)

    def process_samples(self, samples):
        """
        Feed a list/array of float samples [-1..+1].
        """
        for sample in samples:
            scaled = int(sample * 32767)
            self.lib.demod_afsk_process_sample(0, 0, scaled, self.demod_state)

    def get_raw_bits(self, max_bits=1024):
        """
        Retrieve up to 'max_bits' bits from ring buffer in C.
        """
        out_array = self.ffi.new("int[]", max_bits)
        count = self.lib.my_fsk_get_bits(out_array, max_bits)
        return [out_array[i] for i in range(count)]

    def clear_ring_buffer(self):
        self.lib.my_fsk_clear_buffer()
