from cffi import FFI
from pathlib import Path

ffibuilder = FFI()
CURRENT_DIR = Path(__file__).parent

# Read your .h files, then:
ffibuilder.cdef("""
    ...
""")

ffibuilder.set_source("_fsk_demod",
    """
    #include "direwolf.h"
    #include "demod_afsk.h"
    #include "hdlc_rec.h"
    """,
    sources=[
        str(CURRENT_DIR / 'c/demod_afsk.c'),
        str(CURRENT_DIR / 'c/hdlc_rec.c')
    ],
    include_dirs=[str(CURRENT_DIR / 'c/include')]
)

if __name__ == "__main__":
    ffibuilder.compile(verbose=True)
