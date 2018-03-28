#include "dma.h"
#include "pattern.h"
#include "pci_driver_lib.h"
#include "windrvr_int_thread.h"
#include "altera_lib.h"
int dma_data_buf[1024]; // 4096Byte/4Byte = 1024

//dma_descriptor dma_dt[128];
void SetDMADesTable(int num, long dest_addr, long src_addr, int size) {
    printf("   Set the DMA Descriptor Table for the pattern.\n");
    printf("The destination addres is %x, size is %d byte.\n", dest_addr, size);
}
// Set descriptor table
BOOL SetDescTable(struct dma_descriptor *dma_desc, DWORD source_addr_high, DWORD source_addr_low, DWORD dest_addr_high, DWORD dest_addr_low, DWORD ctl_dma_len, WORD id) {
    dma_desc->src_addr_low = source_addr_low;
    dma_desc->src_addr_high = source_addr_high;
    dma_desc->dest_addr_low = dest_addr_low;
    dma_desc->dest_addr_high = dest_addr_high;
    dma_desc->ctl_dma_len = ctl_dma_len | (id << 18);
    dma_desc->reserved[0] = 0x0;
    dma_desc->reserved[1] = 0x0;
    dma_desc->reserved[2] = 0x0;
    return 1;
}

BOOL ConfigDMADescController(WDC_DEVICE_HANDLE hDev, DMA_ADDR desc_table_start_addr,int table_size, BOOL fromDev) {
    DWORD dt_phy_addr_h = (desc_table_start_addr >> 32) & 0xffffffff;
    DWORD dt_phy_addr_l = desc_table_start_addr & 0xffffffff;

    // Move data from CPU to FPGA
    if (!fromDev) {
        // Program the address of descriptor table to DMA descriptor controller
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_RC_HIGH_SRC_ADDR, dt_phy_addr_h);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_RC_LOW_SRC_ADDR, dt_phy_addr_l);
       // WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTRL_HIGH_DEST_ADDR, RD_CTRL_BUF_BASE_LOW);
       // WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTLR_LOW_DEST_ADDR, RD_CTRL_BUF_BASE_LOW);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_TABLE_SIZE, table_size);
    }
    else {
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_RC_HIGH_SRC_ADDR, dt_phy_addr_h);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_RC_LOW_SRC_ADDR, dt_phy_addr_l);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_CTRL_HIGH_DEST_ADDR, WR_CTRL_BUF_BASE_LOW);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_CTLR_LOW_DEST_ADDR, WR_CTRL_BUF_BASE_LOW);
        //WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_TABLE_SIZE, (ALTERA_DMA_DESCRIPTOR_NUM-1));
    }
    return TRUE;
}


DWORD InitDMABookkeep(WDC_DEVICE_HANDLE hDev, WD_DMA **ppDma, WD_DMA **ppDma_wr, WD_DMA **ppDMA_rd_buf, WD_DMA **ppDMA_wr_buf) {
    printf("altera_pcie_dma_bookkeep size is %d.\n", sizeof(struct altera_pcie_dma_bookkeep));
    // bk_ptr = (struct altera_pcie_dma_bookkeep *) malloc(sizeof(struct altera_pcie_dma_bookkeep));
    // DWORD status = WDC_DMASGBufLock(hDev, bk_ptr, DMA_TO_DEVICE, sizeof(struct altera_pcie_dma_bookkeep), ppDma);

    PVOID ppBuf;
    DWORD status = WDC_DMAContigBufLock(hDev, &ppBuf, DMA_TO_DEVICE, sizeof(struct altera_pcie_dma_bookkeep), ppDma);
    //ppBuf = (struct altera_pcie_dma_bookkeep *) malloc(sizeof(struct altera_pcie_dma_bookkeep));
    bk_ptr = (struct altera_pcie_dma_bookkeep *)ppBuf;
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to initiate DMAContigBuf.\n");
        printf("status is %x.\n", status);
    }
    printf("DMA page number is %d.\n", (*ppDma)->dwPages);

    status = WDC_DMASyncCpu(*ppDma);
    if (WD_STATUS_SUCCESS != status) {
        printf("Fail to CPUSync ppDMA.\n");
    }

    // status = WDC_DMASGBufLock(hDev, &(bk_ptr->lite_table_wr_cpu_virt_addr), DMA_TO_DEVICE, sizeof(struct altera_pcie_dma_bookkeep), ppDma_wr);
    // Because of the size of structure altera_pcie_dma_bookkeep is over than 4KB page size, it needs to use ContigBufLock to apply space for altera_pcie_dma_bookkeep
    // DWORD status = WDC_DMAContigBufLock(hDev, bk_ptr, DMA_TO_DEVICE, sizeof(struct altera_pcie_dma_bookkeep), ppDma);
    // if (status != WD_STATUS_SUCCESS) {
    //     printf("Fail to initiate DMAContigBuf for ppDma_wr.\n");
    //  }
    //printf("DMA page number in wr is %d.\n", (*ppDma_wr)->dwPages);

    // status = WDC_DMASyncCpu(*ppDma_wr);
    if (WD_STATUS_SUCCESS != status) {
        printf("Fail to CPUSync ppDMA_wr.\n");
    }


    //bk_ptr->dma_status.altera_dma_num_dwords = ALTERA_DMA_NUM_DWORDS;
    //bk_ptr->dma_status.altera_dma_descriptor_num = ALTERA_DMA_DESCRIPTOR_NUM;

    bk_ptr->numpages = (PAGE_SIZE >= MAX_NUM_DWORDS * 4) ? 1 : (int)((MAX_NUM_DWORDS * 4) / PAGE_SIZE);

    set_lite_table_header(&(bk_ptr->lite_table_rd_cpu_virt_addr.header));
    set_lite_table_header(&(bk_ptr->lite_table_wr_cpu_virt_addr.header));

    bk_ptr->lite_table_rd_bus_addr = (*ppDma)->Page[0].pPhysicalAddr;
    // bk_ptr->lite_table_wr_bus_addr = (*ppDma_wr)->Page[0].pPhysicalAddr;

    // DMA read operation, moving data from host to Device. Apply space for rd buffer
    /*
    rp_rd_buffer = (DWORD *)malloc(RP_RD_BUFFER_SZIE);
    init_rp_mem(rp_rd_buffer,RP_RD_BUFFER_SZIE/4);
    status = WDC_DMASGBufLock(hDev, rp_rd_buffer, DMA_TO_DEVICE, RP_RD_BUFFER_SZIE, ppDMA_rd_buf);
    */
    //Initialize fast DMD pattern
   // init_fast_dmd_pat_dt(1920, 1080, 0, 0, 0, 0xfdfdfdfd);
   // init_fast_dma_pat_data();
    //status = WDC_DMASGBufLock(hDev, &fast_dmd_pattern, DMA_TO_DEVICE, sizeof(FAST_DMD_PATTERN), ppDMA_rd_buf);
    //status = WDC_DMASGBufLock(hDev, &dmd_pattern_data, DMA_TO_DEVICE, sizeof(DMD_PATTERN), ppDMA_rd_buf);
    //printf("sizeof fast_dmd_patten is %d.\n", sizeof(DMD_PATTERN));

    //Apply for continuous memory space for pattern data
    status = WDC_DMAContigBufLock(hDev, &ppBuf, DMA_TO_DEVICE, sizeof(DMD_PATTERN), ppDMA_rd_buf);
    //ppBuf = (struct altera_pcie_dma_bookkeep *) malloc(sizeof(struct altera_pcie_dma_bookkeep));
    dmd_pattern_ptr = (DMD_PATTERN *)ppBuf;
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to initiate DMAContigBuf for dmd_pattern.\n");
        printf("status is %x.\n", status);
    }
    printf("dmd_pattern page number is %d.\n", (*ppDMA_rd_buf)->dwPages);

    status = WDC_DMASyncCpu(*ppDMA_rd_buf);
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to CPUSync ppDMA.\n");
    }
    // bk_ptr->dma_status.altera_dma_num_dwords = (*ppDMA_rd_buf)->Page[0].dwBytes;
    // DMA write operation, moving data from device to pcie. Apply space for wr buffer
   // ep_wr_buffer = (DWORD *)malloc(RP_RD_BUFFER_SZIE);
   // status = WDC_DMASGBufLock(hDev, ep_wr_buffer, DMA_FROM_DEVICE, EP_WR_BUFFER_SZIE, ppDMA_wr_buf);
   // if (status != WD_STATUS_SUCCESS) {
   //     printf("Fail to initiate DMASGBuf for ep_wr_buf.\n");
   // }
    //status = WDC_DMASyncCpu(*ppDMA_wr_buf);
    //if (status != WD_STATUS_SUCCESS) {
     //   printf("Fail to CPUSync ppDMA.\n");
   // }

    (bk_ptr->rp_rd_buffer_virt_addr) = (*ppDMA_rd_buf)->pUserAddr;
    (bk_ptr->rp_rd_buffer_bus_addr) = (*ppDMA_rd_buf)->Page[0].pPhysicalAddr;
    //(bk_ptr->rp_wr_buffer_virt_addr) = (*ppDMA_wr_buf)->pUserAddr;
   // (bk_ptr->rp_wr_buffer_bus_addr) = (*ppDMA_wr_buf)->Page[0].pPhysicalAddr;
    return status;

}

// Start DMA
void DMAOperation(int pattern_num, char *prefix, char *format, int h_pix, int v_line, BOOL fromDev, DWORD vendor_id, DWORD device_id) {
    int next_pattern_size = 0;
    long des_addr = 0;
    int dma_dt_cnt = 0;

    DWORD last_id = 0, write_127 = 0;
    DWORD timeout;
    WD_DMA *ppDMA = NULL, *ppDMA_wr = NULL, *ppDMA_rd_buf = NULL, *ppDMA_wr_buf = NULL;
    PVOID pBuf = NULL;
    DWORD status;

    // Address variables
    //DMA_ADDR onchip_mem_base_addr = (ONCHIP_MEM_BASE_ADDR_HI << 32) + ONCHIP_MEM_BASE_ADDR_LOW;

    DMA_ADDR onchip_mem_base_addr = (DDR_MEM_BASE_ADDR_HI << 32) + DDR_MEM_BASE_ADDR_LOW;
    DMA_ADDR onchip_mem_start_addr;
    DWORD current_page_size, pre_page_size;

    if (!PCI_Get_WD_handle(&hDev)) {
        printf("Fail to get WD_handle.\n");
        return 0;
    }
    WD_Close(hDev);
    status = initialize_PCI(vendor_id, device_id);
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to initialize PCI.\n");
    }
    //CPU memory address
    //DWORD rd_buf_phy_addr_h = (bk_ptr->rp_rd_buffer_bus_addr >> 32) & 0xffffffff;
   // DWORD rd_buf_phy_addr_l = bk_ptr->rp_rd_buffer_bus_addr & 0xffffffff;

    if (!fromDev) {
        status = InitDMABookkeep(hDev, &ppDMA, &ppDMA_wr, &ppDMA_rd_buf, &ppDMA_wr_buf);
        if (status != WD_STATUS_SUCCESS) {
            printf("Fail to initiate DMA bookkeep.\n");
        }
        for (int k = 0; k < pattern_num; k++) {
            char pattern_name[30];
            strcpy(pattern_name, prefix);
            char num[6];
            itoa(k, num, 10);
            strcat(pattern_name, num);
            printf("pattern name is %s.\n", pattern_name);
            strcat(pattern_name, ".");
            strcat(pattern_name, format);
            printf("pattern name is %s.\n", pattern_name);
            FILE *fp = OpenPattern(pattern_name, "r");
            // The maximum number of DMA DT is 128, starting from 0 to 127
            int pattern_size = CalcualtePatternSize(fp);
            printf("pattern_size is %d.\n", pattern_size);
            CloseFile(fp);
            // One page corresponds to one DMA Descriptor Table
            int page_num = (pattern_size % PAGE_SIZE == 0) ? (pattern_size / PAGE_SIZE) : (pattern_size / PAGE_SIZE + 1);
            if (h_pix == 1080) {
                int int_num_per_line = h_pix / 32;  // The number of Int in every line
                int hex_num_per_line = h_pix / 4;

                for (int line_index = 0; line_index < v_line; line_index++) {
                    for (int j = 0; j < int_num_per_line; j++) {
                        //(int_num_per_line + 2) means there are 2 '\n' at every end of line
                        dmd_pattern_ptr->data[line_index * int_num_per_line + j] = ReadDMDPattern(pattern_name, (line_index * (hex_num_per_line + 2) + j * 8), 8);
                    }
                }
            }
            else {
                int int_num_per_line = h_pix / 32;  // The number of Int in every line
                int hex_num_per_line = h_pix / 4;
                PatternFill(pattern_name, h_pix, v_line);
            }

            //  Construct the descriptor table
            if (ppDMA_rd_buf->dwPages > 128) {
                printf("Oversize of DMA Descriptor number.\n");
                break;
            }
            WDC_ReadAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR, &last_id);
            printf("Before DT, last_id is %x.\n", last_id);
            if (last_id == 0xff)
                last_id = 0;
            else
                last_id = last_id + 1;
           // for (int i = 0; i < ppDMA_rd_buf->dwPages; i++) {
              //  (bk_ptr->rp_rd_buffer_bus_addr) = ppDMA_rd_buf->Page[i].pPhysicalAddr;  // Buffer's physical address
                DWORD rd_buf_phy_addr_h = (bk_ptr->rp_rd_buffer_bus_addr >> 32) & 0xffffffff;
                DWORD rd_buf_phy_addr_l = bk_ptr->rp_rd_buffer_bus_addr & 0xffffffff;

              //  if (i == 0) {
                if (k == 0) {  // The first 256-bit data in the first pattern stores in the onchip_mem_base_addr
                    onchip_mem_start_addr = onchip_mem_base_addr;
                   // pre_page_size = 0;
                    current_page_size = ppDMA_rd_buf->Page[0].dwBytes;
                    printf("current_page_size is %d.\n", current_page_size);
                    printf("onchip_mem_start_addr is %x.\n", onchip_mem_start_addr);
                }
                else {  
                    
                   // int addr_inc = ((pre_page_size % 32 == 0) ? (pre_page_size / 32) : (pre_page_size / 32 + 1));
                    onchip_mem_start_addr = onchip_mem_start_addr + pre_page_size;
                    printf("onchip_mem_start_addr is %x.\n", onchip_mem_start_addr);
                    current_page_size = ppDMA_rd_buf->Page[0].dwBytes;
                }
                pre_page_size = current_page_size;
           
                DWORD onchip_mem_addr_h = (onchip_mem_start_addr >> 32) & 0xffffffff;
                DWORD onchip_mem_addr_l = onchip_mem_start_addr & 0xffffffff;
                DWORD dma_dword = (current_page_size) / 4; // Dword number
                SetDescTable(&(bk_ptr->lite_table_rd_cpu_virt_addr.descriptors[last_id]), rd_buf_phy_addr_h, rd_buf_phy_addr_l, onchip_mem_addr_h, onchip_mem_addr_l, dma_dword, last_id);

            WDC_ReadAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR, &last_id);
            printf("After DT, last_id is %x.\n", last_id);
            if (last_id == 0xff) {
                //ConfigDMADescController(hDev, bk_ptr->lite_table_rd_bus_addr, fromDev);
                WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTRL_HIGH_DEST_ADDR, RD_CTRL_BUF_BASE_LOW);
                WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTLR_LOW_DEST_ADDR, RD_CTRL_BUF_BASE_LOW);
                last_id = 127;
            }
            last_id = last_id + ppDMA_rd_buf->dwPages;

            if (last_id > 127) {
                last_id = last_id - 128;
                if ((ppDMA_rd_buf->dwPages > 1) && (last_id != 127)) write_127 = 1;
            }
            ConfigDMADescController(hDev, bk_ptr->lite_table_rd_bus_addr, 0, fromDev);
            if (write_127) {
                WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR, 127);
            }
            WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR, last_id);

           // WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR, last_id);
            printf("Last_id is %d.\n", last_id);
            timeout = TIMEOUT;
            while (1) {
                if (bk_ptr->lite_table_rd_cpu_virt_addr.header.flags[last_id]) {
                    printf("DMA read operation is successful.\n");
                    break;
                }
                timeout--;
                if (timeout == 0) {
                    printf("DMA read operation is timeout.\n");
                    break;
                }
            }
            //WDC_DMASyncIo(ppDMA_wr_buf);
           // WDC_DMABufUnlock(ppDMA_wr_buf);
        }
        WDC_DMASyncIo(ppDMA);
        WDC_DMABufUnlock(ppDMA);
        WDC_DMASyncIo(ppDMA_rd_buf);
        WDC_DMABufUnlock(ppDMA_rd_buf);
        close_pci(hDev);
        return TRUE;
    }
}