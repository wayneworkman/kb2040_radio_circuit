# File: receive/src/viperwolf/build_viperwolf.py

from cffi import FFI
from pathlib import Path

ffibuilder = FFI()
CURRENT_DIR = Path(__file__).parent  # e.g. "src/viperwolf"

# Minimal cdef for single-slicer + raw bit ring buffer:
ffibuilder.cdef("""
    struct demodulator_state_s;

    void demod_afsk_init(int samples_per_sec, int baud,
                         int mark_freq, int space_freq,
                         char profile,
                         struct demodulator_state_s *D);

    void demod_afsk_process_sample(int chan, int subchan,
                                   int sam,
                                   struct demodulator_state_s *D);

    // Custom ring buffer
    void my_fsk_rec_bit(int bit);
    int my_fsk_get_bits(int *out_bits, int max_bits);
    void my_fsk_clear_buffer(void);
""")

ffibuilder.set_source(
    "_viperwolf_demod",  # Will produce _viperwolf_demod.so (or similar)
    """
    #include "viperwolf.h"
    #include "demod_afsk.h"
    #include "my_fsk.h"  // For the ring buffer
    """,
    sources=[
        str(CURRENT_DIR / "c" / "demod_afsk.c"),
        str(CURRENT_DIR / "c" / "my_fsk.c"),
    ],
    include_dirs=[
        str(CURRENT_DIR / "c" / "include")
    ]
)

if __name__ == "__main__":
    ffibuilder.compile(verbose=True)
