#ifndef DIREWOLF_H
#define DIREWOLF_H

/*
 * If you need standard headers here, keep them; otherwise remove. 
 * The key is that we DO NOT redefine macros or structs that 
 * appear in fsk_demod_state.h.
 */
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

/*
 * If your code expects these to be available, just keep them if 
 * they match the rest of your code.
 */
#ifndef MAX_CHANS
  #define MAX_CHANS 2
#endif

#ifndef MAX_SUBCHANS
  #define MAX_SUBCHANS 1
#endif

#ifndef MAX_SLICERS
  #define MAX_SLICERS 1
#endif

/*
 * DO NOT define "BP_WINDOW_TRUNCATED" or other macros here; 
 * let fsk_demod_state.h define the enum or macros as needed.
 *
 * For example, if fsk_demod_state.h has:
 *   typedef enum bp_window_e {
 *       BP_WINDOW_TRUNCATED,
 *       BP_WINDOW_COSINE,
 *       ...
 *   } bp_window_t;
 * Then we must NOT do:
 *   #define BP_WINDOW_TRUNCATED 0
 * in this file.
 */

/*
 * DO NOT define struct demodulator_state_s here if 
 * fsk_demod_state.h is the canonical definition!
 * Instead, forward-declare it or include the correct header:
 */
// #include "fsk_demod_state.h"  // or wherever your real definition lives

/*
 * If you have other Dire Wolf–specific function declarations, 
 * put them here. For example:
 */
//#ifdef __cplusplus
//extern "C" {
//#endif

//void direwolf_some_function(int param);
//void direwolf_another_function(void);

//#ifdef __cplusplus
//}
//#endif

#endif /* DIREWOLF_H */
