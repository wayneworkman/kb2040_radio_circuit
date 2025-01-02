// File: receive/src/direwolf/c/demod_afsk.c

//
//  Minimal single-subchannel AFSK demodulator.
//  Removes references to multi-slicer code and hdlc_rec.
//
//  If you want to route raw bits to your ring buffer, call my_fsk_rec_bit(...).
//

#include "demod_afsk.h"
#include "audio.h"
#include "fsk_demod_state.h"
#include "fsk_gen_filter.h"
#include "my_fsk.h"       // Optional: if you want to store raw bits
#include "textcolor.h"
#include "direwolf.h"
#include "dsp.h"

// Macros for single-slicer:
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

#define TUNE(envvar,param,name,fmt) {                               \
    char *e = getenv(envvar);                                       \
    if (e != NULL) {                                                \
      param = atof(e);                                              \
      text_color_set(DW_COLOR_ERROR);                               \
      dw_printf("TUNE: " name " = " fmt "\\n", param);              \
    } }

// We'll store a small cosine table for oscillator mixing:
static float fcos256_table[256];

static inline float fast_hypot(float x, float y)
{
    return hypotf(x, y);
}

static inline void push_sample(float val, float *buff, int size)
{
    memmove(buff + 1, buff, (size - 1) * sizeof(float));
    buff[0] = val;
}

static inline float convolve(const float *data, const float *filter, int taps)
{
    float sum = 0.0f;
    for (int i=0; i<taps; i++) {
        sum += data[i] * filter[i];
    }
    return sum;
}

static inline float agc(float in, float fast_attack, float slow_decay,
                        float *ppeak, float *pvalley)
{
    // Update peak:
    if (in >= *ppeak) {
        *ppeak = in * fast_attack + *ppeak * (1.0f - fast_attack);
    } else {
        *ppeak = in * slow_decay + *ppeak * (1.0f - slow_decay);
    }

    // Update valley:
    if (in <= *pvalley) {
        *pvalley = in * fast_attack + *pvalley * (1.0f - fast_attack);
    } else {
        *pvalley = in * slow_decay + *pvalley * (1.0f - slow_decay);
    }

    float x = in;
    if (x > *ppeak)   x = *ppeak;
    if (x < *pvalley) x = *pvalley;

    if (*ppeak > *pvalley) {
        return (x - 0.5f*(*ppeak + *pvalley)) / (*ppeak - *pvalley);
    }
    return 0.0f;
}

// Forward-declare the local PLL function:
static void nudge_pll(int chan, int subchan, float demod_out,
                      struct demodulator_state_s *D, float amplitude);

// ------------------------------------------------------------------
// demod_afsk_init
// ------------------------------------------------------------------
void demod_afsk_init(int samples_per_sec,
                     int baud,
                     int mark_freq,
                     int space_freq,
                     char profile,
                     struct demodulator_state_s *D)
{
    // Build table for oscillator usage:
    for (int i=0; i<256; i++) {
        fcos256_table[i] = cosf((float)i * 2.0f * (float)M_PI / 256.0f);
    }

    memset(D, 0, sizeof(*D));
    D->num_slicers = 1; // Single slicer
    D->profile = profile;

    // Possibly read environment overrides:
    TUNE("TUNE_USE_RRC", D->u.afsk.use_rrc, "use_rrc", "%d")

    // Basic logic for 'A' vs 'B':
    switch (profile) {
      case 'A':
      case 'E':
        D->use_prefilter = 1;
        D->prefilter_baud = (baud > 600) ? 0.155f : 0.87f;
        D->pre_filter_len_sym = (baud > 600) ? 383*1200.0f/44100.0f : 1.857f;
        D->pre_window = BP_WINDOW_TRUNCATED;

        D->u.afsk.m_osc_phase  = 0;
        D->u.afsk.m_osc_delta  = (unsigned int)round(pow(2.,32.) * (double)mark_freq / (double)samples_per_sec);
        D->u.afsk.s_osc_phase  = 0;
        D->u.afsk.s_osc_delta  = (unsigned int)round(pow(2.,32.) * (double)space_freq / (double)samples_per_sec);

        D->u.afsk.use_rrc      = 1;
        D->u.afsk.rrc_width_sym= 2.80f;
        D->u.afsk.rrc_rolloff  = 0.20f;

        D->lpf_baud            = 0.14f;
        D->lp_filter_width_sym = 1.388f;

        D->agc_fast_attack     = 0.70f;
        D->agc_slow_decay      = 0.000090f;
        D->pll_locked_inertia    = 0.74f;
        D->pll_searching_inertia = 0.50f;
        break;

      case 'B':
      case 'D':
        D->use_prefilter       = 1;
        D->prefilter_baud      = (baud > 600) ? 0.19f : 0.87f;
        D->pre_filter_len_sym  = (baud > 600) ? 8.163f : 1.857f;
        D->pre_window          = BP_WINDOW_TRUNCATED;

        D->u.afsk.c_osc_phase  = 0;
        D->u.afsk.c_osc_delta  = (unsigned int)round(pow(2.,32.) * 0.5*(mark_freq+space_freq)/(double)samples_per_sec);

        D->u.afsk.use_rrc      = 1;
        D->u.afsk.rrc_width_sym= 2.00f;
        D->u.afsk.rrc_rolloff  = 0.40f;

        D->lpf_baud            = 0.5f;
        D->lp_filter_width_sym = 1.714286f;
        D->u.afsk.normalize_rpsam = 1.0f / (0.5f*fabsf((float)mark_freq-(float)space_freq)*
                                    2.0f*(float)M_PI/(float)samples_per_sec);

        D->agc_fast_attack       = 0.70f;
        D->agc_slow_decay        = 0.000090f;
        D->pll_locked_inertia    = 0.74f;
        D->pll_searching_inertia = 0.50f;

        D->alevel_mark_peak     = -1.0f;
        D->alevel_space_peak    = -1.0f;
        break;

      default:
        text_color_set(DW_COLOR_ERROR);
        dw_printf("Invalid AFSK profile=%c\\n", profile);
        exit(1);
    }

    // Additional TUNE environment:
    TUNE("TUNE_PRE_BAUD",     D->prefilter_baud, "prefilter_baud", "%.3f")
    TUNE("TUNE_PRE_WINDOW",   D->pre_window,     "pre_window",     "%d")
    TUNE("TUNE_LPF_BAUD",     D->lpf_baud,       "lpf_baud",       "%.3f")
    TUNE("TUNE_RRC_ROLLOFF",  D->u.afsk.rrc_rolloff,  "rrc_rolloff",  "%.2f")
    TUNE("TUNE_RRC_WIDTH_SYM",D->u.afsk.rrc_width_sym,"rrc_width_sym","%.2f")
    TUNE("TUNE_AGC_FAST",     D->agc_fast_attack,"agc_fast_attack", "%.3f")
    TUNE("TUNE_AGC_SLOW",     D->agc_slow_decay, "agc_slow_decay",  "%.6f")
    TUNE("TUNE_PLL_LOCKED",   D->pll_locked_inertia, "pll_locked_inertia", "%.2f")
    TUNE("TUNE_PLL_SEARCHING",D->pll_searching_inertia,"pll_searching_inertia","%.2f")

    // PLL step:
    if (baud == 521) {
        D->pll_step_per_sample = (int)round((TICKS_PER_PLL_CYCLE * 520.83) / (double)samples_per_sec);
    } else {
        D->pll_step_per_sample = (int)round((TICKS_PER_PLL_CYCLE * (double)baud)/(double)samples_per_sec);
    }

    // Build pre-filter:
    if (D->use_prefilter) {
        D->pre_filter_taps = (int)(D->pre_filter_len_sym*(float)samples_per_sec/(float)baud);
        D->pre_filter_taps |= 1; // ensure odd
        if (D->pre_filter_taps < 1)  D->pre_filter_taps=1;
        if (D->pre_filter_taps>MAX_FILTER_SIZE) {
            D->pre_filter_taps = (MAX_FILTER_SIZE -1)|1;
        }
        float f1 = MIN(mark_freq, space_freq) - D->prefilter_baud*baud;
        float f2 = MAX(mark_freq, space_freq) + D->prefilter_baud*baud;
        f1 /= (float)samples_per_sec;
        f2 /= (float)samples_per_sec;
        gen_bandpass(f1, f2, D->pre_filter, D->pre_filter_taps, D->pre_window);
    }

    // Build low-pass or RRC:
    if (D->u.afsk.use_rrc) {
        D->lp_filter_taps = (int)(D->u.afsk.rrc_width_sym*(float)samples_per_sec/(float)baud);
        D->lp_filter_taps |= 1;
        if (D->lp_filter_taps<9) D->lp_filter_taps=9;
        if (D->lp_filter_taps>MAX_FILTER_SIZE) {
            D->lp_filter_taps=(MAX_FILTER_SIZE-1)|1;
        }
        gen_rrc_lowpass(D->lp_filter,
                        D->lp_filter_taps,
                        D->u.afsk.rrc_rolloff,
                        (float)samples_per_sec/(float)baud);
    } else {
        D->lp_filter_taps = (int)round(D->lp_filter_width_sym*(float)samples_per_sec/(float)baud);
        if (D->lp_filter_taps<9) D->lp_filter_taps=9;
        if (D->lp_filter_taps>MAX_FILTER_SIZE) {
            D->lp_filter_taps=(MAX_FILTER_SIZE-1)|1;
        }
        float fc = baud*D->lpf_baud/(float)samples_per_sec;
        gen_lowpass(fc, D->lp_filter, D->lp_filter_taps, D->lp_window);
    }
}

// ------------------------------------------------------------------
// demod_afsk_process_sample
// ------------------------------------------------------------------
void demod_afsk_process_sample(int chan,
                               int subchan,
                               int sam,
                               struct demodulator_state_s *D)
{
    // Scale sample:
    float fsam = (float)sam / 16384.0f;

    switch (D->profile) {

      case 'A':
      case 'E': {
        // (Mark/Space oscillator approach)
        if (D->use_prefilter) {
            push_sample(fsam, D->raw_cb, D->pre_filter_taps);
            fsam = convolve(D->raw_cb, D->pre_filter, D->pre_filter_taps);
        }

        // Mark I/Q:
        float cos_m = fcos256_table[(D->u.afsk.m_osc_phase>>24)&0xff];
        float sin_m = fcos256_table[(((D->u.afsk.m_osc_phase>>24)-64)&0xff)];
        push_sample(fsam*cos_m, D->u.afsk.m_I_raw, D->lp_filter_taps);
        push_sample(fsam*sin_m, D->u.afsk.m_Q_raw, D->lp_filter_taps);
        D->u.afsk.m_osc_phase += D->u.afsk.m_osc_delta;

        // Space I/Q:
        float cos_s = fcos256_table[(D->u.afsk.s_osc_phase>>24)&0xff];
        float sin_s = fcos256_table[(((D->u.afsk.s_osc_phase>>24)-64)&0xff)];
        push_sample(fsam*cos_s, D->u.afsk.s_I_raw, D->lp_filter_taps);
        push_sample(fsam*sin_s, D->u.afsk.s_Q_raw, D->lp_filter_taps);
        D->u.afsk.s_osc_phase += D->u.afsk.s_osc_delta;

        float m_I=convolve(D->u.afsk.m_I_raw, D->lp_filter, D->lp_filter_taps);
        float m_Q=convolve(D->u.afsk.m_Q_raw, D->lp_filter, D->lp_filter_taps);
        float m_amp = fast_hypot(m_I, m_Q);

        float s_I=convolve(D->u.afsk.s_I_raw, D->lp_filter, D->lp_filter_taps);
        float s_Q=convolve(D->u.afsk.s_Q_raw, D->lp_filter, D->lp_filter_taps);
        float s_amp = fast_hypot(s_I, s_Q);

        // Single-slicer AGC:
        float m_norm = agc(m_amp, D->agc_fast_attack, D->agc_slow_decay,
                           &D->m_peak, &D->m_valley);
        float s_norm = agc(s_amp, D->agc_fast_attack, D->agc_slow_decay,
                           &D->s_peak, &D->s_valley);

        float demod_out = m_norm - s_norm;
        nudge_pll(chan, subchan, demod_out, D, 1.0f);
      }
      break;

      case 'B':
      case 'D': {
        // (Discriminator approach)
        if (D->use_prefilter) {
            push_sample(fsam, D->raw_cb, D->pre_filter_taps);
            fsam=convolve(D->raw_cb, D->pre_filter, D->pre_filter_taps);
        }

        float cos_c = fcos256_table[(D->u.afsk.c_osc_phase>>24)&0xff];
        float sin_c = fcos256_table[(((D->u.afsk.c_osc_phase>>24)-64)&0xff)];
        push_sample(fsam*cos_c, D->u.afsk.c_I_raw, D->lp_filter_taps);
        push_sample(fsam*sin_c, D->u.afsk.c_Q_raw, D->lp_filter_taps);
        D->u.afsk.c_osc_phase += D->u.afsk.c_osc_delta;

        float c_I=convolve(D->u.afsk.c_I_raw, D->lp_filter, D->lp_filter_taps);
        float c_Q=convolve(D->u.afsk.c_Q_raw, D->lp_filter, D->lp_filter_taps);
        float phase = atan2f(c_Q, c_I);
        float rate  = phase - D->u.afsk.prev_phase;
        if (rate > M_PI)  rate -= 2.0f*M_PI;
        else if (rate< -M_PI) rate += 2.0f*M_PI;
        D->u.afsk.prev_phase = phase;

        float norm_rate = rate*D->u.afsk.normalize_rpsam;
        nudge_pll(chan, subchan, norm_rate, D, 1.0f);
      }
      break;

      default:
        // do nothing
        break;
    }
}

// ------------------------------------------------------------------
// nudge_pll: single-slicer approach
// If it crosses from positive to negative, we sample a bit
// and call my_fsk_rec_bit(...) for raw bits.
// ------------------------------------------------------------------
static void nudge_pll(int chan,
                      int subchan,
                      float demod_out,
                      struct demodulator_state_s *D,
                      float amplitude)
{
    signed int prev_pll = D->slicer[0].data_clock_pll;
    // increment clock
    unsigned int step_u = (unsigned int)D->pll_step_per_sample;
    D->slicer[0].data_clock_pll = (signed int)((unsigned int)prev_pll + step_u);

    // check crossing from + to -
    if (D->slicer[0].data_clock_pll<0 && prev_pll>0) {
        int bit_val = (demod_out>0.0f) ? 1 : 0;
        // Optionally measure quality:
        int quality = (int)(fabsf(demod_out)*100.0f / amplitude);
        if (quality>100) quality=100;

        // Store raw bits if you want:
        my_fsk_rec_bit(bit_val);

        // or if you want fancy HDLC frames, call hdlc_rec_bit(...).
        // hdlc_rec_bit(chan, subchan, 0, bit_val, 0, quality);
    }

    int demod_data = (demod_out>0.0f);
    if (demod_data != D->slicer[0].prev_demod_data) {
        if (D->slicer[0].data_detect) {
            D->slicer[0].data_clock_pll = (int)(D->slicer[0].data_clock_pll
                                        * D->pll_locked_inertia);
        } else {
            D->slicer[0].data_clock_pll = (int)(D->slicer[0].data_clock_pll
                                        * D->pll_searching_inertia);
        }
    }
    D->slicer[0].prev_demod_data = demod_data;
}
