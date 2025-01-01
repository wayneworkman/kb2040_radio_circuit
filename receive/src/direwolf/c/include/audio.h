#ifndef AUDIO_H
#define AUDIO_H
#include "direwolf.h"

struct audio_s {
    int chan_medium[MAX_CHANS];
    struct {
        int modem_type;
        int num_subchan;
    } achan[MAX_CHANS];
};

#endif
