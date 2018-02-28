#include <iostream>
#include <fstream>
using namespace std;
#include "dmd.h"

DMD_PATTERN_1920x1080 *dmd_pat_1920x1080_ptr;
FAST_DMD_PATTERN fast_dmd_pattern;

void GenerateDMDPattern(int h_pix, int line, int pat_num, char *file_name) {
    PAT_DATA_UNIT data_unit;
    ofstream in;
    in.open("dmd_pat_1920x1080.txt", ios::trunc);
    int unit_num = (h_pix * line) % 256 ? (h_pix * line / 256 + 1) : (h_pix * line / 256);
    for (int i = 0; i < unit_num; i++) {
        data_unit.data[0] = i;
        data_unit.data[1] = 0x00010203;
        for (int j = 2; j < 8; j++) {
            data_unit.data[j] = data_unit.data[j - 1] + 0x04040404;
        }

    }
}
void SetDMDDT(DMD_DT dmd_dt_1920x1080) {
    dmd_pat_1920x1080_ptr = (DMD_PATTERN_1920x1080 *)malloc(sizeof(DMD_PATTERN_1920x1080));
    dmd_pat_1920x1080_ptr->dt.pat_h_pix = dmd_dt_1920x1080.pat_h_pix;
    dmd_pat_1920x1080_ptr->dt.pat_line = dmd_dt_1920x1080.pat_line;
    dmd_pat_1920x1080_ptr->dt.pat_total_pix = dmd_dt_1920x1080.pat_total_pix;
    dmd_pat_1920x1080_ptr->dt.pat_fill_size = dmd_dt_1920x1080.pat_fill_size;
    dmd_pat_1920x1080_ptr->dt.pat_num = dmd_dt_1920x1080.pat_num;
    dmd_pat_1920x1080_ptr->dt.pat_start_addr = dmd_dt_1920x1080.pat_start_addr;
    dmd_pat_1920x1080_ptr->dt.pat_end_addr = dmd_dt_1920x1080.pat_end_addr;
    dmd_pat_1920x1080_ptr->dt.pat_rsv = dmd_dt_1920x1080.pat_rsv;



}

// Function: Initialize the fast DMD pattern descriptor
void init_fast_dmd_pat_dt(int pat_h_pix, int pat_line, int h_offset, int v_offset, int pat_fill_size, int fast_dmd_flag) {
    fast_dmd_pattern.fast_dmd_dt.pat_h_pix = pat_h_pix;
    fast_dmd_pattern.fast_dmd_dt.pat_line = pat_line;
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