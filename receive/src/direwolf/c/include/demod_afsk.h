//
// demod_afsk.c (same as your posted version, minus references to 'modem_type', 'v26_alt', etc.)
//
#include "direwolf.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "audio.h"
#include "fsk_demod_state.h"
#include "fsk_gen_filter.h"
#include "hdlc_rec.h"
#include "textcolor.h"
#include "demod_afsk.h"
#include "dsp.h"

// The rest is unchanged from your original demod_afsk.c 
// EXCEPT we skip referencing the removed enums or #includes for other modems.
//
// (See the final code block you posted. It's good as-is. 
// The key is that 'demod_afsk_init' no longer needs 'modem_type' 
// and the struct no longer has that field.)
