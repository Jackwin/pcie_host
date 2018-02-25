#pragma once
#ifndef _DMD_H_
#define _DMD_H_
#include <stdio.h>
#include "../../lib/altera_lib.h"

typedef struct {
    int pat_h_pix;  // pattern horizonal pixels
    int pat_v_pix;  // pattern vertical pixels
    int pat_total_pix; // pat_h_pix X pat_y_pix
    int pat_num;      // the number of the specific pattern
    int pat_fill_size; // The number of pixels to insert to meet 1080p
    int pat_start_addr; // The start address of patterns in FPGA DDR3
    int pat_end_addr; // The end address of patterns in FPGA DDR3
    int pat_rsv;  // Reserved
}DMD_DESCRIPTOR;

// Data format from DDR3/on-chip memory is Big-Endian
// data[7] data[6] data[5] data[4] data[3] data[2] data[1] data[0] = ddr_data[255:0]
typedef struct {
    int data[8]; // Every 8 32-bit constucts a data_unit because the data interface width in FPGA ddr3 user logic is 256-bit.
}PAT_DATA_UNIT;

typedef struct {
    DMD_DESCRIPTOR dmd_dt;
    PAT_DATA_UNIT pat_data[32]; // 256-bit, accomodate to the DDR3/on-chip memory data width
}DMD_PATTERN;


typedef struct {
    int fast_dmd_flag;
    int pat_h_pix;
    int pat_v_pix;
    int h_offset;
    int v_offset;
    int pat_fill_size;
    int rsv1;
    int rsv2;
}FAST_PAT_DESCRIPTOR;

typedef struct {
    FAST_PAT_DESCRIPTOR fast_dmd_dt;
    PAT_DATA_UNIT pat_data[1920 * 1080 / 256];
}FAST_DMD_PATTERN;

void init_dmd_dt();
void init_fast_dmd_pat_dt(int pat_h_pix, int pat_v_pix, int h_offset, int v_offset, int pat_fill_size, int fast_dmd_flag);
void init_fast_dma_pat_data(void);



#endif // !_DMD_H