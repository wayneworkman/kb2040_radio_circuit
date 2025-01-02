# File: receive/src/direwolf/build_fsk.py

from cffi import FFI
from pathlib import Path

ffibuilder = FFI()
CURRENT_DIR = Path(__file__).parent  # e.g. "src/direwolf"

# We do not need to read any .h file contents. This is optional:
with open(CURRENT_DIR / 'c' / 'include' / 'direwolf.h', 'r') as f:
    direwolf_header = f.read()
with open(CURRENT_DIR / 'c' / 'include' / 'demod_afsk.h', 'r') as f:
    demod_header = f.read()
with open(CURRENT_DIR / 'c' / 'include' / 'my_fsk.h', 'r') as f:
    myfsk_header = f.read()

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
    "_fsk_demod",
    # This string is pasted verbatim into _fsk_demod.c:
    """
    #include "direwolf.h"
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
