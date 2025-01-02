/******************************************************************************
 * File: dsp.c
 * 
 * Purpose:
 *   Minimal implementations of the DSP functions declared in dsp.h,
 *   sufficient for use by demod_afsk.c. This is not the full Dire Wolf DSP
 *   code but is enough to compile & run the single-slicer AFSK demod example.
 ******************************************************************************/

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "dsp.h"

/* 
 * Minimal 'window' function that handles your two known bp_window_t modes:
 *   BP_WINDOW_TRUNCATED => returns 1.0 
 *   BP_WINDOW_COSINE    => returns a simple cosine shape
 * Anything else defaults to 1.0
 */
float window(bp_window_t type, int size, int j)
{
    switch (type)
    {
        case BP_WINDOW_TRUNCATED:
            return 1.0f;

        case BP_WINDOW_COSINE:
        {
            float center = 0.5f * (size - 1);
            float x = (j - center) / (float)size * (float)M_PI;
            return cosf(x);
        }

        default:
            return 1.0f;
    }
}

/*----------------------------------------------------------------------------
 * gen_lowpass - Generate a simple lowpass filter via sinc * window.
 *   fc: cutoff fraction of sampling freq (0..0.5)
 *   lp_filter: output array of size filter_size
 *   filter_size: number of taps
 *   wtype: BP_WINDOW_TRUNCATED or BP_WINDOW_COSINE
 *--------------------------------------------------------------------------*/
void gen_lowpass(float fc, float *lp_filter, int filter_size, bp_window_t wtype)
{
    float sum = 0.0f;
    float center = 0.5f * (filter_size - 1);
    int i;

    for (i = 0; i < filter_size; i++)
    {
        float x = i - center;
        float sinc;
        if (fabsf(x) < 1.0e-7f)
        {
            sinc = 2.0f * fc;
        }
        else
        {
            /* normal sinc = sin(2πfc * x) / (π x) */
            sinc = sinf(2.0f * (float)M_PI * fc * x) / ( (float)M_PI * x );
        }

        float w = window(wtype, filter_size, i);
        lp_filter[i] = sinc * w;
        sum += lp_filter[i];
    }

    /* Normalize so DC gain ~ 1.0 */
    if (fabsf(sum) < 1.0e-12f) sum = 1.0f;
    for (i = 0; i < filter_size; i++)
    {
        lp_filter[i] /= sum;
    }
}

/*----------------------------------------------------------------------------
 * gen_bandpass - Generate a simple bandpass filter kernel for a prefilter.
 *   f1, f2: lower & upper cutoff frequencies (fraction of sample rate)
 *   bp_filter: output array of size filter_size
 *   filter_size: number of taps
 *   wtype: e.g. BP_WINDOW_TRUNCATED
 *--------------------------------------------------------------------------*/
void gen_bandpass(float f1, float f2, float *bp_filter, int filter_size, bp_window_t wtype)
{
    float sum = 0.0f;
    float center = 0.5f * (filter_size - 1);
    int i;

    for (i = 0; i < filter_size; i++)
    {
        float x = i - center;
        float y;
        if (fabsf(x) < 1.0e-7f)
        {
            y = 2.0f * (f2 - f1);
        }
        else
        {
            /* difference of two sincs */
            float num1 = sinf(2.0f * (float)M_PI * f2 * x);
            float num2 = sinf(2.0f * (float)M_PI * f1 * x);
            y = (num1 - num2) / ((float)M_PI * x);
        }

        float w = window(wtype, filter_size, i);
        bp_filter[i] = y * w;
        sum += bp_filter[i];
    }

    /* Normalize approx by measuring gain at middle freq = (f1+f2)/2 */
    float mid = 0.5f * (f1 + f2);
    float G = 0.0f;
    for (i = 0; i < filter_size; i++)
    {
        float x = i - center;
        /* approximate amplitude by summation of cos terms at 'mid' */
        G += 2.0f * bp_filter[i] * cosf(2.0f * (float)M_PI * mid * x);
    }

    if (fabsf(G) < 1.0e-12f) G = 1.0f;
    for (i = 0; i < filter_size; i++)
    {
        bp_filter[i] /= G;
    }
}

/*----------------------------------------------------------------------------
 * rrc - root-raised-cosine function used by gen_rrc_lowpass
 *   t: time in "symbol" units
 *   a: roll-off factor, typically 0.2, 0.4, etc.
 *--------------------------------------------------------------------------*/
float rrc(float t, float a)
{
    /* Minimal approach: if near 0, return 1.0; else the normal RRC formula. */
    float eps = 1.0e-8f;

    if (fabsf(t) < eps)
    {
        return 1.0f; /* or 2*fc, depends on normalizing approach */
    }

    /* Official style formula:
       If (1 - 4a^2 t^2) ~ 0, handle carefully, else:
         rrc(t) = (sin(pi t)/(pi t)) * (cos(pi a t)/(1 - (2 a t)^2))
    */
    float numerator = sinf((float)M_PI * t) / ((float)M_PI * t);
    float cterm     = cosf((float)M_PI * a * t);
    float denom     = 1.0f - 4.0f * a * a * t * t;
    
    if (fabsf(denom) < eps)
    {
        /* near the zero denominator region => approximate a “corner” */
        return cterm * 1.0f; /* simplistic fallback */
    }
    return numerator * (cterm / denom);
}

/*----------------------------------------------------------------------------
 * gen_rrc_lowpass - root-raised-cosine filter
 *   pfilter: output array of length 'taps'
 *   rolloff: e.g. 0.2
 *   sps: samples per symbol
 *--------------------------------------------------------------------------*/
void gen_rrc_lowpass(float *pfilter, int taps, float rolloff, float sps)
{
    float sum = 0.0f;
    float half = 0.5f * (taps - 1.0f);
    int i;

    for (i = 0; i < taps; i++)
    {
        /* time offset in symbol units: center the filter at 0 */
        float t = (i - half) / sps;
        float val = rrc(t, rolloff);
        pfilter[i] = val;
        sum += val;
    }

    /* Normalize so sum-of-taps = 1.0 => unity DC gain */
    if (fabsf(sum) < 1.0e-12f) sum = 1.0f;
    for (i = 0; i < taps; i++)
    {
        pfilter[i] /= sum;
    }
}

