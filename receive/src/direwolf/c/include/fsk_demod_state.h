/* fsk_demod_state.h */

#ifndef FSK_DEMOD_STATE_H
#define FSK_DEMOD_STATE_H

#include <stdint.h>          // for int64_t

#include "rpack.h"
#include "audio.h"           // We'll still reference 'struct audio_s' for certain logic

/*
 * We no longer reference "enum modem_t" or "enum v26_e".
 * The rest is left mostly intact for AFSK usage.
 */

typedef enum bp_window_e {
    BP_WINDOW_TRUNCATED,
    BP_WINDOW_COSINE,
    BP_WINDOW_HAMMING,
    BP_WINDOW_BLACKMAN,
    BP_WINDOW_FLATTOP
} bp_window_t;

#define CIC_LEN_MAX 4000

typedef struct cic_s {
    int len;
    short in[CIC_LEN_MAX];
    int sum;
    int inext;
} cic_t;

#define MAX_FILTER_SIZE 480

/*
 * Our demodulator struct is simplified: 
 * no 'modem_type', no 'enum v26_e'.
 */
struct demodulator_state_s
{
    // Removed "enum modem_t modem_type;"
    // Removed any v26_alt references

    char profile;   // 'A' or 'B' for AFSK
#define TICKS_PER_PLL_CYCLE ( 256.0 * 256.0 * 256.0 * 256.0 )

    int pll_step_per_sample;

    bp_window_t lp_window;

    int lpf_use_fir;
    float lpf_iir;
    float lpf_baud;
    float lp_filter_width_sym;
    int lp_filter_taps;

    float agc_fast_attack;
    float agc_slow_decay;
    float quick_attack;
    float sluggish_decay;

    float hysteresis;
    int num_slicers;

    float pll_locked_inertia;
    float pll_searching_inertia;

    int use_prefilter;
    float prefilter_baud;
    float pre_filter_len_sym;
    bp_window_t pre_window;
    int pre_filter_taps;

    float pre_filter[MAX_FILTER_SIZE] __attribute__((aligned(16)));
    float raw_cb[MAX_FILTER_SIZE]     __attribute__((aligned(16)));

    unsigned int lo_phase;

    float alevel_rec_peak;
    float alevel_rec_valley;
    float alevel_mark_peak;
    float alevel_space_peak;

    float lp_filter[MAX_FILTER_SIZE]  __attribute__((aligned(16)));

    float m_peak, s_peak;
    float m_valley, s_valley;
    float m_amp_prev, s_amp_prev;

    /*
     * PLL & data bit timing for up to MAX_SLICERS 
     * (we keep 1 if you only want single-slicer).
     */
    struct {
        signed int data_clock_pll;
        signed int prev_d_c_pll;

        int pll_symbol_count;
        int64_t pll_nudge_total;

        int prev_demod_data;
        float prev_demod_out_f;

        int lfsr;  // if you needed 9600 scrambling, else not used
        int good_flag;
        int bad_flag;
        unsigned char good_hist;
        unsigned char bad_hist;
        unsigned int score;
        int data_detect;
    } slicer[MAX_SLICERS];

    union {
        /* AFSK only: */
        struct afsk_only_s {
            unsigned int m_osc_phase;
            unsigned int m_osc_delta;

            unsigned int s_osc_phase;
            unsigned int s_osc_delta;

            unsigned int c_osc_phase;
            unsigned int c_osc_delta;

            float m_I_raw[MAX_FILTER_SIZE] __attribute__((aligned(16)));
            float m_Q_raw[MAX_FILTER_SIZE] __attribute__((aligned(16)));
            float s_I_raw[MAX_FILTER_SIZE] __attribute__((aligned(16)));
            float s_Q_raw[MAX_FILTER_SIZE] __attribute__((aligned(16)));

            float c_I_raw[MAX_FILTER_SIZE] __attribute__((aligned(16)));
            float c_Q_raw[MAX_FILTER_SIZE] __attribute__((aligned(16)));

            int use_rrc;
            float rrc_width_sym;
            float rrc_rolloff;

            float prev_phase;
            float normalize_rpsam;
        } afsk;

        // Removed the "bb_only_s" and "psk_only_s" if you want pure AFSK
    } u;
};

#endif
