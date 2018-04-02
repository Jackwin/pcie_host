#pragma once
#ifndef _PATTERN_H
#define _PATTERN_H
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#define PATTERN_NUMBER (257)
#define HORIZONTAL_PIXEL (1920)
#define VERTICAL_LINE  (1080)

//std::string mode1_name = "model1.txt";
#define PAGE_SIZE_HEX (4096 * 2)    //hex

//typedef struct FPGA_MEMORY_DATA {
//    int data[8]; // Every 8 32-bit constucts a data_unit because FPGA DDR3 data width is 256-bit.
//}FPGA_MEM_DATA;

//typedef struct {
//    FPGA_MEM_DATA pattern_data[1920 * 1080 / 256];
//}DMD_PATTERN;

typedef struct  {
    int data[1920 * 1080 / 32];
}DMD_PATTERN;

//DMD_PATTERN dmd_pattern_data;
DMD_PATTERN *dmd_pattern_ptr;
DMD_PATTERN dmd_pattern_data[PATTERN_NUMBER];
//int         dmd_pattern_data[PATTERN_NUMBER * (1920 * 1080 / 32)];

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
/*
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
*/
// Data format from DDR3/on-chip memory is Big-Endian
// data[7] data[6] data[5] data[4] data[3] data[2] data[1] data[0] = ddr_data[255:0]
typedef struct PATTERN_DATA_UNIT {
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
void init_fast_dmd_pat_dt(int pat_h_pix, int pat_line, int h_offset, int v_offset, int pat_fill_size, int fast_dmd_flag);
void init_fast_dma_pat_data(void);
char *CombinePatternName(char prefix[10], int pattern_seq, char format[10]);
FILE* OpenPattern(char file_name[], char mode[]);
void CloseFile(FILE *fp);
int CalcualtePatternSize(FILE *fp);
char Hex2Char(int data);
int ReadHex(char pattern_name[], int start_pos);
void GenerateDMDPattern(char prefix[], int pattern_num, char format[], int h_pix, int line, int mode);
int ReadDMDPattern(char pattern_name[], int start_pos, int length);
int BitsToInt(char *char_ptr);
int DuplicateBit(int bit, int n);
bool GetBit(int data, int pos);
int SetBit(int data, int pos);
void PatternFill(char pattern_name[], int h_pix, int v_line);
//void DMAOperation(int pattern_num, char *prefix, char *format);
void SetDMADesTable(int num, long dest_addr, long src_addr, int size);

#endif // !_FILE_H
