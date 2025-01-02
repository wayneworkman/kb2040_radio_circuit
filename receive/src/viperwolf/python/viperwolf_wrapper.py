# File: receive/src/viperwolf/python/viperwolf_wrapper.py

import os
from cffi import FFI

class ViperwolfFSKDecoder:
    def __init__(self, sample_rate=48000, baud_rate=300,
                 mark_freq=1200, space_freq=2200):
        self.ffi = FFI()
        self._init_ffi()

        # ---- Create a new demodulator_state_s using the factory in C.
        self.demod_state = self.lib.create_demodulator_state()
        if not self.demod_state:
            raise MemoryError("create_demodulator_state() returned NULL (allocation failed)")

        # Now call demod_afsk_init on that allocated pointer:
        self.lib.demod_afsk_init(
            sample_rate,
            baud_rate,
            mark_freq,
            space_freq,
            ord('A'),  # or 'B'
            self.demod_state
        )

    def _init_ffi(self):
        self.ffi.cdef("""
            typedef struct demodulator_state_s demodulator_state_s;

            demodulator_state_s * create_demodulator_state(void);
            void free_demodulator_state(demodulator_state_s *p);

            void demod_afsk_init(int, int, int, int, char, demodulator_state_s*);
            void demod_afsk_process_sample(int, int, int, demodulator_state_s*);

            void my_fsk_rec_bit(int bit);
            int my_fsk_get_bits(int *out_bits, int max_bits);
            void my_fsk_clear_buffer(void);
        """)

        # The .so is placed next to this file by build_viperwolf.py
        current_dir = os.path.dirname(__file__)
        so_path = os.path.join(current_dir, "_viperwolf_demod.so")
        self.lib = self.ffi.dlopen(so_path)

    def __del__(self):
        """
        Optional destructor to free the allocated struct and avoid memory leaks.
        """
        if getattr(self, 'demod_state', None):
            self.lib.free_demodulator_state(self.demod_state)
            self.demod_state = None

    def process_samples(self, samples):
        """
        Feed a list or array of float samples [-1..+1].
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
