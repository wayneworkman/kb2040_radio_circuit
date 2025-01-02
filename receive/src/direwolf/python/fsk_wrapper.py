# File: receive/src/direwolf/python/fsk_wrapper.py

from cffi import FFI
import numpy as np

class DirewolfFSKDecoder:
    def __init__(self, sample_rate=48000, baud_rate=300,
                 mark_freq=1200, space_freq=2200):
        self.ffi = FFI()
        self._init_ffi()

        # Allocate our demod_state on the heap:
        self.demod_state = self.ffi.new("struct demodulator_state_s *")

        # Initialize the demod:
        self.lib.demod_afsk_init(
            sample_rate,
            baud_rate,
            mark_freq,
            space_freq,
            ord('A'),    # or 'B' if you prefer
            self.demod_state
        )

    def _init_ffi(self):
        self.ffi.cdef("""
            typedef struct demodulator_state_s demodulator_state_s;

            void demod_afsk_init(int, int, int, int, char, demodulator_state_s*);
            void demod_afsk_process_sample(int, int, int, demodulator_state_s*);

            void my_fsk_rec_bit(int bit);
            int my_fsk_get_bits(int *out_bits, int max_bits);
            void my_fsk_clear_buffer(void);
        """)
        # Load the just-compiled shared object:
        self.lib = self.ffi.dlopen("_fsk_demod.so")

    def process_samples(self, samples):
        """
        Feed a numpy array of float32 samples, e.g. from a mic input,
        scaled to [-1..+1], into the demod. We'll do single-slicer logic.
        """
        for sample in samples:
            scaled = int(sample * 32767)
            self.lib.demod_afsk_process_sample(0, 0, scaled, self.demod_state)

    def get_raw_bits(self, max_bits=1024):
        """
        Retrieve up to 'max_bits' bits from the ring buffer in C.
        Return them as a Python list of 0/1.
        """
        out_array = self.ffi.new("int[]", max_bits)
        count = self.lib.my_fsk_get_bits(out_array, max_bits)
        return [out_array[i] for i in range(count)]

    def clear_ring_buffer(self):
        self.lib.my_fsk_clear_buffer()
