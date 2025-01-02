//
//    This file is part of Dire Wolf, an amateur radio packet TNC.
//
//    Copyright (C) 2011, 2012, 2013, 2014, 2015, 2020  John Langner, WB2OSZ
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include "direwolf.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "audio.h"
#include "fsk_demod_state.h"
#include "fsk_gen_filter.h"
// Removed "hdlc_rec.h" here, since we won’t call hdlc_rec_bit anymore.
#include "textcolor.h"
#include "demod_afsk.h"
#include "dsp.h"

// Include our custom bit-capture function:
#include "my_fsk.h"

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

#define TUNE(envvar,param,name,fmt) {                               \
    char *e = getenv(envvar);                                       \
    if (e != NULL) {                                                \
      param = atof(e);                                              \
      text_color_set (DW_COLOR_ERROR);                              \
      dw_printf ("TUNE: " name " = " fmt "\n", param);              \
    } }


// Cosine table indexed by unsigned byte.
static float fcos256_table[256];

#define fcos256(x) (fcos256_table[((x)>>24)&0xff])
#define fsin256(x) (fcos256_table[(((x)>>24)-64)&0xff])

static void nudge_pll (int chan, int subchan, int slice, float demod_out, struct demodulator_state_s *D, float amplitude);


/* Quick approximation to sqrt(x*x + y*y) */
/* No benefit for regular PC. */
/* Might help with microcomputer platform??? */

__attribute__((hot)) __attribute__((always_inline))
static inline float fast_hypot(float x, float y)
{
#if 0
    x = fabsf(x);
    y = fabsf(y);

    if (x > y) {
      return (x * .941246f + y * .41f);
    }
    else {
      return (y * .941246f + x * .41f);
    }
#else
    return (hypotf(x,y));
#endif
}


/* Add sample to buffer and shift the rest down. */

__attribute__((hot)) __attribute__((always_inline))
static inline void push_sample (float val, float *buff, int size)
{
    memmove(buff+1,buff,(size-1)*sizeof(float));
    buff[0] = val; 
}


/* FIR filter kernel. */

__attribute__((hot)) __attribute__((always_inline))
static inline float convolve (const float *__restrict__ data, const float *__restrict__ filter, int filter_taps)
{
    float sum = 0.0f;
    int j;

//#pragma GCC ivdep
    for (j=0; j<filter_taps; j++) {
        sum += filter[j] * data[j];
    }
    return (sum);
}

// Automatic Gain control - used when we have a single slicer.
//
// The first step is to create an envelope for the peak and valley
// of the mark or space amplitude.  We need to keep track of the valley
// because it does not go down to zero when the tone is not present.
// We want to find the difference between tone present and not.
//
// We use an IIR filter with fast attack and slow decay which only considers the past.
// Perhaps an improvement could be obtained by looking in the future as well.
//
// Result should settle down to 1 unit peak to peak.  i.e. -0.5 to +0.5

__attribute__((hot)) __attribute__((always_inline))
static inline float agc (float in, float fast_attack, float slow_decay, float *ppeak, float *pvalley)
{
    if (in >= *ppeak) {
      *ppeak = in * fast_attack + *ppeak * (1.0f - fast_attack);
    }
    else {
      *ppeak = in * slow_decay + *ppeak * (1.0f - slow_decay);
    }

    if (in <= *pvalley) {
      *pvalley = in * fast_attack + *pvalley * (1.0f - fast_attack);
    }
    else  {   
      *pvalley = in * slow_decay + *pvalley * (1.0f - slow_decay);
    }

    float x = in;
    if (x > *ppeak)   x = *ppeak;   // experiment: clip to envelope?
    if (x < *pvalley) x = *pvalley;

    if (*ppeak > *pvalley) {
      return ((x - 0.5f * (*ppeak + *pvalley)) / (*ppeak - *pvalley));
    }
    return (0.0f);
}

#define MIN_G 0.5f
#define MAX_G 4.0f

/* static */  float space_gain[MAX_SUBCHANS];


/*------------------------------------------------------------------
 *
 * Name:        demod_afsk_init
 *
 * Purpose:     Initialization for an AFSK demodulator.
 *              Select appropriate parameters and set up filters.
 *
 *----------------------------------------------------------------*/
void demod_afsk_init (int samples_per_sec, int baud, int mark_freq,
                        int space_freq, char profile, struct demodulator_state_s *D)
{

    int j;

    for (j = 0; j < 256; j++) {
      fcos256_table[j] = cosf((float)j * 2.0f * (float)M_PI / 256.0f);
    }

    memset (D, 0, sizeof(struct demodulator_state_s));
    D->num_slicers = 1;

#if DEBUG1
    dw_printf ("demod_afsk_init (rate=%d, baud=%d, mark=%d, space=%d, profile=%c\n",
        samples_per_sec, baud, mark_freq, space_freq, profile);
#endif
    D->profile = profile;

    switch (D->profile) {

      case 'A':    // Official name
      case 'E':    // For compatibility during transition

        D->profile = 'A';

        D->use_prefilter = 1;

        if (baud > 600) {
          D->prefilter_baud = 0.155;
          D->pre_filter_len_sym = 383 * 1200. / 44100.; 
          D->pre_window = BP_WINDOW_TRUNCATED;
        }
        else {
          D->prefilter_baud = 0.87;
          D->pre_filter_len_sym = 1.857;
          D->pre_window = BP_WINDOW_COSINE;
        }

        D->u.afsk.m_osc_phase = 0;
        D->u.afsk.m_osc_delta = round ( pow(2., 32.) * (double)mark_freq / (double)samples_per_sec );

        D->u.afsk.s_osc_phase = 0;
        D->u.afsk.s_osc_delta = round ( pow(2., 32.) * (double)space_freq / (double)samples_per_sec );

        D->u.afsk.use_rrc = 1;
        TUNE("TUNE_USE_RRC", D->u.afsk.use_rrc, "use_rrc", "%d")

        if (D->u.afsk.use_rrc) {
          D->u.afsk.rrc_width_sym = 2.80;
          D->u.afsk.rrc_rolloff = 0.20;
        }
        else {
          D->lpf_baud = 0.14;
          D->lp_filter_width_sym = 1.388;
          D->lp_window = BP_WINDOW_TRUNCATED;
        }

        D->agc_fast_attack = 0.70;
        D->agc_slow_decay  = 0.000090;

        D->pll_locked_inertia    = 0.74;
        D->pll_searching_inertia = 0.50;
        break;

      case 'B':
      case 'D':

        D->profile = 'B';

        D->use_prefilter = 1;

        if (baud > 600) {
          D->prefilter_baud = 0.19;
          D->pre_filter_len_sym = 8.163;
          D->pre_window = BP_WINDOW_TRUNCATED;
        }
        else {
          D->prefilter_baud = 0.87;
          D->pre_filter_len_sym = 1.857;
          D->pre_window = BP_WINDOW_COSINE;
        }

        D->u.afsk.c_osc_phase = 0;
        D->u.afsk.c_osc_delta = round ( pow(2., 32.) * 0.5 * (mark_freq + space_freq) / (double)samples_per_sec );

        D->u.afsk.use_rrc = 1;
        TUNE("TUNE_USE_RRC", D->u.afsk.use_rrc, "use_rrc", "%d")

        if (D->u.afsk.use_rrc) {
          D->u.afsk.rrc_width_sym = 2.00;
          D->u.afsk.rrc_rolloff   = 0.40;
        }
        else {
          D->lpf_baud = 0.5;
          D->lp_filter_width_sym = 1.714286;
          D->lp_window = BP_WINDOW_TRUNCATED;
        }

        D->u.afsk.normalize_rpsam = 1.0 / (0.5 * abs(mark_freq - space_freq) * 2 * M_PI / samples_per_sec);

        D->agc_fast_attack = 0.70;
        D->agc_slow_decay  = 0.000090;

        D->pll_locked_inertia    = 0.74;
        D->pll_searching_inertia = 0.50;

        D->alevel_mark_peak  = -1;
        D->alevel_space_peak = -1;
        break;

      default:
        text_color_set(DW_COLOR_ERROR);
        dw_printf ("Invalid AFSK demodulator profile = %c\n", profile);
        exit (1);
    }

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

    if (baud == 521) {
      D->pll_step_per_sample = (int) round((TICKS_PER_PLL_CYCLE * (double)520.83) / ((double)samples_per_sec));
    }
    else {
      D->pll_step_per_sample = (int) round((TICKS_PER_PLL_CYCLE * (double)baud) / ((double)samples_per_sec));
    }

    if (D->use_prefilter) {
      D->pre_filter_taps = ((int)( D->pre_filter_len_sym * (float)samples_per_sec / (float)baud )) | 1;
      TUNE("TUNE_PRE_FILTER_TAPS", D->pre_filter_taps, "pre_filter_taps", "%d")

      if (D->pre_filter_taps > MAX_FILTER_SIZE) {
        text_color_set (DW_COLOR_ERROR);
        dw_printf ("Warning: Calculated pre filter size of %d is too large.\n", D->pre_filter_taps);
        dw_printf ("Decrease the audio sample rate or increase the decimation factor.\n");
        dw_printf ("You can use -D2 or -D3, on the command line, to down-sample the audio rate\n");
        dw_printf ("before demodulating...\n");
        D->pre_filter_taps = (MAX_FILTER_SIZE - 1) | 1;
      }

      float f1 = MIN(mark_freq,space_freq) - D->prefilter_baud * baud;
      float f2 = MAX(mark_freq,space_freq) + D->prefilter_baud * baud;
      f1 = f1 / (float)samples_per_sec;
      f2 = f2 / (float)samples_per_sec;

      gen_bandpass (f1, f2, D->pre_filter, D->pre_filter_taps, D->pre_window);
    }

    if (D->u.afsk.use_rrc) {
      D->lp_filter_taps = ((int) (D->u.afsk.rrc_width_sym * (float)samples_per_sec / baud)) | 1;
      TUNE("TUNE_LP_FILTER_TAPS", D->lp_filter_taps, "lp_filter_taps (RRC)", "%d")

      if (D->lp_filter_taps > MAX_FILTER_SIZE) {
        text_color_set(DW_COLOR_ERROR);
        dw_printf ("Calculated RRC low pass filter size of %d is too large.\n", D->lp_filter_taps);
        dw_printf ("Decrease the audio sample rate...\n");
        D->lp_filter_taps = (MAX_FILTER_SIZE - 1) | 1;
      }

      assert (D->lp_filter_taps > 8 && D->lp_filter_taps <= MAX_FILTER_SIZE);
      (void)gen_rrc_lowpass (D->lp_filter, D->lp_filter_taps, D->u.afsk.rrc_rolloff, (float)samples_per_sec / baud);
    }
    else {
      D->lp_filter_taps = (int) round( D->lp_filter_width_sym * (float)samples_per_sec / (float)baud );
      TUNE("TUNE_LP_FILTER_TAPS", D->lp_filter_taps, "lp_filter_taps (FIR)", "%d")

      if (D->lp_filter_taps > MAX_FILTER_SIZE) {
        text_color_set (DW_COLOR_ERROR);
        dw_printf ("Calculated FIR low pass filter size of %d is too large.\n", D->lp_filter_taps);
        dw_printf ("Decrease the audio sample rate...\n");
        D->lp_filter_taps = (MAX_FILTER_SIZE - 1) | 1;
      }

      assert (D->lp_filter_taps > 8 && D->lp_filter_taps <= MAX_FILTER_SIZE);
      float fc = baud * D->lpf_baud / (float)samples_per_sec;
      gen_lowpass (fc, D->lp_filter, D->lp_filter_taps, D->lp_window);
    }

    space_gain[0] = MIN_G;
    float step = powf(10.0, log10f(MAX_G/MIN_G) / (MAX_SUBCHANS-1));
    for (j=1; j<MAX_SUBCHANS; j++) {
      space_gain[j] = space_gain[j-1] * step;
    }
}


/*-------------------------------------------------------------------
 *
 * Name:        demod_afsk_process_sample
 *
 * Purpose:     (1) Demodulate the AFSK signal.
 *              (2) Recover clock and data.
 *
 *--------------------------------------------------------------------*/
__attribute__((hot))
void demod_afsk_process_sample (int chan, int subchan, int sam, struct demodulator_state_s *D)
{
#if DEBUG4
    static FILE *demod_log_fp = NULL;
    static int seq = 0;
#endif

    assert (chan >= 0 && chan < MAX_CHANS);
    assert (subchan >= 0 && subchan < MAX_SUBCHANS);

    float fsam = (float)sam / 16384.0f;

    switch (D->profile) {

      case 'E':
      default:
      case 'A': {

        if (D->use_prefilter) {
          push_sample (fsam, D->raw_cb, D->pre_filter_taps);
          fsam = convolve (D->raw_cb, D->pre_filter, D->pre_filter_taps);
        }

        push_sample (fsam * fcos256(D->u.afsk.m_osc_phase), D->u.afsk.m_I_raw, D->lp_filter_taps);
        push_sample (fsam * fsin256(D->u.afsk.m_osc_phase), D->u.afsk.m_Q_raw, D->lp_filter_taps);
        D->u.afsk.m_osc_phase += D->u.afsk.m_osc_delta;

        push_sample (fsam * fcos256(D->u.afsk.s_osc_phase), D->u.afsk.s_I_raw, D->lp_filter_taps);
        push_sample (fsam * fsin256(D->u.afsk.s_osc_phase), D->u.afsk.s_Q_raw, D->lp_filter_taps);
        D->u.afsk.s_osc_phase += D->u.afsk.s_osc_delta;

        float m_I   = convolve (D->u.afsk.m_I_raw, D->lp_filter, D->lp_filter_taps);
        float m_Q   = convolve (D->u.afsk.m_Q_raw, D->lp_filter, D->lp_filter_taps);
        float m_amp = fast_hypot(m_I, m_Q);

        float s_I   = convolve (D->u.afsk.s_I_raw, D->lp_filter, D->lp_filter_taps);
        float s_Q   = convolve (D->u.afsk.s_Q_raw, D->lp_filter, D->lp_filter_taps);
        float s_amp = fast_hypot(s_I, s_Q);

        if (m_amp >= D->alevel_mark_peak) {
          D->alevel_mark_peak = m_amp * D->quick_attack + D->alevel_mark_peak * (1.0f - D->quick_attack);
        }
        else {
          D->alevel_mark_peak = m_amp * D->sluggish_decay + D->alevel_mark_peak * (1.0f - D->sluggish_decay);
        }

        if (s_amp >= D->alevel_space_peak) {
          D->alevel_space_peak = s_amp * D->quick_attack + D->alevel_space_peak * (1.0f - D->quick_attack);
        }
        else {
          D->alevel_space_peak = s_amp * D->sluggish_decay + D->alevel_space_peak * (1.0f - D->sluggish_decay);
        }

        if (D->num_slicers <= 1) {
          float m_norm = agc (m_amp, D->agc_fast_attack, D->agc_slow_decay, &(D->m_peak), &(D->m_valley));
          float s_norm = agc (s_amp, D->agc_fast_attack, D->agc_slow_decay, &(D->s_peak), &(D->s_valley));

          float demod_out = m_norm - s_norm;

          nudge_pll (chan, subchan, 0, demod_out, D, 1.0);
        }
        else {
          (void) agc (m_amp, D->agc_fast_attack, D->agc_slow_decay, &(D->m_peak), &(D->m_valley));
          (void) agc (s_amp, D->agc_fast_attack, D->agc_slow_decay, &(D->s_peak), &(D->s_valley));

          for (int slice=0; slice<D->num_slicers; slice++) {
            float demod_out = m_amp - s_amp * space_gain[slice];
            float amp       = 0.5f * (D->m_peak - D->m_valley + (D->s_peak - D->s_valley) * space_gain[slice]);
            if (amp < 0.0000001f) amp = 1;

            nudge_pll (chan, subchan, slice, demod_out, D, amp);
          }
        }
      }
      break;

      case 'D':
      case 'B': {
        if (D->use_prefilter) {
          push_sample (fsam, D->raw_cb, D->pre_filter_taps);
          fsam = convolve (D->raw_cb, D->pre_filter, D->pre_filter_taps);
        }

        push_sample (fsam * fcos256(D->u.afsk.c_osc_phase), D->u.afsk.c_I_raw, D->lp_filter_taps);
        push_sample (fsam * fsin256(D->u.afsk.c_osc_phase), D->u.afsk.c_Q_raw, D->lp_filter_taps);
        D->u.afsk.c_osc_phase += D->u.afsk.c_osc_delta;

        float c_I = convolve (D->u.afsk.c_I_raw, D->lp_filter, D->lp_filter_taps);
        float c_Q = convolve (D->u.afsk.c_Q_raw, D->lp_filter, D->lp_filter_taps);

        float phase = atan2f (c_Q, c_I);
        float rate  = phase - D->u.afsk.prev_phase;
        if (rate > M_PI) rate -= 2 * M_PI;
        else if (rate < -M_PI) rate += 2 * M_PI;
        D->u.afsk.prev_phase = phase;

        float norm_rate = rate * D->u.afsk.normalize_rpsam;

        if (D->num_slicers <= 1) {
          float demod_out = norm_rate;
          nudge_pll (chan, subchan, 0, demod_out, D, 1.0);
        }
        else {
          for (int slice=0; slice<D->num_slicers; slice++) {
            float offset    = -0.5 + slice * (1.0 / (D->num_slicers - 1));
            float demod_out = norm_rate + offset;
            nudge_pll (chan, subchan, slice, demod_out, D, 1.0);
          }
        }
      }
      break;
    }

#if DEBUG4
    // Some debug file logic was here, omitted for brevity in this code example
    // but you can keep it if you like capturing logs.
#endif
}


/*
 * Finally, a PLL is used to sample near the centers of the data bits.
 */
static void nudge_pll (int chan, int subchan, int slice, float demod_out, struct demodulator_state_s *D, float amplitude)
{
    D->slicer[slice].prev_d_c_pll = D->slicer[slice].data_clock_pll;

    // Perform the add as unsigned to avoid signed overflow error.
    D->slicer[slice].data_clock_pll = (signed)((unsigned)(D->slicer[slice].data_clock_pll) + (unsigned)(D->pll_step_per_sample));

    if (D->slicer[slice].data_clock_pll < 0 && D->slicer[slice].prev_d_c_pll > 0) {
        // Overflow => sample bit here
        int quality = fabsf(demod_out) * 100.0f / amplitude;
        if (quality > 100) quality = 100;

        // Replace hdlc_rec_bit() with our custom function:
        my_fsk_rec_bit(demod_out > 0 ? 1 : 0);
    }

    int demod_data = demod_out > 0;
    if (demod_data != D->slicer[slice].prev_demod_data) {
        if (D->slicer[slice].data_detect) {
          D->slicer[slice].data_clock_pll = (int)(D->slicer[slice].data_clock_pll * D->pll_locked_inertia);
        }
        else {
          D->slicer[slice].data_clock_pll = (int)(D->slicer[slice].data_clock_pll * D->pll_searching_inertia);
        }
    }

    D->slicer[slice].prev_demod_data = demod_data;
}

/* end demod_afsk.c */
