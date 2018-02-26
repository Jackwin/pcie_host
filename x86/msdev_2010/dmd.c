#include "dmd.h"

DMD_PATTERN dmd_pat;
FAST_DMD_PATTERN fast_dmd_pattern;
void init_dmd_dt(void) {
    
    dmd_pat.dmd_dt.pat_h_pix = 20;
    dmd_pat.dmd_dt.pat_v_pix = 20;
    dmd_pat.dmd_dt.pat_total_pix = dmd_pat.dmd_dt.pat_h_pix * dmd_pat.dmd_dt.pat_v_pix;
    dmd_pat.dmd_dt.pat_fill_size = 1920 / dmd_pat.dmd_dt.pat_h_pix - 1;
    dmd_pat.dmd_dt.pat_num = 4;
    dmd_pat.dmd_dt.pat_start_addr = 1;
    dmd_pat.dmd_dt.pat_end_addr = (dmd_pat.dmd_dt.pat_total_pix % 256 == 0) ? dmd_pat.dmd_dt.pat_num * (dmd_pat.dmd_dt.pat_total_pix / 256) 
        : dmd_pat.dmd_dt.pat_num * (dmd_pat.dmd_dt.pat_total_pix / 256 + 1);

    dmd_pat.pat_data[0].data[0] = 0xffaaff77;
    dmd_pat.pat_data[0].data[1] = 0xffbbffbb;
    dmd_pat.pat_data[0].data[2] = 0xffccffcc;
    dmd_pat.pat_data[0].data[3] = 0xffddffdd;
    dmd_pat.pat_data[0].data[4] = 0xffeeffee;
    dmd_pat.pat_data[0].data[5] = 0xff99ff99;
    dmd_pat.pat_data[0].data[6] = 0xff88ff88;
    dmd_pat.pat_data[0].data[7] = 0xff77ff77;

    dmd_pat.pat_data[1].data[0] = 0xffbbffbb;
    dmd_pat.pat_data[1].data[1] = 0xffccffcc;
    dmd_pat.pat_data[1].data[2] = 0xffddffdd;
    dmd_pat.pat_data[1].data[3] = 0xffeeffee;
    dmd_pat.pat_data[1].data[4] = 0xff99ff99;
    dmd_pat.pat_data[1].data[5] = 0xff88ff88;
    dmd_pat.pat_data[1].data[6] = 0xff77ff77;
    dmd_pat.pat_data[1].data[7] = 0xffaaffaa;

    dmd_pat.pat_data[2].data[0] = 0xffccffcc;
    dmd_pat.pat_data[2].data[1] = 0xffddffdd;
    dmd_pat.pat_data[2].data[2] = 0xffeeffee;
    dmd_pat.pat_data[2].data[3] = 0xff99ff99;
    dmd_pat.pat_data[2].data[4] = 0xff88ff88;
    dmd_pat.pat_data[2].data[5] = 0xff77ff77;
    dmd_pat.pat_data[2].data[6] = 0xffaaffaa;
    dmd_pat.pat_data[2].data[7] = 0xffbbffbb;


    dmd_pat.pat_data[3].data[0] = 0xffddffdd;
    dmd_pat.pat_data[3].data[1] = 0xffeeffee;
    dmd_pat.pat_data[3].data[2] = 0xff99ff99;
    dmd_pat.pat_data[3].data[3] = 0xff88ff88;
    dmd_pat.pat_data[3].data[4] = 0xff77ff77;
    dmd_pat.pat_data[3].data[5] = 0xffaaffaa;
    dmd_pat.pat_data[3].data[6] = 0xffbbffbb;
    dmd_pat.pat_data[3].data[7] = 0xffccffcc;
}

// Function: Initialize the fast DMD pattern descriptor
void init_fast_dmd_pat_dt(int pat_h_pix, int pat_v_pix, int h_offset, int v_offset, int pat_fill_size, int fast_dmd_flag) {
    fast_dmd_pattern.fast_dmd_dt.pat_h_pix = pat_h_pix;
    fast_dmd_pattern.fast_dmd_dt.pat_v_pix = pat_v_pix;
    fast_dmd_pattern.fast_dmd_dt.h_offset = h_offset;
    fast_dmd_pattern.fast_dmd_dt.v_offset = v_offset;
    fast_dmd_pattern.fast_dmd_dt.pat_fill_size = pat_fill_size;
   
    fast_dmd_pattern.fast_dmd_dt.rsv1 = 0;
    fast_dmd_pattern.fast_dmd_dt.rsv2 = 0;
    fast_dmd_pattern.fast_dmd_dt.fast_dmd_flag = fast_dmd_flag;
}

// Fuction: Initialize the fast DMD pattern data
void init_fast_dma_pat_data(void) {
    fast_dmd_pattern.pat_data[0].data[0] = 0x00010203;
    fast_dmd_pattern.pat_data[0].data[1] = 0x04050607;
    fast_dmd_pattern.pat_data[0].data[2] = 0x08090a0b;
    fast_dmd_pattern.pat_data[0].data[3] = 0x0c0d0e0f;
    fast_dmd_pattern.pat_data[0].data[4] = 0x10111213;
    fast_dmd_pattern.pat_data[0].data[5] = 0x14151617;
    fast_dmd_pattern.pat_data[0].data[6] = 0x18191a1b;
    fast_dmd_pattern.pat_data[0].data[7] = 0x1c1d1e1f;

    for (int i = 1; i < (1920 * 1080 / 256); i++) {
        for (int j = 0; j < 8; j++)
       // fast_dmd_pattern.pat_data[i].data[j] = fast_dmd_pattern.pat_data[i - 1].data[j] + 0x01010101;
            fast_dmd_pattern.pat_data[i].data[j] = 0;
    }

}
void init_fast_pattern(void) {
    PAT_DATA_UNIT fast_pattern[1920 * 1080 / 256];

}