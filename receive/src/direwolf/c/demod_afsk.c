// File: receive/src/direwolf/c/demod_afsk.c
//
// Minimal single-slicer AFSK code that calls my_fsk_rec_bit(...).

#include "demod_afsk.h"
#include "audio.h"
#include "fsk_demod_state.h"
#include "fsk_gen_filter.h"
#include "my_fsk.h"     // ring buffer for raw bits
#include "textcolor.h"
#include "direwolf.h"
#include "dsp.h"

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define TUNE(envvar,param,name,fmt) {                 \
    char *e = getenv(envvar);                         \
    if(e){ param=atof(e);                             \
       text_color_set(DW_COLOR_ERROR);                \
       dw_printf("TUNE: " name " = " fmt "\n",param); \
    } }

// We'll keep a small cos table for mixing:
static float fcos256_table[256];

static inline float fast_hypot(float x, float y){ return hypotf(x,y); }
static inline void push_sample(float v, float *buf, int size){
    memmove(buf+1, buf, (size-1)*sizeof(float));
    buf[0]=v;
}
static inline float convolve(const float *data,const float *filt,int taps){
    float s=0; for(int i=0;i<taps;i++){ s+=data[i]*filt[i]; } return s;
}

// AGC:
static inline float agc(float in, float fa, float sd, float *ppeak,float *pval)
{
    if(in>=*ppeak) *ppeak=in*fa+*ppeak*(1.f-fa);
    else           *ppeak=in*sd+*ppeak*(1.f-sd);

    if(in<=*pval)  *pval=in*fa+*pval*(1.f-fa);
    else           *pval=in*sd+*pval*(1.f-sd);

    float x=in;
    if(x>*ppeak) x=*ppeak;
    if(x<*pval)  x=*pval;
    if(*ppeak>*pval){
        return (x-0.5f*(*ppeak+*pval))/(*ppeak-*pval);
    }
    return 0.0f;
}

// forward decl
static void nudge_pll(int chan,int subchan,float demod_out,
                      struct demodulator_state_s*D,float amplitude);

void demod_afsk_init(int sps,int baud,int mf,int sf,char prof,struct demodulator_state_s*D)
{
    for(int i=0;i<256;i++){
        fcos256_table[i]=cosf( (float)i * 2.f*(float)M_PI/256.f );
    }
    memset(D,0,sizeof(*D));
    D->num_slicers=1;
    D->profile=prof;

    TUNE("TUNE_USE_RRC",D->u.afsk.use_rrc,"use_rrc","%d")

    switch(prof){
      case 'A':
      case 'E':
        D->use_prefilter=1;
        D->prefilter_baud=(baud>600)?0.155f:0.87f;
        D->pre_filter_len_sym=(baud>600)?(383*1200.f/44100.f):1.857f;
        D->pre_window=BP_WINDOW_TRUNCATED;

        D->u.afsk.m_osc_phase=0;
        D->u.afsk.m_osc_delta=(unsigned int)round(pow(2.,32.)*(double)mf/(double)sps);

        D->u.afsk.s_osc_phase=0;
        D->u.afsk.s_osc_delta=(unsigned int)round(pow(2.,32.)*(double)sf/(double)sps);

        D->u.afsk.use_rrc=1;
        D->u.afsk.rrc_width_sym=2.80f;
        D->u.afsk.rrc_rolloff=0.20f;

        D->lpf_baud=0.14f;
        D->lp_filter_width_sym=1.388f;

        D->agc_fast_attack=0.70f;
        D->agc_slow_decay=0.000090f;
        D->pll_locked_inertia=0.74f;
        D->pll_searching_inertia=0.50f;
      break;

      case 'B':
      case 'D':
        D->use_prefilter=1;
        D->prefilter_baud=(baud>600)?0.19f:0.87f;
        D->pre_filter_len_sym=(baud>600)?8.163f:1.857f;
        D->pre_window=BP_WINDOW_TRUNCATED;

        D->u.afsk.c_osc_phase=0;
        D->u.afsk.c_osc_delta=(unsigned int)round(pow(2.,32.)*0.5*(mf+sf)/(double)sps);

        D->u.afsk.use_rrc=1;
        D->u.afsk.rrc_width_sym=2.00f;
        D->u.afsk.rrc_rolloff=0.40f;

        D->lpf_baud=0.50f;
        D->lp_filter_width_sym=1.714286f;

        D->u.afsk.normalize_rpsam=1.0f/(0.5f*fabsf((float)mf-(float)sf)*2.f*(float)M_PI/(float)sps);

        D->agc_fast_attack=0.70f;
        D->agc_slow_decay=0.000090f;
        D->pll_locked_inertia=0.74f;
        D->pll_searching_inertia=0.50f;

        D->alevel_mark_peak=-1.f;
        D->alevel_space_peak=-1.f;
      break;

      default:
        text_color_set(DW_COLOR_ERROR);
        dw_printf("Invalid profile=%c\n",prof);
        exit(1);
    }

    TUNE("TUNE_PRE_BAUD",D->prefilter_baud,"prefilter_baud","%.3f")

    if(baud==521){
        D->pll_step_per_sample=(int)round((TICKS_PER_PLL_CYCLE*520.83)/(double)sps);
    } else {
        D->pll_step_per_sample=(int)round((TICKS_PER_PLL_CYCLE*(double)baud)/(double)sps);
    }

    if(D->use_prefilter){
        D->pre_filter_taps=(int)(D->pre_filter_len_sym*(float)sps/(float)baud);
        D->pre_filter_taps|=1;
        if(D->pre_filter_taps<1) D->pre_filter_taps=1;
        if(D->pre_filter_taps>MAX_FILTER_SIZE){
            D->pre_filter_taps=(MAX_FILTER_SIZE-1)|1;
        }
        float f1=MIN(mf,sf)-D->prefilter_baud*baud;
        float f2=MAX(mf,sf)+D->prefilter_baud*baud;
        f1/=(float)sps;f2/=(float)sps;
        gen_bandpass(f1,f2,D->pre_filter,D->pre_filter_taps,D->pre_window);
    }

    if(D->u.afsk.use_rrc){
        D->lp_filter_taps=(int)(D->u.afsk.rrc_width_sym*(float)sps/(float)baud);
        D->lp_filter_taps|=1;
        if(D->lp_filter_taps<9) D->lp_filter_taps=9;
        if(D->lp_filter_taps>MAX_FILTER_SIZE){
            D->lp_filter_taps=(MAX_FILTER_SIZE-1)|1;
        }
        gen_rrc_lowpass(D->lp_filter,
                        D->lp_filter_taps,
                        D->u.afsk.rrc_rolloff,
                        (float)sps/(float)baud);
    } else {
        D->lp_filter_taps=(int)round(D->lp_filter_width_sym*(float)sps/(float)baud);
        if(D->lp_filter_taps<9) D->lp_filter_taps=9;
        if(D->lp_filter_taps>MAX_FILTER_SIZE){
            D->lp_filter_taps=(MAX_FILTER_SIZE-1)|1;
        }
        float fc=baud*D->lpf_baud/(float)sps;
        gen_lowpass(fc,D->lp_filter,D->lp_filter_taps,D->lp_window);
    }
}

void demod_afsk_process_sample(int chan,int subchan,int sam,struct demodulator_state_s*D)
{
    float fsam=(float)sam/16384.f;

    switch(D->profile){
      case 'A':
      case 'E': {
        if(D->use_prefilter){
            push_sample(fsam,D->raw_cb,D->pre_filter_taps);
            fsam=convolve(D->raw_cb,D->pre_filter,D->pre_filter_taps);
        }
        // Mark
        unsigned int mp=(D->u.afsk.m_osc_phase>>24)&0xff;
        float cos_m=fcos256_table[mp];
        float sin_m=fcos256_table[((mp-64)&0xff)];

        push_sample(fsam*cos_m,D->u.afsk.m_I_raw,D->lp_filter_taps);
        push_sample(fsam*sin_m,D->u.afsk.m_Q_raw,D->lp_filter_taps);
        D->u.afsk.m_osc_phase += D->u.afsk.m_osc_delta;

        // Space
        unsigned int sp=(D->u.afsk.s_osc_phase>>24)&0xff;
        float cos_s=fcos256_table[sp];
        float sin_s=fcos256_table[((sp-64)&0xff)];

        push_sample(fsam*cos_s,D->u.afsk.s_I_raw,D->lp_filter_taps);
        push_sample(fsam*sin_s,D->u.afsk.s_Q_raw,D->lp_filter_taps);
        D->u.afsk.s_osc_phase += D->u.afsk.s_osc_delta;

        float m_I=convolve(D->u.afsk.m_I_raw,D->lp_filter,D->lp_filter_taps);
        float m_Q=convolve(D->u.afsk.m_Q_raw,D->lp_filter,D->lp_filter_taps);
        float m_amp=fast_hypot(m_I,m_Q);

        float s_I=convolve(D->u.afsk.s_I_raw,D->lp_filter,D->lp_filter_taps);
        float s_Q=convolve(D->u.afsk.s_Q_raw,D->lp_filter,D->lp_filter_taps);
        float s_amp=fast_hypot(s_I,s_Q);

        float m_norm=agc(m_amp,D->agc_fast_attack,D->agc_slow_decay,&D->m_peak,&D->m_valley);
        float s_norm=agc(s_amp,D->agc_fast_attack,D->agc_slow_decay,&D->s_peak,&D->s_valley);
        float demod_out=m_norm - s_norm;

        nudge_pll(chan,subchan,demod_out,D,1.0f);
      }
      break;

      case 'B':
      case 'D': {
        if(D->use_prefilter){
            push_sample(fsam,D->raw_cb,D->pre_filter_taps);
            fsam=convolve(D->raw_cb,D->pre_filter,D->pre_filter_taps);
        }
        unsigned int cp=(D->u.afsk.c_osc_phase>>24)&0xff;
        float cos_c=fcos256_table[cp];
        float sin_c=fcos256_table[((cp-64)&0xff)];

        push_sample(fsam*cos_c,D->u.afsk.c_I_raw,D->lp_filter_taps);
        push_sample(fsam*sin_c,D->u.afsk.c_Q_raw,D->lp_filter_taps);
        D->u.afsk.c_osc_phase += D->u.afsk.c_osc_delta;

        float c_I=convolve(D->u.afsk.c_I_raw,D->lp_filter,D->lp_filter_taps);
        float c_Q=convolve(D->u.afsk.c_Q_raw,D->lp_filter,D->lp_filter_taps);
        float phase=atan2f(c_Q,c_I);
        float rate=phase-D->u.afsk.prev_phase;
        if(rate>M_PI) rate-=2.f*M_PI;
        else if(rate<-M_PI) rate+=2.f*M_PI;
        D->u.afsk.prev_phase=phase;

        float norm_rate=rate*D->u.afsk.normalize_rpsam;
        nudge_pll(chan,subchan,norm_rate,D,1.f);
      }
      break;
    }
}

static void nudge_pll(int chan,int subchan,float demod_out,struct demodulator_state_s*D,float amplitude)
{
    signed int prev_pll=D->slicer[0].data_clock_pll;
    unsigned int step_u=(unsigned int)D->pll_step_per_sample;
    D->slicer[0].data_clock_pll=(signed int)((unsigned int)prev_pll+step_u);

    // crossing from + to -
    if(D->slicer[0].data_clock_pll<0 && prev_pll>0){
        int bit_val=(demod_out>0.f)?1:0;
        int quality=(int)(fabsf(demod_out)*100.f/amplitude);
        if(quality>100) quality=100;

        // raw bits:
        my_fsk_rec_bit(bit_val);
    }

    int demod_data=(demod_out>0.f)?1:0;
    if(demod_data!=D->slicer[0].prev_demod_data){
        if(D->slicer[0].data_detect){
            D->slicer[0].data_clock_pll=(int)(D->slicer[0].data_clock_pll*D->pll_locked_inertia);
        } else {
            D->slicer[0].data_clock_pll=(int)(D->slicer[0].data_clock_pll*D->pll_searching_inertia);
        }
    }
    D->slicer[0].prev_demod_data=demod_data;
}
