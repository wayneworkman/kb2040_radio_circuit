#ifndef AUDIO_H
#define AUDIO_H

#include "direwolf.h"

/*
 * Minimal "audio_s" struct for single AFSK usage. 
 * We remove references to 'modem_type' or multi-subchan logic.
 */
struct audio_s {
    // If you only have 1 channel, you could do "int chan_medium;" 
    // but let's keep the array for 2 channels in case you want that:
    int chan_medium[MAX_CHANS];
    
    // We don't need 'modem_type' or more subchannel fields if you only do 1.
    // But you can keep "int num_subchan;" if you want the code to support multiple subchannels.
    struct {
        int num_subchan;    // e.g. 1 means we only do single subchannel
    } achan[MAX_CHANS];
};

#endif
