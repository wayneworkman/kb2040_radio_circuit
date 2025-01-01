cd ~/git/kb2040_radio_circuit/receive/src/direwolf/c/include
cat > direwolf.h << 'EOF'
#ifndef DIREWOLF_H
#define DIREWOLF_H

#include <stdint.h>
#include <math.h>
#include <stdlib.h>

/* Core FSK parameters */
#define MAX_SUBCHANS 1
#define MAX_SLICERS 1
#define MAX_FILTER_SIZE 1024
#define MAX_CHANS 2

/* Windows for filter generation */
#define BP_WINDOW_TRUNCATED 0 
#define BP_WINDOW_COSINE 1

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
    float pre_filter[MAX_FILTER_SIZE];
    float raw_cb[MAX_FILTER_SIZE];
    float lpf_baud;
    float lp_filter_width_sym;
    int lp_window;
    int lp_filter_taps;
    float lp_filter[MAX_FILTER_SIZE];
    float quick_attack;
    float sluggish_decay;
    float agc_fast_attack;
    float agc_slow_decay;
    float pll_locked_inertia;
    float pll_searching_inertia;
    float alevel_mark_peak;
    float alevel_space_peak;
    int pll_step_per_sample;

    struct {
        int m_osc_phase;
        int m_osc_delta;
        int s_osc_phase;
        int s_osc_delta;
        int c_osc_phase;
        int c_osc_delta;
        float m_I_raw[MAX_FILTER_SIZE];
        float m_Q_raw[MAX_FILTER_SIZE];
        float s_I_raw[MAX_FILTER_SIZE];
        float s_Q_raw[MAX_FILTER_SIZE];
        float c_I_raw[MAX_FILTER_SIZE];
        float c_Q_raw[MAX_FILTER_SIZE];
        float prev_phase;
        float normalize_rpsam;
        int use_rrc;
        float rrc_width_sym;
        float rrc_rolloff;
    } u;

    struct {
        int data_clock_pll;
        int prev_d_c_pll; 
        int prev_demod_data;
        int data_detect;
    } slicer[MAX_SLICERS];
};

#endif
EOF

cat > demod_afsk.h << 'EOF'
#ifndef DEMOD_AFSK_H
#define DEMOD_AFSK_H

#include "direwolf.h"

void demod_afsk_init(int samples_per_sec, int baud, int mark_freq,
                     int space_freq, char profile, 
                     struct demodulator_state_s *D);

void demod_afsk_process_sample(int chan, int subchan, int sam,
                             struct demodulator_state_s *D);

#endif
EOF

cat > hdlc_rec.h << 'EOF'
#ifndef HDLC_REC_H
#define HDLC_REC_H

#include "direwolf.h"

void hdlc_rec_bit(int chan, int subchan, int slice, int raw, 
                  int is_scrambled, int not_used_remove);

#endif
EOF

# Create a README.md since pyproject.toml references it
cd ~/git/kb2040_radio_circuit/receive
cat > README.md << 'EOF'
# KB2040 Radio Circuit

FSK modulation and demodulation for amateur radio using KB2040.
EOF