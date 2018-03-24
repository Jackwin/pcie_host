#include "pattern.h"

DMD_PATTERN_1920x1080 *dmd_pat_1920x1080_ptr;
FAST_DMD_PATTERN fast_dmd_pattern;
int one_line_pix_array[60] = { 0 };

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

// mode 1:-  2: | 3: =
void GenerateDMDPattern(char prefix[], int pattern_num, char format[], int h_pix, int line, int mode) {
    for (int k = 0; k < pattern_num; k++) {
        // Generate the pattern name
        char pattern_name[30];
        strcpy(pattern_name, prefix);
        printf("prefix is %s.\n", prefix);
        char num[6];
        itoa(pattern_num, num, 10);
        strcat(pattern_name, num);
        strcat(pattern_name, format);

        printf("pattern name is %s.\n", pattern_name);
        FILE *fp = OpenPattern(pattern_name, "w");
        char char_vec[4];
        switch (mode)
        {
        case 1:
            for (int i = 0; i < line; i++) {
                if ((i >(line / 2 - line / 12)) && (i  < (line / 2 + line / 12))) {
                    for (int k = 0; k < h_pix; k++) {
                        char_vec[k % 4] = 1;
                        if (k % 4 == 3) {  // bits to hex
                            int s = BitsToInt(char_vec);
                            char c = Hex2Char(s);
                            fputc(c, fp);
                        }
                    }
                    fputs('\n', fp);
                }
                else {
                    for (int k = 0; k < h_pix; k++) {
                        char_vec[k % 4] = 0;
                        if (k % 4 == 3) {
                            int s = BitsToInt(char_vec);
                            char c = Hex2Char(s);
                            fputc(c, fp);
                        }
                    }
                    fputs('\n', fp);
                }
            }
            CloseFile(fp);
            break;
        case 2:
            for (int i = 0; i < line; i++) {
                for (int k = 0; k < h_pix; k++) {
                    if (k >(h_pix / 2 - line / 12) && (k >(h_pix / 2 + line / 12))) {
                        char_vec[k % 4] = 1;
                    }
                    else {
                        char_vec[k % 4] = 0;
                    }
                    if (k % 4 == 3) {
                        int s = BitsToInt(char_vec);
                        char c = Hex2Char(s);
                        fputc(c, fp);
                    }
                }
                fputc('\n', fp);
            }
            CloseFile(fp);
            break;
        case 3:
            for (int i = 0; i < line; i++) {
                for (int k = 0; k < h_pix; k++) {
                    if (0 <= i  && i <= line / 4 || ((line / 4) * 3 <= i && i <= line)) {
                        char_vec[k % 4] = k % 2;
                    }
                    else {
                        char_vec[k % 4] = 0;
                    }
                    if (k % 4 == 3) {
                        int s = BitsToInt(char_vec);
                        char c = Hex2Char(s);
                        fputc(c, fp);
                    }
                }
                fputc('\n', fp);
            }
            CloseFile(fp);
            break;
        default:
            break;
        }
    }
}

/*Func: Read 8 hex and combine them to int
Param: length must be the multiples of 8
*/

int ReadDMDPattern(char pattern_name[], int start_pos, int length) {
    char ch = 0;
    int buf[8];
    int line_cnt = 0;
    int data;
    FILE *fp = OpenPattern(pattern_name, "r");
    int pos = start_pos;
    int pos_inc = 0;
    while (ch != EOF) {
        fseek(fp, pos, SEEK_SET); // the beginning of the file
        ch = fgetc(fp);;
        // Next line   
        if (ch == '\n') {
            line_cnt++;
            pos_inc++;
        }
        else {
            // Change from ASCII to integer
            if ('0' <= ch && ch <= '9')
                ch = ch - '0';
            else if ('A' <= ch  && ch <= 'F')
                ch = ch - 'A' + 10;
            else if ('a' <= ch  && ch <= 'f')
                ch = ch - 'a' + 10;

            buf[(pos_inc - line_cnt) % 8] = (int)ch;
            if ((pos_inc - line_cnt) % 8 == 7) {
                data = (buf[0] << 28) + (buf[1] << 24) + (buf[2] << 20) + (buf[3] << 16) + (buf[4] << 12) + (buf[5] << 8) + (buf[6] << 4) + buf[7];
                break;
            }
            pos_inc++;
        }
        pos = pos + 1;
    }
    CloseFile(fp);
    return data;
}

//Read a type data from the file and return the corresponding hex value
int ReadHex(char pattern_name[], int start_pos) {
    char ch = 0;
    int line_cnt = 0;
    int offset;
    int data = 0;
    int pos = start_pos;
    FILE *fp = OpenPattern(pattern_name, "r");
    while (ch != EOF) {
        fseek(fp, start_pos, SEEK_SET); // the beginning of the file
        ch = fgetc(fp);
        // Next line
        if (ch == '\n') {
            line_cnt++;
            pos++;
        }
        else {
            // Change from ASCII to integer
            if ('0' <= ch && ch <= '9')
                ch = ch - '0';
            else if ('A' <= ch  && ch <= 'F')
                ch = ch - 'A' + 10;
            else if ('a' <= ch  && ch <= 'f')
                ch = ch - 'a' + 10;
            data = (int)ch;
        }
        CloseFile(fp);
        return data;
    }
}

int BitsToInt(char *char_ptr) {
    int data_char = 0;

    for (int i = 0; i < 4; i++) {
        data_char = data_char | (*(char_ptr + i)) << (3 - i);
    }
    return data_char;
}

// Count the pattern size in the unit of half-byte(4-bit)
int CalcualtePatternSize(FILE *fp) {
    int len = 0;
    char ch = 0;
    ch = fgetc(fp);
    while (ch != EOF) {
        ch = fgetc(fp);
        if (ch != '\n')
            len++;
    }
    return len;
}

FILE* OpenPattern(char file_name[], char mode[]) {
    //printf("file_name is %s, mode is %s.\n", file_name, mode);
    FILE *fp = fopen(file_name, mode);
    if (fp == NULL) {
        printf("Fail to open the file %s.\n", file_name);
        exit(1);
    }
    //printf("Open successfully.\n");
    return fp;;
}

void CloseFile(FILE *fp) {
    fclose(fp);

}

char *CombinePatternName(char prefix[10], int pattern_seq, char format[10]) {
    char pattern_name[30];
    strcpy(pattern_name, prefix);
    printf("pattern name is %s.\n", pattern_name);
    char num[6];
    itoa(pattern_seq, num, 10);
    strcat(pattern_name, num);
    printf("pattern name is %s.\n", pattern_name);
    strcat(pattern_name, format);
    printf("pattern name is %s.\n", pattern_name);
    return pattern_name;
}

char Hex2Char(int data) {
    char ch = '0';
    if (0 <= data && data <= 9) {
        ch = data + 48;
    }
    else if (10 <= data && data <= 15) {
        ch = data + 'a' - 10;
    }
    else {
        printf("No valid Hex data.\n");
    }

    return ch;
}

// The maximum fill size is (64-1)
void PatternFill(char pattern_name[], int h_pix, int v_line) {
    // The memory space to store one line 1920 pixels. 1920/32
    int h_fill_size = HORIZONTAL_PIXEL / h_pix - 1;
    int v_fill_size = VERTICAL_LINE / v_line - 1;

    int bit_cnt_per_line = 0;
    int duplicate = HORIZONTAL_PIXEL / h_pix;

    for (int j = 0; j < v_line; j++) {
        // For every line, set 0 for every bit
        for (int i = 0; i < 60; i++) {
            one_line_pix_array[i] = 0;
        }
        for (int n = 0; n < VERTICAL_LINE / v_line; n++){
            bit_cnt_per_line = 0;
            if (n == 0) {
                for (int i = 0; i < h_pix / 4; i++) {
                    int hex = ReadHex(pattern_name, i + j * (h_pix/4 + 2));
                    // Loop for the 4 bits in a hex
                    for (int k = 0; k < 4; k++) {
                        int bit = GetBit(hex, 3 - k); // Get the bit from the most-left to right
                        if (bit == 1) {
                            // Duplicate the bit
                            for (int m = 0; m < HORIZONTAL_PIXEL / h_pix; m++) {
                                one_line_pix_array[bit_cnt_per_line / 32] = SetBit(one_line_pix_array[bit_cnt_per_line / 32], (31 - ((i * 4 + k) * duplicate + m)));
                                bit_cnt_per_line++;
                            }
                        }
                        else {
                            // The bit is default 0. No need to fill 0
                            bit_cnt_per_line = bit_cnt_per_line + HORIZONTAL_PIXEL / h_pix;
                        }
                    }
                    int line_offset = j * (VERTICAL_LINE / v_line) + 0;  // The line offset in 1080 lines
                   // In one line, 1920 bits equal to 60 int-types (32-bit)
                   // for (int h = 0; h < HORIZONTAL_PIXEL / 32; h++) {
                        // Assign one line pix int-array to one pattern memory
                  //      dmd_pttern_data.pattern_data[(line_offset * 60 + h) / 8].data[(line_offset * 60 + h) % 8] = one_line_pix_array[h];
                  //  }
                }
                if (j == 1) {
                    for (int mm = 0; mm < 60; mm++)
                        printf("one_line_pix_array[%d] is %x.\n ", mm, one_line_pix_array[mm]);
                }
            }
            // Just copy the last line data
            else {
                int line_offset = j * (VERTICAL_LINE / v_line) + n;  // The offset line in 1080 lines
                // In one line, 1920 bits equal to 60 int-types (32-bit)
               // for (int h = 0; h < 60; h++) {
                    // Assign one line pix int-array to one pattern memory
                //    dmd_pttern_data.pattern_data[(line_offset * 60 + h) / 8].data[(line_offset * 60 + h) % 8] = one_line_pix_array[h];
              //  }
            }
        }
   }
}

// The maximum duplication number is 31
// The bit is only 1 or 0, output n bit.
// Assume bit = 1, n = 4, output = f
int DuplicateBit(int bit, int n) {
    int data = 1;
    if (bit == 1 && n > 1) {
        for (int i = 1; i < n; i++) {
            data = (data << 1) | data;
        }
        return data;
    }
    else if (bit == 1 && n == 1) {
        return data;
    }
    else if (bit == 0) {
        return 0;
    }

}
// pos starts from 0.
bool GetBit(int data, int pos) {
    return ((data >> pos) & 1);
}

// Set '1' in the pos
int SetBit(int data, int pos) {
    int val = data;
    val = val | (1 << pos);
    return val;
}