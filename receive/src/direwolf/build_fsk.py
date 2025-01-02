# File: src/direwolf/build_fsk.py

from cffi import FFI
from pathlib import Path

ffibuilder = FFI()
CURRENT_DIR = Path(__file__).parent  # Points to "src/direwolf"

with open(CURRENT_DIR / 'c' / 'include' / 'direwolf.h', 'r') as f:
    direwolf_header = f.read()

with open(CURRENT_DIR / 'c' / 'include' / 'demod_afsk.h', 'r') as f:
    demod_header = f.read()

with open(CURRENT_DIR / 'c' / 'include' / 'hdlc_rec.h', 'r') as f:
    hdlc_header = f.read()

ffibuilder.cdef("""
    struct demodulator_state_s;
    void demod_afsk_init(int samples_per_sec, int baud,
                         int mark_freq, int space_freq,
                         char profile,
                         struct demodulator_state_s *D);
    void demod_afsk_process_sample(int chan, int subchan,
                                   int sam,
                                   struct demodulator_state_s *D);

    // Our custom ring buffer functions:
    void my_fsk_rec_bit(int bit);
    int my_fsk_get_bits(int *out_bits, int max_bits);
    void my_fsk_clear_buffer(void);
""")

ffibuilder.set_source(
    "_fsk_demod",
    """
    #include "direwolf.h"
    #include "demod_afsk.h"
    #include "hdlc_rec.h"
    #include "my_fsk.h"
    """,
    sources=[
        str(CURRENT_DIR / 'c' / 'demod_afsk.c'),
        str(CURRENT_DIR / 'c' / 'hdlc_rec.c'),   # might still be needed if you want to link it; or you can remove
        str(CURRENT_DIR / 'c' / 'my_fsk.c'),     # <- new addition
    ],
    include_dirs=[str(CURRENT_DIR / 'c' / 'include')]
)

if __name__ == "__main__":
    ffibuilder.compile(verbose=True)
