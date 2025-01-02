# File: receive/src/viperwolf/build_viperwolf.py

import sys
import os
import glob
import shutil
from pathlib import Path
from cffi import FFI

print("** BUILDING VIPERWOLF EXTENSION (with dsp.c) **", file=sys.stderr)

ffibuilder = FFI()
CURRENT_DIR = Path(__file__).parent

# Our minimal cdef
ffibuilder.cdef(r"""
    struct demodulator_state_s;

    void demod_afsk_init(int samples_per_sec, int baud,
                         int mark_freq, int space_freq,
                         char profile,
                         struct demodulator_state_s *D);

    void demod_afsk_process_sample(int chan, int subchan,
                                   int sam,
                                   struct demodulator_state_s *D);

    void my_fsk_rec_bit(int bit);
    int my_fsk_get_bits(int *out_bits, int max_bits);
    void my_fsk_clear_buffer(void);
""")

ffibuilder.set_source(
    "_viperwolf_demod",
    r'''
    #include "viperwolf.h"
    #include "demod_afsk.h"
    #include "my_fsk.h"
    ''',
    sources=[
        # Build the c files needed:
        str(CURRENT_DIR / "c" / "demod_afsk.c"),
        str(CURRENT_DIR / "c" / "my_fsk.c"),
        str(CURRENT_DIR / "c" / "dsp.c"),
        str(CURRENT_DIR / "c" / "textcolor.c"),
    ],
    include_dirs=[str(CURRENT_DIR / "c" / "include")]
)

if __name__ == "__main__":
    # 1) Actually compile the extension into a temporary build dir:
    tmpdir = CURRENT_DIR / "build_viperwolf_temp"
    tmpdir.mkdir(exist_ok=True)
    print(f"** Using tmpdir={tmpdir}", file=sys.stderr)
    module_filename = ffibuilder.compile(
        verbose=True,
        tmpdir=str(tmpdir)
    )
    print(f"** CFFI compile produced module_filename={module_filename}", file=sys.stderr)

    # 2) We want to find _viperwolf_demod.*.so or pyd/dll in both tmpdir & CURRENT_DIR
    patterns = [
        "_viperwolf_demod.so",
        "_viperwolf_demod.*.so",
        "_viperwolf_demod.*.pyd",
        "_viperwolf_demod.*.dll",
    ]
    matched = []
    # Search in tmpdir first:
    for pat in patterns:
        matched.extend(glob.glob(str(tmpdir / pat)))
    # Also check CURRENT_DIR
    for pat in patterns:
        matched.extend(glob.glob(str(CURRENT_DIR / pat)))

    # 3) If found, move next to viperwolf_wrapper.py, rename to _viperwolf_demod.so
    python_dir = CURRENT_DIR / "python"
    python_dir.mkdir(exist_ok=True)

    if not matched:
        print("WARNING: No compiled _viperwolf_demod.* file found to move!", file=sys.stderr)
    else:
        # Move & rename each match. Usually there's only 1 match on Linux.
        for srcpath in matched:
            base = os.path.basename(srcpath)
            final_dest = python_dir / "_viperwolf_demod.so"  # force rename
            print(f"Moving {srcpath} -> {final_dest}", file=sys.stderr)
            if final_dest.exists():
                final_dest.unlink()  # remove old version if it exists
            shutil.move(srcpath, final_dest)

    print("** Done build_viperwolf.py", file=sys.stderr)
