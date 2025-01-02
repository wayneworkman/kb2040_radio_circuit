// File: receive/src/direwolf/c/my_fsk.c
//
// Minimal ring buffer for raw bits. 
// If full, drop new bits.

#include <string.h>
#include "my_fsk.h"

#define MY_FSK_RING_SIZE 8192

static int s_ring[MY_FSK_RING_SIZE];
static int s_head=0;
static int s_tail=0;

void my_fsk_rec_bit(int bit)
{
    int next=(s_head+1)%MY_FSK_RING_SIZE;
    if(next==s_tail){
        // ring full, drop
        return;
    }
    s_ring[s_head]=bit;
    s_head=next;
}

int my_fsk_get_bits(int *out,int max_bits)
{
    int count=0;
    while(count<max_bits && s_tail!=s_head){
        out[count]=s_ring[s_tail];
        s_tail=(s_tail+1)%MY_FSK_RING_SIZE;
        count++;
    }
    return count;
}

void my_fsk_clear_buffer(void)
{
    s_head=0;
    s_tail=0;
    memset(s_ring,0,sizeof(s_ring));
}
