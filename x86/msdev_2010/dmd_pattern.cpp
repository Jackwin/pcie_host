#include <fstream>

#include <iostream>
#include "dmd_pattern.h"
#include <stdio.h>
#include <stdlib.h>
#include <string>
using namespace std;
//#define PATTERN_PATH ".\\pattern_data\\"


void SetParam(int h_pix, int line, int total_pix, int num, int h_fill_size, int v_fill_size, int fpga_start_addr, int fpga_end_addr) {
    h_pix = h_pix;
    line = line;
    total_pix = total_pix;
    num = num;
    h_fill_size = h_fill_size;
    v_fill_size = v_fill_size;
    fpga_start_addr = fpga_start_addr;
    fpga_end_addr = fpga_end_addr;
}

// 3 types of mode 1:-  2: | 3: =
void GenerateDMAPattern(int h_pix, int line, int mode, int pattern_num) {

    for (int k = 0; k < pattern_num; k++) {
        string pattern_name("pattern_");
        int pattern_name_len = pattern_name.length();
        pattern_name.insert(pattern_name_len, to_string(k));
        pattern_name.insert(pattern_name.length(), ".txt");
        ofstream outfile( pattern_name, ofstream::out);
        char char_vec[8];
        if (!outfile) {
            cerr << pattern_name << endl;
            exit(1);
        }
        switch (mode)
        {
        case 1:
            for (int i = 0; i < line; i++) {
                if ((i >(line / 2 - line / 12)) && (i  < (line / 2 + line / 12))) {
                    for (int k = 0; k < h_pix; k++) {
                        char_vec[k % 4] = 1;
                        if (k % 4 == 3) {  // bits to hex
                            int s = BitsToInt(char_vec);
                            outfile << hex << s;
                        }
                    }
                    outfile << "\n";
                }
                else {
                    for (int k = 0; k < h_pix; k++) {
                        char_vec[k % 4] = 0;
                        if (k % 4 == 3) {
                            int s = BitsToInt(char_vec);
                            outfile << hex << s;
                        }
                    }
                    outfile << "\n";
                }
            }
            outfile.close();
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
                        outfile << hex << s;
                    }
                }
                outfile << "\n";
            }
            outfile.close();
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
                        outfile << hex << s;
                    }
                }
                outfile << "\n";
            }
        default:
            break;
        }
    }

}
// Read DMD pattern data from the disk
int ReadDMDPattern(int h_pix, int line, string pattern_name, int start_pos, int length) {
    char c;
    int buf[8];
    int line_cnt = 0;
    int offset;
    int data;
    ifstream infile(pattern_name);
    if (!infile.is_open()) {
        cout << "Error opening file";
        exit(1);
    }
    int pos = start_pos;
    while (!infile.eof()) {
        infile.seekg(pos, ios::beg);
        infile.get(c);
        if (infile.fail())
        {
            break;
        }
        // Next line
        if (c == 10) {
            line_cnt++;
            pos++;
        }
        else {
            // Change from ASCII to integer
            if ('0' <= c && c <= '9')
                c = c - '0';
            else if ('A' <= c  && c <= 'F')
                c = c - 'A' + 10;
            else if ('a' <= c  && c <= 'f')
                c = c - 'a' + 10;

            buf[(pos - line_cnt) % 8] = (int)c;
            if ((pos - line_cnt) % 8 == 7) {
                offset = (pos - line_cnt) / 8;
                data = (buf[0] << 28) + (buf[1] << 24) + (buf[2] << 20) + (buf[3] << 16) + (buf[4] << 12) + (buf[5] << 8) + (buf[6] << 4) + buf[7];
            }
            if ((pos - line_cnt) == (start_pos + length - 1)) break;
            pos++;
        }
    }
    infile.close();
    return data;
}

int BitsToInt(char *char_ptr) {
    int data_char = 0;

    for (int i = 0; i < 4; i++) {
        data_char = data_char | (*(char_ptr + i)) << (3 - i);
    }
    return data_char;
}

