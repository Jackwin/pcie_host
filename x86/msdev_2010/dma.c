#include "dma.h"
#include "pattern.h"
#include "pci_driver_lib.h"
#include "windrvr_int_thread.h"
#include "altera_lib.h"
#include "string.h"
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
        //WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTRL_HIGH_DEST_ADDR, RD_CTRL_BUF_BASE_LOW);
        //WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTLR_LOW_DEST_ADDR, RD_CTRL_BUF_BASE_LOW);
       // WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTRL_HIGH_DEST_ADDR, RD_CTRL_BUF_BASE_LOW);
       // WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTLR_LOW_DEST_ADDR, RD_CTRL_BUF_BASE_LOW);
       // WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_TABLE_SIZE, table_size);
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
    /*
    status = WDC_DMASyncCpu(*ppDma);
   if (WD_STATUS_SUCCESS != status) {
        printf("Fail to CPUSync ppDMA.\n");
    }
    */

    // status = WDC_DMASGBufLock(hDev, &(bk_ptr->lite_table_wr_cpu_virt_addr), DMA_TO_DEVICE, sizeof(struct altera_pcie_dma_bookkeep), ppDma_wr);
    // Because of the size of structure altera_pcie_dma_bookkeep is over than 4KB page size, it needs to use ContigBufLock to apply space for altera_pcie_dma_bookkeep
    // DWORD status = WDC_DMAContigBufLock(hDev, bk_ptr, DMA_TO_DEVICE, sizeof(struct altera_pcie_dma_bookkeep), ppDma);
    // if (status != WD_STATUS_SUCCESS) {
    //     printf("Fail to initiate DMAContigBuf for ppDma_wr.\n");
    //  }
    //printf("DMA page number in wr is %d.\n", (*ppDma_wr)->dwPages);

    // status = WDC_DMASyncCpu(*ppDma_wr);
 //   if (WD_STATUS_SUCCESS != status) {
  //      printf("Fail to CPUSync ppDMA_wr.\n");
  //  }


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

/*
    //Apply for continuous memory space for pattern data
    status = WDC_DMAContigBufLock(hDev, &ppBuf, DMA_TO_DEVICE, sizeof(DMD_PATTERN), ppDMA_rd_buf);
    //ppBuf = (struct altera_pcie_dma_bookkeep *) malloc(sizeof(struct altera_pcie_dma_bookkeep));
    dmd_pattern_ptr = (DMD_PATTERN *)ppBuf;
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to initiate DMAContigBuf for dmd_pattern.\n");
        printf("status is %x.\n", status);
    }
    printf("dmd_pattern page number is %d.\n", (*ppDMA_rd_buf)->dwPages);
    (bk_ptr->rp_rd_buffer_virt_addr) = (*ppDMA_rd_buf)->pUserAddr;
    (bk_ptr->rp_rd_buffer_bus_addr) = (*ppDMA_rd_buf)->Page[0].pPhysicalAddr;
    */

/*
    status = WDC_DMASyncCpu(*ppDMA_rd_buf);
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to CPUSync ppDMA.\n");
    }
    */
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
    //(bk_ptr->rp_wr_buffer_virt_addr) = (*ppDMA_wr_buf)->pUserAddr;
   // (bk_ptr->rp_wr_buffer_bus_addr) = (*ppDMA_wr_buf)->Page[0].pPhysicalAddr;
    return status;

}

/*
Func name: FPGA_read()
Func: FPGA read data from CPU
param:  vendor_id
        device_id
        source_data_ptr: the data to be sent in the CPU memory
        data_size: the size of the to-be-sent data in byte
        fpga_ddr3_addr_offset: the offset address. The starting addres in FPGA ddr3 is 0
        fpga_onchip_addr_offset:
        target: 0 stands for copying data to FPGA DDR3
                1 stands for copying data to FPGA onchip memory
Return:
*/

int FPGA_read(DWORD vendor_id, DWORD device_id, int *source_data_ptr, int data_size, int fpga_ddr3_addr_offset, int fpga_onchip_addr_offset, int target) {
    DWORD last_id = 0, write_127 = 0;
    DWORD timeout;
    WD_DMA *pDMA = NULL, *ppDMA_wr = NULL, *ppDMA_wr_buf = NULL;
    PVOID ppBuf;
    PVOID pBuf = NULL;
    WD_DMA *pDMA_rd_buf_array[128];
    PVOID ppBuf_array[128];
    static DWORD status;
    int flag = 0;
    int *pdata;

    // Total descriptor number
    int total_descriptor_num = (data_size % DMA_SIZE_PER_DESCRIPTOR == 0) ? (data_size / DMA_SIZE_PER_DESCRIPTOR) : (data_size / DMA_SIZE_PER_DESCRIPTOR + 1);
    int last_descriptor_dma_size = data_size - (total_descriptor_num - 1) * DMA_SIZE_PER_DESCRIPTOR;
    int dt_num = (total_descriptor_num % 128 == 0) ? (total_descriptor_num / 128) : (total_descriptor_num / 128 + 1);
    int last_dt_descriptor_num = (total_descriptor_num % 128 == 0) ? 128 : (total_descriptor_num % 128) ;

    printf("total descriptor number is %d, last_descriptor dma size is %d,.\n", total_descriptor_num, last_descriptor_dma_size);
    printf(" dt number is %d, last_dt_descriptor_num is %d.\n", dt_num, last_dt_descriptor_num);
    DMA_ADDR offchip_mem_base_addr = (DDR_MEM_BASE_ADDR_HI << 32) + DDR_MEM_BASE_ADDR_LOW + fpga_ddr3_addr_offset;
    DMA_ADDR offchip_mem_start_addr;

    DMA_ADDR onchip_mem_base_addr = (ONCHIP_MEM_BASE_ADDR_HI << 32) + ONCHIP_MEM_BASE_ADDR_LOW + 3 * 32 + fpga_onchip_addr_offset;
    DMA_ADDR onchip_mem_start_addr;
    DMA_ADDR mem_start_addr;
    DWORD current_page_size, pre_page_size;

    if (target == 1 && (onchip_mem_base_addr + data_size >= ONCHIP_MEM_SIZE)) {
        printf("Oversize the onchip memory.\n");
        return 0;
    }
    
    // Initialize PCI device
    if (!PCI_Get_WD_handle(&hDev)) {
        printf("Fail to get WD_handle.\n");
        return 0;
    }
    WD_Close(hDev);
    status = initialize_PCI(vendor_id, device_id);
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to initialize PCI.\n");
    }

    // ----------- Set up bookkeep for the descriptor table ---------------------------------------------------
    status = WDC_DMAContigBufLock(hDev, &ppBuf, DMA_TO_DEVICE, sizeof(struct altera_pcie_dma_bookkeep), &pDMA);
    bk_ptr = (struct altera_pcie_dma_bookkeep *)ppBuf;
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to initiate DMAContigBuf for pDMA.\n");
        printf("status is %x.\n", status);
        return 0;
    }
    printf("DMA page number is %d.\n", pDMA->dwPages);
    set_lite_table_header(&(bk_ptr->lite_table_rd_cpu_virt_addr.header));
    set_lite_table_header(&(bk_ptr->lite_table_wr_cpu_virt_addr.header));
    // Get the descriptor table physical address
    bk_ptr->lite_table_rd_bus_addr = pDMA->Page[0].pPhysicalAddr;

    //-------------------------Move data from CPU memory to FPGA--------------------------------------------
    for (int dt_index = 0; dt_index < dt_num; dt_index++) {
        int descriptor_num;
        if (dt_index != (dt_num - 1)) descriptor_num = 128;
        else descriptor_num = last_dt_descriptor_num;
        //Apply for continuous memory space for pattern data
        for (int i = 0; i < 128; i++) {
            DWORD rd_buf_phy_addr_h, rd_buf_phy_addr_l;
            // Actual descriptors
            if (i < descriptor_num) {
                printf("%d ...........\n", i);
                status = WDC_DMAContigBufLock(hDev, &ppBuf_array[i], DMA_TO_DEVICE, DMA_SIZE_PER_DESCRIPTOR, &pDMA_rd_buf_array[i]);
                //ppBuf = (struct altera_pcie_dma_bookkeep *) malloc(sizeof(struct altera_pcie_dma_bookkeep));
                pdata = (int *)ppBuf_array[i];
                if (status != WD_STATUS_SUCCESS) {
                    printf("Fail to initiate DMAContigBuf for dmd_pattern.\n");
                    printf("status is %x.\n", status);
                }
                //Copy data
                if (i == 77)
                    printf("hi.\n");
                // Change to int address
                int source_data_addr = source_data_ptr + (dt_index * (128 * DMA_SIZE_PER_DESCRIPTOR) + i * DMA_SIZE_PER_DESCRIPTOR) / 4;
                printf("source data address is %x.\n", source_data_addr);
                memcpy(pdata, (int *)(source_data_addr), DMA_SIZE_PER_DESCRIPTOR);
                printf("pdata[0] is %x.\n", *pdata);

                //(bk_ptr->rp_rd_buffer_virt_addr) = pDMA_rd_buf_array[i]->pUserAddr;
                //(bk_ptr->rp_rd_buffer_bus_addr) = pDMA_rd_buf_array[i]->Page[0].pPhysicalAddr;
               // printf("pDMA_rd_buf_array[%d] physical address is 0x %x.\n",i, bk_ptr->rp_rd_buffer_bus_addr);

                status = WDC_DMASyncCpu(pDMA_rd_buf_array[i]);
                if (status != WD_STATUS_SUCCESS) {
                    printf("Fail to Sync pDMA_rd_buf_array[%d].\n", i);
                }

                //  Construct the descriptor table
                if (pDMA_rd_buf_array[i]->dwPages > 1) {
                    printf("Over-page!! The default page is 1.\n");
                    break;
                }

                // CPU memory physical address
                // rd_buf_phy_addr_h = (bk_ptr->rp_rd_buffer_bus_addr >> 32) & 0xffffffff;
                 //rd_buf_phy_addr_l = bk_ptr->rp_rd_buffer_bus_addr & 0xffffffff;
                rd_buf_phy_addr_h = (pDMA_rd_buf_array[i]->Page[0].pPhysicalAddr >> 32) & 0xffffffff;
                rd_buf_phy_addr_l = (pDMA_rd_buf_array[i]->Page[0].pPhysicalAddr) & 0xffffffff;

                if (i == 0 && dt_index == 0) {
                    if (target == 0)
                        mem_start_addr = offchip_mem_base_addr;
                    else
                        mem_start_addr = onchip_mem_base_addr;

                    current_page_size = pDMA_rd_buf_array[0]->Page[0].dwBytes;
                    printf("current_page_size is %d.\n", current_page_size);
                    printf("mem_start_addr is 0x%x.\n", mem_start_addr);
                }
                else {
                    // int addr_inc = ((pre_page_size % 32 == 0) ? (pre_page_size / 32) : (pre_page_size / 32 + 1));
                    mem_start_addr = mem_start_addr + pre_page_size;
                    printf("mem_start_addr is %x.\n", mem_start_addr);
                    current_page_size = pDMA_rd_buf_array[i]->Page[0].dwBytes;
                }
                pre_page_size = current_page_size;
            }

            DWORD mem_addr_h = (mem_start_addr >> 32) & 0xffffffff;
            DWORD mem_addr_l = mem_start_addr & 0xffffffff;
            DWORD dma_dword = (current_page_size) / 4; // Dword number
            SetDescTable(&(bk_ptr->lite_table_rd_cpu_virt_addr.descriptors[i]), rd_buf_phy_addr_h, rd_buf_phy_addr_l, mem_addr_h, mem_addr_l, dma_dword, i);
        }
        printf("\n");
        printf("Start to DMA. ROUND %d. \n", (dt_index + 1));
        status = WDC_DMASyncCpu(pDMA);
            if (WD_STATUS_SUCCESS != status) {
            printf("Fail to CPUSync ppDMA.\n");
        }

        WDC_ReadAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR, &last_id);
        printf("Before DT, last_id is %x.\n", last_id);

        if (last_id == 0xff) {
            last_id = 127;
            ConfigDMADescController(hDev, bk_ptr->lite_table_rd_bus_addr, 127, 0);
            WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTRL_HIGH_DEST_ADDR, RD_CTRL_BUF_BASE_HI);
            WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTLR_LOW_DEST_ADDR, RD_CTRL_BUF_BASE_LOW);
        }
        else
            last_id = last_id + descriptor_num;

        if (last_id > 127) {
            last_id = last_id - 128;
            //if ((ppDMA1_rd_buf->dwPages > 1) && (last_id != 127)) write_127 = 1;
            if (descriptor_num > 1 && last_id != 127) write_127 = 1;
        }

        if (write_127) {
            WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR, 127);
        }
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR, last_id);
        printf("Last_id is %d.\n", last_id);
        timeout = TIMEOUT;

        while (1) {
            if (bk_ptr->lite_table_rd_cpu_virt_addr.header.flags[last_id]) {
                printf("DMA read operation is successful.\n");
                flag = 1;
                break;
            }
            timeout--;
            if (timeout == 0) {
                flag = 0;
                printf("DMA read operation is timeout.\n");
                break;
            }
        }

        for (int m = 0; m < descriptor_num; m++) {
            WDC_DMASyncIo(pDMA_rd_buf_array[m]);
            WDC_DMABufUnlock(pDMA_rd_buf_array[m]);
        }
        if (flag == 0) {
            WDC_DMASyncIo(pDMA);
            WDC_DMABufUnlock(pDMA);
            close_pci(hDev);
            return 0;
        }

    }

    WDC_DMASyncIo(pDMA);
    WDC_DMABufUnlock(pDMA);
    int read_data;
    for (int i = 0; i < 128; i++) {
        int addr = 0x60 + i * 0x3f480;
        WDC_ReadAddr32(hDev, ALTERA_AD_BAR4,addr, &read_data);
        printf("%d: Read_data from Address %x is %x.\n",i, addr, read_data);
    }
    close_pci(hDev);
    return TRUE;
}

  void GeneratePatternData(int pattern_num, char *prefix, char *format, int h_pix, int v_line) {
      for (int k = 0; k < pattern_num; k++) {
          char pattern_name[30];
          strcpy(pattern_name, prefix);
          char num[6];
          itoa(0, num, 10);
          strcat(pattern_name, num);
          printf("pattern name is %s.\n", pattern_name);
          strcat(pattern_name, ".");
          strcat(pattern_name, format);
          //printf("pattern name is %s.\n", pattern_name);
          FILE *fp = OpenPattern(pattern_name, "r");
          // The maximum number of DMA DT is 128, starting from 0 to 127
          int pattern_size = CalcualtePatternSize(fp);
          printf("pattern_size is %d.\n", pattern_size);
          CloseFile(fp);
          // One page corresponds to one DMA Descriptor Table
          int page_num = (pattern_size % PAGE_SIZE == 0) ? (pattern_size / PAGE_SIZE) : (pattern_size / PAGE_SIZE + 1);
          if (h_pix == 1920) {
              int int_num_per_line = h_pix / 32;  // The number of Int in every line
              int hex_num_per_line = h_pix / 4;

              for (int line_index = 0; line_index < v_line; line_index++) {
                  for (int j = 0; j < int_num_per_line; j++) {
                      //(int_num_per_line + 2) means there are 2 '\n' at every end of line
                      dmd_pattern_data[0].data[line_index * int_num_per_line + j] = ReadDMDPattern(pattern_name, (line_index * (hex_num_per_line + 2) + j * 8), 8);
                  }
              }
          }
          else {
              int int_num_per_line = h_pix / 32;  // The number of Int in every line
              int hex_num_per_line = h_pix / 4;
              PatternFill(pattern_name, h_pix, v_line);
          }
      }

      for (int m = 1; m < PATTERN_NUMBER; m++)
          for (int j = 0; j < 1920 * 1080 / 32; j++) {
              dmd_pattern_data[m].data[j] = dmd_pattern_data[0].data[j];
      }
  }

 
  int DMAOperation(DWORD vendor_id, DWORD device_id) {
      GeneratePatternData(1, "model", "txt", 1920, 1080);
      int status = FPGA_read(vendor_id, device_id, (int *)(dmd_pattern_data),sizeof(DMD_PATTERN) * PATTERN_NUMBER, 0x60, 0, 0);
      return status;
  }