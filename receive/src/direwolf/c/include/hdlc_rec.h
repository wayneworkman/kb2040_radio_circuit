#ifndef HDLC_REC_H
#define HDLC_REC_H

#include "direwolf.h"

void hdlc_rec_bit(int chan, int subchan, int slice, int raw, 
                  int is_scrambled, int not_used_remove);

#endif