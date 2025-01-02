# File: receive/src/viperwolf/build_viperwolf.py

from cffi import FFI
from pathlib import Path
import shutil
import glob
import os

ffibuilder = FFI()
CURRENT_DIR = Path(__file__).parent  # e.g. "receive/src/viperwolf"

# Minimal cdef for single-slicer + ring buffer
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
    "_viperwolf_demod",  # The compiled extension name -> _viperwolf_demod.so
    """
    #include "viperwolf.h"
    #include "demod_afsk.h"
    #include "my_fsk.h"  // ring buffer
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
    # 1) Compile the extension
    ffibuilder.compile(verbose=True)

    # 2) Move the .so file next to viperwolf_wrapper.py
    python_dir = CURRENT_DIR / "python"
    # Typical name is "_viperwolf_demod.so", but on some OS/Python combos
    # it might be named _viperwolf_demod.cpython-310-x86_64-linux-gnu.so, etc.
    # So we search for matching patterns:
    candidates = list(glob.glob(str(CURRENT_DIR / "_viperwolf_demod.so"))) \
               + list(glob.glob(str(CURRENT_DIR / "_viperwolf_demod.*.so"))) \
               + list(glob.glob(str(CURRENT_DIR / "_viperwolf_demod.*.pyd")))

    for path_str in candidates:
        base_name = os.path.basename(path_str)
        dest_path = python_dir / base_name
        print(f"Moving {path_str} -> {dest_path}")
        shutil.move(path_str, dest_path)
