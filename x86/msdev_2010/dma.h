#pragma once
#ifndef _DMA_H_
#define _DMA_H_
#include "altera_lib.h"
#include "pattern.h"
//#define   DMA_SIZE_PER_DESCRIPTOR (1 << 18)  //256K
#define   DMA_SIZE_PER_DESCRIPTOR 0x3f480 // 1920x1080/8
// Register address
#define     RSV3_ADDR  ((ONCHIP_MEM_BASE_ADDR_HI << 32) | ONCHIP_MEM_BASE_ADDR_LOW + 0x0)
#define     CAPTURE_PULSE_CYCLE_REG_ADDR (ONCHIP_MEM_BASE_ADDR_LOW +0x04)
#define     START_ADDR_REG_ADDR (ONCHIP_MEM_BASE_ADDR_LOW +0x08)
#define     TO_SNED_TOTAL_BYTE_REG_ADDR (ONCHIP_MEM_BASE_ADDR_LOW +0x0c)
#define     PATTERN_REG_ADDR (ONCHIP_MEM_BASE_ADDR_LOW +0x10)
#define     ONE_FRAME_BYTE_REG_ADDR (ONCHIP_MEM_BASE_ADDR_LOW +0x14)
#define     TO_SEND_FRAME_NUM_REG_ADDR (ONCHIP_MEM_BASE_ADDR_LOW +0x18)
#define     START_PLAY_REG_ADDR (ONCHIP_MEM_BASE_ADDR_LOW +0x1c)
#define     START_PLAY 0x80000000
#define     DDR3_SOURCE 0x40000000
#define     ONCHIP_MEM_SOURCE 0x0
typedef struct {
    int rsv_reg;
    int capture_pulse_cycle_reg;
    int start_addr_reg;
    int to_send_total_byte_reg;
    int pattern_reg;
    int one_frame_byte_reg;
    int to_send_frame_num_reg;
    int start_play_reg;
}WPS_REG;


void SetDMADesTable(int num, long dest_addr, long src_addr, int size);
BOOL SetDescTable(struct dma_descriptor *dma_desc, DWORD source_addr_high, DWORD source_addr_low, DWORD dest_addr_high, DWORD dest_addr_low, DWORD ctl_dma_len, WORD id);
BOOL ConfigDMADescController(WDC_DEVICE_HANDLE hDev, DMA_ADDR desc_table_start_addr, int table_size, BOOL fromDev);
DWORD InitDMABookkeep(WDC_DEVICE_HANDLE hDev, WD_DMA **ppDma, WD_DMA **ppDma_wr, WD_DMA **ppDMA_rd_buf, WD_DMA **ppDMA_wr_buf);
//void DMAOperation(int pattern_num, char *prefix, char *format, int h_pix, int v_line, BOOL fromDev, DWORD vendor_id, DWORD device_id, DMD_PATTERN *dmd_pat_ptr);
int FPGA_read(DWORD vendor_id, DWORD device_id, int *source_data_ptr, int data_size, int byte_per_descriptor, int fpga_ddr3_addr_offset, int fpga_onchip_addr_offset, int target);
void GeneratePatternData(int pattern_num, char *prefix, char *format, int h_pix, int v_line);
int DMAOperation(DWORD vendor_id, DWORD device_id);
#endif // !_DMA_H

