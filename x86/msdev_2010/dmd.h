#pragma once
#ifndef _DMD_H_
#define _DMD_H_
#include <stdio.h>
#include "../../lib/altera_lib.h"

typedef struct DMD_DESCRIPTOR {
    int pat_h_pix;  // pattern horizonal pixels
    int pat_line;  // pattern vertical pixels
    int pat_total_pix; // pat_h_pix X pat_y_pix
    int pat_num;      // the number of the specific pattern
    int pat_fill_size; // The number of pixels to insert to meet 1080p
    int pat_start_addr; // The start address of patterns in FPGA DDR3
    int pat_end_addr; // The end address of patterns in FPGA DDR3
    int pat_rsv;  // Reserved
}DMD_DT;

DMD_DT dt_1920x1080 = {
    .pat_h_pix = 1920,
    .pat_line = 1080,
    .pat_total_pix = 1920 * 1080,
    .pat_num = 1000,
    .pat_fill_size = 0,
    .pat_start_addr = 2,
    .pat_end_addr = 2 + 1920 * 1080/ 256 * 1000,
    .pat_rsv = 0,
};

DMD_DT dt_480x270 = {
    .pat_h_pix = 480,
    .pat_line = 270,
    .pat_total_pix = 480 * 270,
    .pat_num = 1000,
    .pat_fill_size = 4,
    .pat_start_addr = 2 + 1920 * 1080 / 256 * 1000 + 1,
    .pat_end_addr = 2 + 1920 * 1080 / 256 * 1000 + 1 + (480 * 270 / 256 + 1) * 1000,
    .pat_rsv = 0,
};

// Data format from DDR3/on-chip memory is Big-Endian
// data[7] data[6] data[5] data[4] data[3] data[2] data[1] data[0] = ddr_data[255:0]
typedef struct PATTERN_DATA_UNIT{
    int data[8]; // Every 8 32-bit constucts a data_unit because the data interface width in FPGA ddr3 user logic is 256-bit.
}PAT_DATA_UNIT;

typedef struct {
    DMD_DT dt;
    PAT_DATA_UNIT data[1920 * 1080 / 256]; // 256-bit, accomodate to the DDR3/on-chip memory data width
}DMD_PATTERN_1920x1080;

typedef struct {
    DMD_DT dt;
    PAT_DATA_UNIT data[480 * 270 / 256]; // 256-bit, accomodate to the DDR3/on-chip memory data width
}DMD_PATTERN_480x270;

typedef struct {
    int fast_dmd_flag;
    int pat_h_pix;
    int pat_line;
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

void SetDMDDT(DMD_DT dmd_dt_1920x1080);
void GenerateDMDPattern(int h_pix, int line, int pat_num);
void init_fast_dmd_pat_dt(int pat_h_pix, int pat_line, int h_offset, int v_offset, int pat_fill_size, int fast_dmd_flag);
void init_fast_dma_pat_data(void);



#endif // !_DMD_H