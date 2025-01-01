# src/direwolf/build_fsk.py
from setuptools import setup, Extension
from cffi import FFI

ffibuilder = FFI()

# Read our C headers
with open('c/direwolf.h', 'r') as f:
    direwolf_header = f.read()
    
with open('c/demod_afsk.h', 'r') as f:
    demod_header = f.read()

# Define the C interface
ffibuilder.cdef("""
    struct demodulator_state_s {
        float m_peak;
        float m_valley;
        float s_peak;
        float s_valley;
    };
    
    void demod_afsk_init(int samples_per_sec, int baud,
                        int mark_freq, int space_freq,
                        char profile, 
                        struct demodulator_state_s *D);
                        
    void demod_afsk_process_sample(int chan, int subchan,
                                 int sam,
                                 struct demodulator_state_s *D);
""")

# Create the extension module
ffibuilder.set_source("_fsk_demod",
    """
    #include "direwolf.h"
    #include "demod_afsk.h"
    """,
    sources=['c/demod_afsk.c'],
    include_dirs=['c']
)

if __name__ == "__main__":
    ffibuilder.compile(verbose=True)

# build.sh
#!/bin/bash

# Ensure we're in the project root
cd "$(dirname "$0")"

# Create virtual environment if it doesn't exist 
if [ ! -d "venv" ]; then
    python3 -m venv venv
fi

# Activate virtual environment
source venv/bin/activate

# Install requirements
pip install --upgrade pip
pip install cffi numpy wheel setuptools sounddevice

# Build the FSK module
cd src/direwolf
python build_fsk.py

echo "Build complete!"