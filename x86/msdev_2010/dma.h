#pragma once
#ifndef _DMA_H_
#define _DMA_H_
#include "altera_lib.h"
#include "pattern.h"
//#define   DMA_SIZE_PER_DESCRIPTOR (1 << 18)  //256K
#define   DMA_SIZE_PER_DESCRIPTOR 0x3f480 // 1920x1080/8
// Register address

#define     START_PLAY_REG_ADDR (ONCHIP_MEM_BASE_ADDR_LOW +0x0)
#define     CAPTURE_PULSE_CYCLE_REG_ADDR (ONCHIP_MEM_BASE_ADDR_LOW +0x4)
#define     START_ADDR_REG_ADDR (ONCHIP_MEM_BASE_ADDR_LOW +0x8)
#define     TO_SNED_TOTAL_BYTE_REG_ADDR (ONCHIP_MEM_BASE_ADDR_LOW +0xc)
#define     PATTERN_REG_ADDR (ONCHIP_MEM_BASE_ADDR_LOW +0x10)
#define     ONE_FRAME_BYTE_REG_ADDR (ONCHIP_MEM_BASE_ADDR_LOW +0x14)
#define     TO_SEND_FRAME_NUM_REG_ADDR (ONCHIP_MEM_BASE_ADDR_LOW +0x18)
#define     RSV_ADDR  ( ONCHIP_MEM_BASE_ADDR_LOW + 0x1c)

#define     START_PLAY 0x80000000
#define     DDR3_SOURCE 0x40000000
#define     ONCHIP_MEM_SOURCE 0x0

// FPGA DDR3 is 1GByte, and is divided into 16 section, of which each is 64MByte.
#define     SECTION_SIZE 0x4000000
// Section controller address
#define     SEC_CTRL_ADDR 0x0
#define     WPS_INTR_ADDR 0x30
#define     USR_INTR_ADDR 0x40

#define     MAX_TABLE_SIZE 127
typedef struct {

    int start_play_reg;
    int capture_pulse_cycle_reg;
    int start_addr_reg;
    int to_send_total_byte_reg;
    int pattern_reg;
    int one_frame_byte_reg;
    int to_send_frame_num_reg;
    int rsv_reg;
}WPS_REG;

DWORD InitDMABookkeep(WDC_DEVICE_HANDLE hDev, WD_DMA **ppDma, WD_DMA **ppDma_wr, WD_DMA **ppDMA_rd_buf, WD_DMA **ppDMA_wr_buf);
//void DMAOperation(int pattern_num, char *prefix, char *format, int h_pix, int v_line, BOOL fromDev, DWORD vendor_id, DWORD device_id, DMD_PATTERN *dmd_pat_ptr);
int FPGAReadFromCPU(DWORD vendor_id, DWORD device_id, int *source_data_ptr, int data_size, int byte_per_descriptor, int fpga_ddr3_addr_offset, int fpga_onchip_addr_offset, int target);
void GeneratePatternData(int pattern_num, char *prefix, char *format, int h_pix, int v_line);
int* FPGAWriteToCPU(int data_size, int byte_per_descriptor, int fpga_ddr3_addr_offset, int fpga_onchip_addr_offset, int target);
int DMAOperation(DWORD vendor_id, DWORD device_id);
int *ApplyMemorySpace(int byte_size, DWORD direction);
int DMAToOnchipMem(DMA_ADDR cpu_memory_start_addr, int byte_size, int fpga_onchip_addr_offset);
void IntHandler(WDC_DEVICE_HANDLE hDev_0, PCI_DRIVER_INT_RESULT *pIntResult);
//void ApplyMemorySpace(DMD_PATTERN * source_data_ptr, int byte_size, DWORD direction);
//int DMAToOnchipMem(int byte_size, int fpga_onchip_addr_offset);
BOOL SelectSection(int sectionID);
BOOL WriteReg(DWORD addr, DWORD wr_data);
DWORD ReadReg(DWORD addr);
BOOL CfgWPSReg(WPS_REG wps_register);
#endif // !_DMA_H

