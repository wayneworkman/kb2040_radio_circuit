// File: receive/src/direwolf/c/include/fsk_demod_state.h

#ifndef FSK_DEMOD_STATE_H
#define FSK_DEMOD_STATE_H

#include <stdint.h>

// minimal window enum
typedef enum bp_window_e {
    BP_WINDOW_TRUNCATED,
    BP_WINDOW_COSINE
} bp_window_t;

#define TICKS_PER_PLL_CYCLE (256.0*256.0*256.0*256.0)
#define MAX_FILTER_SIZE 480

struct demodulator_state_s {
    char profile; // 'A' or 'B'

    int pll_step_per_sample;

    bp_window_t lp_window;

    float lpf_baud;
    float lp_filter_width_sym;
    int lp_filter_taps;

    float agc_fast_attack;
    float agc_slow_decay;

    float pll_locked_inertia;
    float pll_searching_inertia;

    int use_prefilter;
    float prefilter_baud;
    float pre_filter_len_sym;
    bp_window_t pre_window;
    int pre_filter_taps;

    float pre_filter[MAX_FILTER_SIZE];
    float raw_cb[MAX_FILTER_SIZE];

    float lp_filter[MAX_FILTER_SIZE];

    int num_slicers;
    float m_peak,s_peak;
    float m_valley,s_valley;

    // optional alevel:
    float alevel_mark_peak;
    float alevel_space_peak;

    union {
        // AFSK only
        struct {
            unsigned int m_osc_phase;
            unsigned int m_osc_delta;
            unsigned int s_osc_phase;
            unsigned int s_osc_delta;

            unsigned int c_osc_phase;
            unsigned int c_osc_delta;

            float m_I_raw[MAX_FILTER_SIZE];
            float m_Q_raw[MAX_FILTER_SIZE];
            float s_I_raw[MAX_FILTER_SIZE];
            float s_Q_raw[MAX_FILTER_SIZE];

            float c_I_raw[MAX_FILTER_SIZE];
            float c_Q_raw[MAX_FILTER_SIZE];

            int use_rrc;
            float rrc_width_sym;
            float rrc_rolloff;

            float prev_phase;
            float normalize_rpsam;
        } afsk;
    } u;

    // Single slicer:
    struct {
        signed int data_clock_pll;
        signed int prev_d_c_pll;

        int prev_demod_data;
        int data_detect;
    } slicer[1];
};

#endif
