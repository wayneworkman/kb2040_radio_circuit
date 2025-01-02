// File: receive/src/direwolf/c/demod_afsk.c
// Where it goes in your directory tree:
//   receive/src/direwolf/c/demod_afsk.c

//
//    This file is part of Dire Wolf, an amateur radio packet TNC.
//    Simplified here for a single subchannel / single-slicer scenario.
//
//    Copyright (C) 2011 ... 2020  John Langner, WB2OSZ
//    Licensed under GNU GPL v2 (or later) as originally stated.
//

#include "demod_afsk.h"        // Prototypes only
#include "audio.h"
#include "fsk_demod_state.h"
#include "fsk_gen_filter.h"
// If you don't want HDLC logic at all, remove #include "hdlc_rec.h"
#include "hdlc_rec.h"
#include "textcolor.h"
#include "direwolf.h"
#include "dsp.h"

// ------------------------------------------------------------------
// We define a single-slicer approach, so num_slicers=1, no loops over
// multiple slicers, etc.
// ------------------------------------------------------------------

// Quick macros:
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

#define TUNE(envvar,param,name,fmt) {                               \
    char *e = getenv(envvar);                                       \
    if (e != NULL) {                                                \
      param = atof(e);                                              \
      text_color_set (DW_COLOR_ERROR);                              \
      dw_printf ("TUNE: " name " = " fmt "\n", param);              \
    } }

#define MIN_G 0.5f
#define MAX_G 4.0f

// Cosine table indexed by unsigned byte:
static float fcos256_table[256];

// Inline macros for oscillator usage:
#define fcos256(x) (fcos256_table[((x)>>24)&0xff])
#define fsin256(x) (fcos256_table[(((x)>>24)-64)&0xff])

// Forward declare our internal helper:
static void nudge_pll(int chan, int subchan, float demod_out,
                      struct demodulator_state_s *D, float amplitude);

// Quick approximation to sqrt(x*x + y*y):
static inline float fast_hypot(float x, float y)
{
    return hypotf(x,y);
}

// Slide samples in a ring buffer:
static inline void push_sample(float val, float *buff, int size)
{
    memmove(buff + 1, buff, (size - 1) * sizeof(float));
    buff[0] = val;
}

// FIR filter kernel:
static inline float convolve(const float *data, const float *filter, int filter_taps)
{
    float sum = 0.0f;
    for (int j = 0; j < filter_taps; j++) {
        sum += filter[j] * data[j];
    }
    return sum;
}

// AGC for single slicer (mark vs. space):
static inline float agc(float in, float fast_attack, float slow_decay,
                        float *ppeak, float *pvalley)
{
    if (in >= *ppeak) {
      *ppeak = in * fast_attack + *ppeak * (1.0f - fast_attack);
    } else {
      *ppeak = in * slow_decay + *ppeak * (1.0f - slow_decay);
    }

    if (in <= *pvalley) {
      *pvalley = in * fast_attack + *pvalley * (1.0f - fast_attack);
    } else {
      *pvalley = in * slow_decay + *pvalley * (1.0f - slow_decay);
    }

    float x = in;
    if (x > *ppeak)   x = *ppeak;
    if (x < *pvalley) x = *pvalley;

    // If peak>valley, compute normalized range, else return 0.
    if (*ppeak > *pvalley) {
      return (x - 0.5f * (*ppeak + *pvalley)) / (*ppeak - *pvalley);
    }
    return 0.0f;
}

// ------------------------------------------------------------------
// demod_afsk_init: single-slicer init
// ------------------------------------------------------------------
void demod_afsk_init(int samples_per_sec,
                     int baud,
                     int mark_freq,
                     int space_freq,
                     char profile,
                     struct demodulator_state_s *D)
{
    // Build a small cosine table:
    for (int j = 0; j < 256; j++) {
        fcos256_table[j] = cosf((float)j * 2.0f * (float)M_PI / 256.0f);
    }

    // Clear D:
    memset(D, 0, sizeof(*D));
    D->num_slicers = 1;  // We do single-slicer only.

    // Store the profile:
    D->profile = profile;

    // Possibly differentiate 'A' or 'B' in your code:
    // but we still only do 1 slicer. We'll keep typical code from your original.

    // Basic TUNE environment override logic:
    // (Used by some older Direwolf code, but you can keep or remove.)
    TUNE("TUNE_USE_RRC", D->u.afsk.use_rrc, "use_rrc", "%d")

    // Decide initial filter logic based on profile:
    switch (D->profile) {
      case 'A':
      case 'E':
        // 'A' style
        D->use_prefilter = 1;
        D->prefilter_baud = (baud > 600) ? 0.155f : 0.87f;
        D->pre_filter_len_sym = (baud > 600) ? (383 * 1200.0f / 44100.0f) : 1.857f;
        D->pre_window = BP_WINDOW_TRUNCATED;

        D->u.afsk.m_osc_phase = 0;
        D->u.afsk.m_osc_delta = (unsigned int) round(pow(2., 32.) * (double)mark_freq / (double)samples_per_sec);

        D->u.afsk.s_osc_phase = 0;
        D->u.afsk.s_osc_delta = (unsigned int) round(pow(2., 32.) * (double)space_freq / (double)samples_per_sec);

        // Use Root Raised Cosine?
        D->u.afsk.use_rrc = 1;
        D->u.afsk.rrc_width_sym = 2.80f;
        D->u.afsk.rrc_rolloff   = 0.20f;

        D->lpf_baud = 0.14f; // fallback if use_rrc=0
        D->lp_filter_width_sym = 1.388f;

        D->agc_fast_attack = 0.70f;
        D->agc_slow_decay  = 0.000090f;

        D->pll_locked_inertia    = 0.74f;
        D->pll_searching_inertia = 0.50f;
        break;

      case 'B':
      case 'D':
        // 'B' style
        D->use_prefilter = 1;
        D->prefilter_baud = (baud > 600) ? 0.19f : 0.87f;
        D->pre_filter_len_sym = (baud > 600) ? 8.163f : 1.857f;
        D->pre_window = BP_WINDOW_TRUNCATED;

        D->u.afsk.c_osc_phase = 0;
        D->u.afsk.c_osc_delta = (unsigned int) round(pow(2., 32.) *
                                 0.5 * (mark_freq + space_freq) / (double)samples_per_sec);

        D->u.afsk.use_rrc = 1;
        D->u.afsk.rrc_width_sym = 2.00f;
        D->u.afsk.rrc_rolloff   = 0.40f;

        D->lpf_baud = 0.50f; // fallback if use_rrc=0
        D->lp_filter_width_sym = 1.714286f;

        D->u.afsk.normalize_rpsam = 1.0f / (0.5f * fabsf((float)mark_freq - (float)space_freq) *
                                            2.0f * (float)M_PI / (float)samples_per_sec);

        D->agc_fast_attack = 0.70f;
        D->agc_slow_decay  = 0.000090f;

        D->pll_locked_inertia    = 0.74f;
        D->pll_searching_inertia = 0.50f;

        D->alevel_mark_peak  = -1.0f;
        D->alevel_space_peak = -1.0f;
        break;

      default:
        text_color_set(DW_COLOR_ERROR);
        dw_printf("Invalid AFSK demodulator profile = %c\n", profile);
        exit(1);
    }

    // TUNE overrides if set:
    TUNE("TUNE_PRE_BAUD", D->prefilter_baud, "prefilter_baud", "%.3f")
    TUNE("TUNE_PRE_WINDOW", D->pre_window, "pre_window", "%d")

    TUNE("TUNE_LPF_BAUD", D->lpf_baud, "lpf_baud", "%.3f")
    TUNE("TUNE_LP_WINDOW", D->lp_window, "lp_window", "%d")

    TUNE("TUNE_RRC_ROLLOFF", D->u.afsk.rrc_rolloff, "rrc_rolloff", "%.2f")
    TUNE("TUNE_RRC_WIDTH_SYM", D->u.afsk.rrc_width_sym, "rrc_width_sym", "%.2f")

    TUNE("TUNE_AGC_FAST", D->agc_fast_attack, "agc_fast_attack", "%.3f")
    TUNE("TUNE_AGC_SLOW", D->agc_slow_decay, "agc_slow_decay", "%.6f")

    TUNE("TUNE_PLL_LOCKED", D->pll_locked_inertia, "pll_locked_inertia", "%.2f")
    TUNE("TUNE_PLL_SEARCHING", D->pll_searching_inertia, "pll_searching_inertia", "%.2f")

    // Compute D->pll_step_per_sample for the bit timing:
    if (baud == 521) {
      // odd special case from older code
      D->pll_step_per_sample = (int) round((TICKS_PER_PLL_CYCLE * 520.83) / (double)samples_per_sec);
    } else {
      D->pll_step_per_sample = (int) round((TICKS_PER_PLL_CYCLE * (double)baud) /
                                           (double)samples_per_sec);
    }

    // Pre-filter:
    if (D->use_prefilter) {
      D->pre_filter_taps = (int)(D->pre_filter_len_sym *
                                 (float)samples_per_sec / (float)baud);
      // Round up to odd number:
      D->pre_filter_taps = (D->pre_filter_taps | 1);

      TUNE("TUNE_PRE_FILTER_TAPS", D->pre_filter_taps, "pre_filter_taps", "%d")
      if (D->pre_filter_taps > MAX_FILTER_SIZE) {
        text_color_set(DW_COLOR_ERROR);
        dw_printf("Warning: pre_filter_taps of %d is too large.\n", D->pre_filter_taps);
        D->pre_filter_taps = (MAX_FILTER_SIZE - 1) | 1;
      }

      float f1 = MIN(mark_freq, space_freq) - D->prefilter_baud * baud;
      float f2 = MAX(mark_freq, space_freq) + D->prefilter_baud * baud;
      f1 /= (float)samples_per_sec;
      f2 /= (float)samples_per_sec;

      gen_bandpass(f1, f2, D->pre_filter, D->pre_filter_taps, D->pre_window);
    }

    // Low-pass or RRC filter:
    if (D->u.afsk.use_rrc) {
      D->lp_filter_taps = (int)(D->u.afsk.rrc_width_sym *
                                (float)samples_per_sec / baud);
      D->lp_filter_taps = (D->lp_filter_taps | 1);

      TUNE("TUNE_LP_FILTER_TAPS", D->lp_filter_taps, "lp_filter_taps (RRC)", "%d")
      if (D->lp_filter_taps > MAX_FILTER_SIZE) {
        text_color_set(DW_COLOR_ERROR);
        dw_printf("Calculated RRC low pass filter size %d too large.\n", D->lp_filter_taps);
        D->lp_filter_taps = (MAX_FILTER_SIZE - 1) | 1;
      }
      if (D->lp_filter_taps < 9) {
        D->lp_filter_taps = 9;
      }
      gen_rrc_lowpass(D->lp_filter, D->lp_filter_taps,
                      D->u.afsk.rrc_rolloff,
                      (float)samples_per_sec / (float)baud);
    } else {
      D->lp_filter_taps = (int) round(D->lp_filter_width_sym *
                                      (float)samples_per_sec / (float)baud);

      TUNE("TUNE_LP_FILTER_TAPS", D->lp_filter_taps, "lp_filter_taps (FIR)", "%d")
      if (D->lp_filter_taps > MAX_FILTER_SIZE) {
        text_color_set(DW_COLOR_ERROR);
        dw_printf("Calculated FIR LP filter size %d too large.\n", D->lp_filter_taps);
        D->lp_filter_taps = (MAX_FILTER_SIZE - 1) | 1;
      }
      if (D->lp_filter_taps < 9) {
        D->lp_filter_taps = 9;
      }

      float fc = baud * D->lpf_baud / (float)samples_per_sec;
      gen_lowpass(fc, D->lp_filter, D->lp_filter_taps, D->lp_window);
    }

    // End of single-slicer init
}

// ------------------------------------------------------------------
// demod_afsk_process_sample: single-slicer logic
// ------------------------------------------------------------------
void demod_afsk_process_sample(int chan,
                               int subchan,
                               int sam,
                               struct demodulator_state_s *D)
{
    // Scale incoming sample:
    float fsam = (float)sam / 16384.0f;

    // Profile logic determines method:
    switch (D->profile) {

      case 'A':
      case 'E': {
        // Basic “A” style demod with mark/space oscillators:

        // Optional bandpass pre-filter:
        if (D->use_prefilter) {
          push_sample(fsam, D->raw_cb, D->pre_filter_taps);
          fsam = convolve(D->raw_cb, D->pre_filter, D->pre_filter_taps);
        }

        // Multiply by mark oscillator (I/Q):
        push_sample(fsam * fcos256(D->u.afsk.m_osc_phase),
                    D->u.afsk.m_I_raw,
                    D->lp_filter_taps);
        push_sample(fsam * fsin256(D->u.afsk.m_osc_phase),
                    D->u.afsk.m_Q_raw,
                    D->lp_filter_taps);
        D->u.afsk.m_osc_phase += D->u.afsk.m_osc_delta;

        // Multiply by space oscillator (I/Q):
        push_sample(fsam * fcos256(D->u.afsk.s_osc_phase),
                    D->u.afsk.s_I_raw,
                    D->lp_filter_taps);
        push_sample(fsam * fsin256(D->u.afsk.s_osc_phase),
                    D->u.afsk.s_Q_raw,
                    D->lp_filter_taps);
        D->u.afsk.s_osc_phase += D->u.afsk.s_osc_delta;

        // Convolve with LPF / RRC:
        float m_I = convolve(D->u.afsk.m_I_raw, D->lp_filter, D->lp_filter_taps);
        float m_Q = convolve(D->u.afsk.m_Q_raw, D->lp_filter, D->lp_filter_taps);
        float m_amp = fast_hypot(m_I, m_Q);

        float s_I = convolve(D->u.afsk.s_I_raw, D->lp_filter, D->lp_filter_taps);
        float s_Q = convolve(D->u.afsk.s_Q_raw, D->lp_filter, D->lp_filter_taps);
        float s_amp = fast_hypot(s_I, s_Q);

        // If you want to track amplitude peaks, you can do something like:
        if (m_amp >= D->alevel_mark_peak) {
          D->alevel_mark_peak = m_amp * D->quick_attack
                                + D->alevel_mark_peak * (1.0f - D->quick_attack);
        } else {
          D->alevel_mark_peak = m_amp * D->sluggish_decay
                                + D->alevel_mark_peak * (1.0f - D->sluggish_decay);
        }
        if (s_amp >= D->alevel_space_peak) {
          D->alevel_space_peak = s_amp * D->quick_attack
                                 + D->alevel_space_peak * (1.0f - D->quick_attack);
        } else {
          D->alevel_space_peak = s_amp * D->sluggish_decay
                                 + D->alevel_space_peak * (1.0f - D->sluggish_decay);
        }

        // Single-slicer approach: do AGC on each tone, compare difference:
        float m_norm = agc(m_amp, D->agc_fast_attack, D->agc_slow_decay,
                           &D->m_peak, &D->m_valley);
        float s_norm = agc(s_amp, D->agc_fast_attack, D->agc_slow_decay,
                           &D->s_peak, &D->s_valley);

        // demod_out roughly -1..+1
        float demod_out = m_norm - s_norm;
        nudge_pll(chan, subchan, demod_out, D, 1.0f);
      }
      break;

      case 'B':
      case 'D': {
        // “B” style: frequency discriminator (rate of phase change).

        if (D->use_prefilter) {
          push_sample(fsam, D->raw_cb, D->pre_filter_taps);
          fsam = convolve(D->raw_cb, D->pre_filter, D->pre_filter_taps);
        }

        // Multiply by center freq oscillator (I/Q):
        push_sample(fsam * fcos256(D->u.afsk.c_osc_phase),
                    D->u.afsk.c_I_raw, D->lp_filter_taps);
        push_sample(fsam * fsin256(D->u.afsk.c_osc_phase),
                    D->u.afsk.c_Q_raw, D->lp_filter_taps);
        D->u.afsk.c_osc_phase += D->u.afsk.c_osc_delta;

        float c_I = convolve(D->u.afsk.c_I_raw, D->lp_filter, D->lp_filter_taps);
        float c_Q = convolve(D->u.afsk.c_Q_raw, D->lp_filter, D->lp_filter_taps);

        float phase = atan2f(c_Q, c_I);
        float rate = phase - D->u.afsk.prev_phase;
        if (rate > M_PI)  rate -= 2.0f * M_PI;
        else if (rate < -M_PI) rate += 2.0f * M_PI;
        D->u.afsk.prev_phase = phase;

        float norm_rate = rate * D->u.afsk.normalize_rpsam;

        // single-slicer => no offsets:
        nudge_pll(chan, subchan, norm_rate, D, 1.0f);
      }
      break;

      default:
        // Fallback if unknown profile
        break;
    }
}

// ------------------------------------------------------------------
// nudge_pll: single-slicer approach
// We sample the bit on negative overflow of data_clock_pll.
// Then we call hdlc_rec_bit(...) or your own bit function
// if you want raw bits. 
// ------------------------------------------------------------------
static void nudge_pll(int chan,
                      int subchan,
                      float demod_out,
                      struct demodulator_state_s *D,
                      float amplitude)
{
    // Track previous for sign crossing:
    signed int prev_pll = D->slicer[0].data_clock_pll;

    // Move clock forward by step each sample:
    unsigned int step_u = (unsigned int) D->pll_step_per_sample;
    D->slicer[0].data_clock_pll =
      (signed int)((unsigned int)(D->slicer[0].data_clock_pll) + step_u);

    // If we crossed from + to - => sample bit:
    if (D->slicer[0].data_clock_pll < 0 && prev_pll > 0) {
        // We have a bit; demod_out>0 => '1' else => '0'
        int quality = (int)(fabsf(demod_out) * 100.0f / amplitude);
        if (quality > 100) quality = 100;

        // Call hdlc_rec_bit or your custom function:
        hdlc_rec_bit(chan, subchan, 0,
                     (demod_out > 0.0f) ? 1 : 0,
                     0, quality);
        // Optionally track it with your own bit callback
        // e.g. my_fsk_rec_bit( demod_out>0 ? 1 : 0 );
    }

    // If demod_data toggled => nudge the PLL:
    int demod_data = (demod_out > 0.0f);
    if (demod_data != D->slicer[0].prev_demod_data) {
        // Adjust clock to lock or search:
        if (D->slicer[0].data_detect) {
            D->slicer[0].data_clock_pll = (int)(D->slicer[0].data_clock_pll *
                                                D->pll_locked_inertia);
        } else {
            D->slicer[0].data_clock_pll = (int)(D->slicer[0].data_clock_pll *
                                                D->pll_searching_inertia);
        }
    }

    D->slicer[0].prev_demod_data = demod_data;
}
