# src/direwolf/python/fsk_wrapper.py
from cffi import FFI
import numpy as np
import logging

class DirewolfFSKDecoder:
    """
    A Python wrapper around direwolf's FSK demodulation capabilities.
    Integrates with your existing AFSK demodulator.
    """
    def __init__(self, sample_rate=48000, baud_rate=300,
                 mark_freq=1200, space_freq=2200):
        # Initialize CFFI
        self.ffi = FFI()
        self._init_ffi()
        
        # Create demodulator state
        self.demod_state = self.ffi.new("struct demodulator_state_s *")
        
        # Initialize demodulator
        self.lib.demod_afsk_init(sample_rate, baud_rate, mark_freq,
                                space_freq, ord('A'), self.demod_state)
        
    def _init_ffi(self):
        """Initialize the FFI interface to C code."""
        self.ffi.cdef("""
            struct demodulator_state_s {
                float m_peak;
                float m_valley;
                float s_peak;
                float s_valley;
                // Add other needed fields
            };
            
            void demod_afsk_init(int samples_per_sec, int baud, 
                               int mark_freq, int space_freq,
                               char profile, 
                               struct demodulator_state_s *D);
                               
            void demod_afsk_process_sample(int chan, int subchan,
                                         int sam,
                                         struct demodulator_state_s *D);
        """)
        
        self.lib = self.ffi.dlopen("_fsk_demod.so")
        
    def process_samples(self, samples):
        """Process a chunk of audio samples."""
        for sample in samples:
            # Scale to 16-bit range expected by direwolf
            scaled = int(sample * 32767)
            self.lib.demod_afsk_process_sample(0, 0, scaled, 
                                             self.demod_state)

# Modified afsk_demod.py (excerpt showing integration)
def demodulator_process(audio_q, message_q):
    """
    Modified demodulator process that can use either the original
    Goertzel detector or the direwolf decoder.
    """
    # Initialize both decoders
    direwolf_decoder = DirewolfFSKDecoder(
        sample_rate=SAMPLE_RATE,
        baud_rate=BAUD_RATE,
        mark_freq=FREQ0,
        space_freq=FREQ1
    )
    
    # Rest of your existing demodulator code...