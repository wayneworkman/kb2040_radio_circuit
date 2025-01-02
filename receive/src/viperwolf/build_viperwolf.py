# File: receive/src/viperwolf/build_viperwolf.py

from cffi import FFI
from pathlib import Path
import shutil
import glob
import os

ffibuilder = FFI()
CURRENT_DIR = Path(__file__).parent  # e.g. "src/viperwolf"

# Minimal cdef for single-slicer + ring buffer:
ffibuilder.cdef("""
    struct demodulator_state_s;

    void demod_afsk_init(int samples_per_sec, int baud,
                         int mark_freq, int space_freq,
                         char profile,
                         struct demodulator_state_s *D);

    void demod_afsk_process_sample(int chan, int subchan,
                                   int sam,
                                   struct demodulator_state_s *D);

    // Our ring buffer
    void my_fsk_rec_bit(int bit);
    int my_fsk_get_bits(int *out_bits, int max_bits);
    void my_fsk_clear_buffer(void);
""")

ffibuilder.set_source(
    "_viperwolf_demod",  # name of the output extension (creates _viperwolf_demod.so)
    """
    #include "viperwolf.h"
    #include "demod_afsk.h"
    #include "my_fsk.h"  // ring buffer
    """,
    sources=[
        str(CURRENT_DIR / "c" / "demod_afsk.c"),
        str(CURRENT_DIR / "c" / "my_fsk.c"),
    ],
    include_dirs=[str(CURRENT_DIR / "c" / "include")]
)

if __name__ == "__main__":
    # 1) Build the extension
    ffibuilder.compile(verbose=True)

    # 2) Move the resulting .so file to 'python/' so it's next to viperwolf_wrapper.py
    so_candidates = glob.glob(str(CURRENT_DIR / "_viperwolf_demod.so")) \
                   + glob.glob(str(CURRENT_DIR / "_viperwolf_demod.*.so")) \
                   + glob.glob("_viperwolf_demod.*.pyd")  # Windows?
    for sf in so_candidates:
        dest = CURRENT_DIR / "python" / os.path.basename(sf)
        print(f"Moving {sf} -> {dest}")
        shutil.move(sf, dest)
