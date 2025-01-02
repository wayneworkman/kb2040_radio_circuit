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

# Then use cdef(...) with direwolf_header, demod_header, hdlc_header if needed.
# or do everything inline as you have now.

ffibuilder.cdef("""

    // Forward-declare the struct using the same tag name
    // that your C code uses in function params:
    struct demodulator_state_s;

    // Now declare the functions exactly matching your .h file:
    void demod_afsk_init(int samples_per_sec, int baud,
                         int mark_freq, int space_freq,
                         char profile,
                         struct demodulator_state_s *D);

    void demod_afsk_process_sample(int chan, int subchan,
                                   int sam,
                                   struct demodulator_state_s *D);

    void hdlc_rec_bit(int chan, int subchan, int slice,
                      int raw, int is_scrambled, int not_used_remove);

""")



ffibuilder.set_source(
    "_fsk_demod",
    """
    #include "direwolf.h"
    #include "demod_afsk.h"
    #include "hdlc_rec.h"
    """,
    sources=[
        str(CURRENT_DIR / 'c' / 'demod_afsk.c'),
        # str(CURRENT_DIR / 'c' / 'hdlc_rec.c'),     # KEEP THIS if it's the correct (only) version
    ],
    include_dirs=[str(CURRENT_DIR / 'c' / 'include')]
)

if __name__ == "__main__":
    ffibuilder.compile(verbose=True)
