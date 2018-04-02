#pragma once
#ifndef _DMA_H_
#define _DMA_H_
#include "altera_lib.h"
#include "pattern.h"
void SetDMADesTable(int num, long dest_addr, long src_addr, int size);
BOOL SetDescTable(struct dma_descriptor *dma_desc, DWORD source_addr_high, DWORD source_addr_low, DWORD dest_addr_high, DWORD dest_addr_low, DWORD ctl_dma_len, WORD id);
BOOL ConfigDMADescController(WDC_DEVICE_HANDLE hDev, DMA_ADDR desc_table_start_addr, int table_size, BOOL fromDev);
DWORD InitDMABookkeep(WDC_DEVICE_HANDLE hDev, WD_DMA **ppDma, WD_DMA **ppDma_wr, WD_DMA **ppDMA_rd_buf, WD_DMA **ppDMA_wr_buf);
void DMAOperation(int pattern_num, char *prefix, char *format, int h_pix, int v_line, BOOL fromDev, DWORD vendor_id, DWORD device_id, DMD_PATTERN *dmd_pat_ptr);
void GeneratePatternData(int pattern_num, char *prefix, char *format, int h_pix, int v_line);
#endif // !_DMA_H

