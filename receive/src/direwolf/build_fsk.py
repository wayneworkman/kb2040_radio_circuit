# File: src/direwolf/build_fsk.py
from cffi import FFI

ffibuilder = FFI()

# Load headers
with open('c/include/direwolf.h', 'r') as f:
    direwolf_header = f.read()

with open('c/include/demod_afsk.h', 'r') as f:
    demod_header = f.read()

with open('c/include/hdlc_rec.h', 'r') as f:
    hdlc_header = f.read()

# Parse header content and extract struct definition and function declarations
ffibuilder.cdef("""
    struct demodulator_state_s {
        float m_peak;
        float m_valley;
        float s_peak;
        float s_valley;
        char profile;
        int num_slicers;
        int use_prefilter;
        float prefilter_baud;
        float pre_filter_len_sym;
        int pre_window;
        int pre_filter_taps;
        float pre_filter[1024];
        float raw_cb[1024];
        ...;  // This tells CFFI there are other fields we don't care about
    };
    
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

ffibuilder.set_source("_fsk_demod",
    """
    #include "direwolf.h"
    #include "demod_afsk.h"
    #include "hdlc_rec.h"
    """,
    sources=['c/demod_afsk.c', 'c/hdlc_rec.c'],
    include_dirs=['c/include']
)

if __name__ == "__main__":
    ffibuilder.compile(verbose=True)