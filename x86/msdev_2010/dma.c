#include "dma.h"
#include "pattern.h"
#include "pci_driver_lib.h"
#include "windrvr_int_thread.h"
#include "altera_lib.h"
#include "string.h"

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

BOOL ConfigDMADescController(WDC_DEVICE_HANDLE hDev, DMA_ADDR desc_table_start_addr, BOOL fromDev) {
    DWORD dt_phy_addr_h = (desc_table_start_addr >> 32) & 0xffffffff;
    DWORD dt_phy_addr_l = desc_table_start_addr & 0xffffffff;

    // Move data from CPU to FPGA
    if (!fromDev) {
        // Program the address of descriptor table to DMA descriptor controller
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_RC_HIGH_SRC_ADDR, dt_phy_addr_h);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_RC_LOW_SRC_ADDR, dt_phy_addr_l);
    }
    else {
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_RC_HIGH_SRC_ADDR, dt_phy_addr_h);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_RC_LOW_SRC_ADDR, dt_phy_addr_l);
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

    bk_ptr->numpages = (PAGE_SIZE >= MAX_NUM_DWORDS * 4) ? 1 : (int)((MAX_NUM_DWORDS * 4) / PAGE_SIZE);

    set_lite_table_header(&(bk_ptr->lite_table_rd_cpu_virt_addr.header));
    set_lite_table_header(&(bk_ptr->lite_table_wr_cpu_virt_addr.header));

    bk_ptr->lite_table_rd_bus_addr = (*ppDma)->Page[0].pPhysicalAddr;

    return status;

}

/*
Func name: FPGA_read()
Func: FPGA read data from CPU
param:  vendor_id
        device_id
        source_data_ptr: the data to be sent in the CPU memory
        data_size: the size of the to-be-sent data in byte
        byte_per_descriptor: the data size in one descriptor
        fpga_ddr3_addr_offset: the offset address. The starting addres in FPGA ddr3 is 0
        fpga_onchip_addr_offset:
        target: 0 stands for copying data to FPGA DDR3
                1 stands for copying data to FPGA onchip memory
Return:
*/

int FPGA_read(DWORD vendor_id, DWORD device_id, int *source_data_ptr, int data_size, int byte_per_descriptor, int fpga_ddr3_addr_offset, int fpga_onchip_addr_offset, int target) {
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
    int table_size = 0;
    // Total descriptor number
    int total_descriptor_num = (data_size % byte_per_descriptor == 0) ? (data_size / byte_per_descriptor) : (data_size / byte_per_descriptor + 1);
    int last_descriptor_dma_size = data_size - (total_descriptor_num - 1) * byte_per_descriptor;
    int dt_num = (total_descriptor_num % 128 == 0) ? (total_descriptor_num / 128) : (total_descriptor_num / 128 + 1);
    int last_dt_descriptor_num = (total_descriptor_num % 128 == 0) ? 128 : (total_descriptor_num % 128) ;

    printf("total descriptor number is %d, last_descriptor dma size is %d,.\n", total_descriptor_num, last_descriptor_dma_size);
    printf(" dt number is %d, last_dt_descriptor_num is %d.\n", dt_num, last_dt_descriptor_num);
    DMA_ADDR offchip_mem_base_addr = (DDR_MEM_BASE_ADDR_HI << 32) + DDR_MEM_BASE_ADDR_LOW + fpga_ddr3_addr_offset;
    DMA_ADDR offchip_mem_start_addr;

   // DMA_ADDR onchip_mem_base_addr = (ONCHIP_MEM_BASE_ADDR_HI << 32) + ONCHIP_MEM_BASE_ADDR_LOW + 3 * 32 + fpga_onchip_addr_offset;
    DMA_ADDR onchip_mem_base_addr = (ONCHIP_MEM_BASE_ADDR_HI << 32) + ONCHIP_MEM_BASE_ADDR_LOW + fpga_onchip_addr_offset;
    DMA_ADDR onchip_mem_start_addr;
    DMA_ADDR mem_start_addr;
    DWORD current_page_size, pre_page_size;

    if (target == 1 && (data_size > ONCHIP_MEM_SIZE)) {
        printf("Oversize the onchip memory.\n");
        return 0;
    }

    //-------------------------Move data from CPU memory to FPGA--------------------------------------------
    for (int dt_index = 0; dt_index < dt_num; dt_index++) {
        // --------------------- Initialize the bookkeep --------------------------------------------------
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

        int descriptor_num;
        if (dt_index != (dt_num - 1)) descriptor_num = 128;
        else descriptor_num = last_dt_descriptor_num;
        //Apply for continuous memory space for pattern data
        for (int i = 0; i < 128; i++) {
            DWORD rd_buf_phy_addr_h, rd_buf_phy_addr_l;
            // Actual descriptors
            if (i < descriptor_num) {
                status = WDC_DMAContigBufLock(hDev, &ppBuf_array[i], DMA_TO_DEVICE, byte_per_descriptor, &pDMA_rd_buf_array[i]);
                //ppBuf = (struct altera_pcie_dma_bookkeep *) malloc(sizeof(struct altera_pcie_dma_bookkeep));
                pdata = (int *)ppBuf_array[i];
                if (status != WD_STATUS_SUCCESS) {
                    printf("Fail to initiate DMAContigBuf for dmd_pattern.\n");
                    printf("status is %x.\n", status);
                }
                // Change to int address
               // int source_data_addr = source_data_ptr + (dt_index * (128 * DMA_SIZE_PER_DESCRIPTOR) + i * DMA_SIZE_PER_DESCRIPTOR) / 4;
                // Repeat copy the same data. Just as an example
                int source_data_addr = source_data_ptr + i * byte_per_descriptor / 4;
                //printf("source data address is %x.\n", source_data_addr);
                memcpy(pdata, (int *)(source_data_addr), byte_per_descriptor);
               // printf("pdata[1920*1080/32-1] is %x,  source_data_addr[1920*1080/32-1]is %x.\n", pdata[1920 * 1080 / 32 - 1], source_data_ptr[1920 * 1080 / 32 - 1]);
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
                rd_buf_phy_addr_h = (pDMA_rd_buf_array[i]->Page[0].pPhysicalAddr >> 32) & 0xffffffff;
                rd_buf_phy_addr_l = (pDMA_rd_buf_array[i]->Page[0].pPhysicalAddr) & 0xffffffff;

                if (i == 0 && dt_index == 0) {
                    if (target == 0)
                        mem_start_addr = offchip_mem_base_addr;
                    else
                        mem_start_addr = onchip_mem_base_addr;

                    current_page_size = pDMA_rd_buf_array[0]->Page[0].dwBytes;
                    printf("current_page_size is %d.\n", current_page_size);
                   // printf("mem_start_addr is 0x%x.\n", mem_start_addr);
                }
                else {
                    // Addr problem??
                    mem_start_addr = mem_start_addr + pre_page_size;
                  //  printf("mem_start_addr is %x.\n", mem_start_addr);
                    current_page_size = pDMA_rd_buf_array[i]->Page[0].dwBytes;
                }
                pre_page_size = current_page_size ;
            }

            DWORD mem_addr_h = (mem_start_addr >> 32) & 0xffffffff;
            DWORD mem_addr_l = mem_start_addr & 0xffffffff;
            DWORD dma_dword = (current_page_size) / 4; // Dword number

           // printf("mem_addr_h is %x, mem_addr_l is %x.\n", mem_addr_h, mem_addr_l);
            printf("dma_dword is %d.\n", dma_dword);
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
/*
        if (last_id == 0xff) {
            last_id = 127;
            ConfigDMADescController(hDev, bk_ptr->lite_table_rd_bus_addr, 127, 0);
            WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTRL_HIGH_DEST_ADDR, RD_CTRL_BUF_BASE_HI);
            WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTLR_LOW_DEST_ADDR, RD_CTRL_BUF_BASE_LOW);
        }
 */
        ConfigDMADescController(hDev, bk_ptr->lite_table_rd_bus_addr, 0);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTRL_HIGH_DEST_ADDR, RD_CTRL_BUF_BASE_HI);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTLR_LOW_DEST_ADDR, RD_CTRL_BUF_BASE_LOW);
       // last_id = last_id + descriptor_num;
        last_id = descriptor_num - 1;

        if (last_id > 127) {
            last_id = last_id - 128;
            //if ((ppDMA1_rd_buf->dwPages > 1) && (last_id != 127)) write_127 = 1;
            if (descriptor_num > 1 && last_id != 127) {
                write_127 = 1;
                table_size = last_id;
            }
        }

        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_TABLE_SIZE, last_id);
        printf("Set table size is %d.\n", (last_id ));
        if (write_127) {
            //WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_TABLE_SIZE, (last_id + descriptor_num - 1));
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
            //close_pci(hDev);
            return 0;
        }
        WDC_DMASyncIo(pDMA);
        WDC_DMABufUnlock(pDMA);

    }

    int read_data;
    int check_fail = 0;
    for (int i = 0; i < total_descriptor_num; i++) {
        int addr = fpga_ddr3_addr_offset + i * 0x3f480;
        WDC_ReadAddr32(hDev, ALTERA_AD_BAR4,addr, &read_data);
        if (read_data != 0xee234567) {
            printf("Data error at address %x.\n", addr);
            printf("%d: Read_data from DDR3 Address %x is %x.\n", i, addr, read_data);
            check_fail = 1;
        }
    }
    if (check_fail) printf("Fail to check the data.\n");
    else printf("Success to check the data.\n");

    return TRUE;
}

// ------------------------------------------------------------------------------------------------
/*
Func name: FPGA_write_to_CPU()
Func: FPGA writes data to CPU
param:
        data_size: the size of the to-be-read data in byte, which should be the multiples of 4-byte
        byte_per_descriptor: the data size in one descriptor
        fpga_ddr3_addr_offset: the offset address. The starting addres in FPGA ddr3 is 0
        fpga_onchip_addr_offset:
        target: 0 stands for copying data to FPGA DDR3
                1 stands for copying data to FPGA onchip memory
Return: Return the pointer to the read data in CPU memory
*/

int* FPGA_write_to_CPU(int data_size, int byte_per_descriptor, int fpga_ddr3_addr_offset, int fpga_onchip_addr_offset, int target) {
    DWORD last_id = 0, write_127 = 0;
    DWORD timeout;
    WD_DMA *pDMA = NULL, *ppDMA_wr = NULL, *ppDMA_wr_buf = NULL;
    PVOID ppBuf;
    PVOID pBuf = NULL;
    WD_DMA *pDMA_wr_buf;
    PVOID ppBuf_data;
    static DWORD status;
    int flag = 0;
    int *pdata;
    int table_size = 0;
    // Total descriptor number
    int total_descriptor_num = (data_size % byte_per_descriptor == 0) ? (data_size / byte_per_descriptor) : (data_size / byte_per_descriptor + 1);
    int last_descriptor_dma_size = data_size - (total_descriptor_num - 1) * byte_per_descriptor;
    int dt_num = (total_descriptor_num % 128 == 0) ? (total_descriptor_num / 128 ) : (total_descriptor_num / 128 + 1);
    int last_dt_descriptor_num = (total_descriptor_num % 128 == 0) ? 128 : (total_descriptor_num % 128) ;

    printf("total descriptor number is %d, last_descriptor dma size is %d,.\n", total_descriptor_num, last_descriptor_dma_size);
    printf(" dt number is %d, last_dt_descriptor_num is %d.\n", dt_num, last_dt_descriptor_num);

    DMA_ADDR fpga_start_addr;
    DMA_ADDR cpu_start_addr;

    if (target == 0)
        fpga_start_addr = (DDR_MEM_BASE_ADDR_HI << 32) + DDR_MEM_BASE_ADDR_LOW + fpga_ddr3_addr_offset;
    else
        fpga_start_addr = (ONCHIP_MEM_BASE_ADDR_HI << 32) + ONCHIP_MEM_BASE_ADDR_LOW + fpga_onchip_addr_offset;
    DWORD current_page_size, pre_page_size;

    if (target == 1 && (data_size > ONCHIP_MEM_SIZE)) {
        printf("Oversize the onchip memory.\n");
        return 0;
    }
    // -------------------- Apply for the physical memory space to store the to-read data -----------------------------
    status = WDC_DMAContigBufLock(hDev, &ppBuf_data, DMA_FROM_DEVICE, data_size, &pDMA_wr_buf);
    pdata = (int *)ppBuf_data;
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to initiate DMAContigBuf for dmd_pattern.\n");
        printf("status is %x.\n", status);
    }

    status = WDC_DMASyncCpu(pDMA_wr_buf);
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to Sync pDMA_wr_buf.\n");
    }
    // CPU physical memory address
    cpu_start_addr = pDMA_wr_buf->Page[0].pPhysicalAddr;

    for (int dt_index = 0; dt_index < dt_num; dt_index++) {
        // --------------------- Initialize the bookkeep --------------------------------------------------
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
        bk_ptr->lite_table_wr_bus_addr = pDMA->Page[0].pPhysicalAddr + sizeof(struct lite_dma_desc_table);

        int descriptor_num;
        if (dt_index != (dt_num - 1)) descriptor_num = 128;
        else descriptor_num = last_dt_descriptor_num;

        for (int i = 0; i < descriptor_num; i++) {
            DWORD cpu_start_addr_h, cpu_start_addr_l;
            // Actual descriptors
           // if (i < descriptor_num) {
            fpga_start_addr = fpga_start_addr + i * byte_per_descriptor / 4;

            // Get the CPU physical
            // The fisrt and last descriptor
            if(dt_index == 0 && i == 0 && dt_num == 1 && last_dt_descriptor_num == 1) {
                current_page_size = data_size;
            }
            // The first descriptor
            else if (dt_index == 0 && i == 0) {
                current_page_size = byte_per_descriptor;
            }
            // The last descriptor
            else if (dt_index == (dt_num - 1) && (i == (last_dt_descriptor_num - 1))) {
                current_page_size = last_descriptor_dma_size;
                cpu_start_addr = cpu_start_addr + pre_page_size;
                fpga_start_addr = fpga_start_addr + pre_page_size;
            }
            else {
                current_page_size = byte_per_descriptor;
                cpu_start_addr = cpu_start_addr + pre_page_size;
                fpga_start_addr = fpga_start_addr + pre_page_size;
            }

            pre_page_size = current_page_size;
            //}

            cpu_start_addr_h = (cpu_start_addr >> 32) & 0xffffffff;
            cpu_start_addr_l = cpu_start_addr & 0xffffffff;

            DWORD fpga_start_addr_h = (fpga_start_addr >> 32) & 0xffffffff;
            DWORD fpga_start_addr_l = fpga_start_addr & 0xffffffff;
            DWORD dma_dword = (current_page_size) / 4; // Dword number
            SetDescTable(&(bk_ptr->lite_table_wr_cpu_virt_addr.descriptors[i]), fpga_start_addr_h, fpga_start_addr_l, cpu_start_addr_h, cpu_start_addr_l, dma_dword, i);
        }

        printf("\n Start to DMA. ROUND %d. \n", (dt_index + 1));
        status = WDC_DMASyncCpu(pDMA);
            if (WD_STATUS_SUCCESS != status) {
            printf("Fail to CPUSync ppDMA.\n");
        }
        WDC_ReadAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_LAST_PTR, &last_id);
        printf("Before DT, the last write descriptor id is %x.\n", last_id);

        ConfigDMADescController(hDev, bk_ptr->lite_table_wr_bus_addr, 1);
        //Configure the DT conttroller FIFO address
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_CTRL_HIGH_DEST_ADDR, WR_CTRL_BUF_BASE_HI);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_CTLR_LOW_DEST_ADDR, WR_CTRL_BUF_BASE_LOW);
        last_id = descriptor_num - 1;
        // Keep last_id under 128
        if (last_id > 127) {
            last_id = last_id - 128;
            if (descriptor_num > 1 && last_id != 127) {
                write_127 = 1;
            }
        }

        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_TABLE_SIZE, last_id);
        if (write_127) {
            WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_LAST_PTR, 127);
        }

        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_LAST_PTR, last_id);
        printf("Last_id is %d.\n", last_id);
        timeout = TIMEOUT;

        while (1) {
            if (bk_ptr->lite_table_wr_cpu_virt_addr.header.flags[last_id]) {
                printf("DMA write operation is successful.\n");
                flag = 1;
                break;
            }
            timeout--;
            if (timeout == 0) {
                flag = 0;
                printf("DMA write operation is timeout.\n");
                break;
            }
        }

        WDC_DMASyncIo(pDMA);
        WDC_DMABufUnlock(pDMA);
    }

    int *p;
    if ((p = (char *)malloc(data_size)) == NULL)  {
        printf("malloc error\n");
        return p;
    }

    memset(p, 0, data_size);
    memcpy(p, (int *)(pdata), data_size);

    WDC_DMASyncIo(pDMA_wr_buf);
    WDC_DMABufUnlock(pDMA_wr_buf);

    return p;

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

  for (int m = 1; m < PATTERN_NUMBER; m++) {
      for (int j = 0; j < 1920 * 1080 / 32; j++) {
          dmd_pattern_data[m].data[j] = dmd_pattern_data[0].data[j];
      }
      //printf("dmd_pattern_data[%d].data[1920 * 1080 / 32 - 1] is %x.\n", m, dmd_pattern_data[m].data[1920 * 1080 / 32 - 1]);
  }
}


int DMAOperation(DWORD vendor_id, DWORD device_id) {
    GeneratePatternData(1, "model", "txt", 1920, 1080);
    int status;
    int read_data;
    int to_send_frame_num = 3;
  // status = FPGA_read(vendor_id, device_id, (int *)(dmd_pattern_data), sizeof(DMD_PATTERN) * to_send_frame_num, DMA_SIZE_PER_DESCRIPTOR ,0, 0, 0);

   // status = FPGA_read(vendor_id, device_id, (int *)(dmd_pattern_data), sizeof(DMD_PATTERN) * to_send_frame_num, DMA_SIZE_PER_DESCRIPTOR, 0, 0x20, 1);

    WPS_REG wps_register;
    wps_register.rsv_reg = 0xffffeeee;
    wps_register.capture_pulse_cycle_reg = 10;
    //wps_register.start_addr_reg = ONCHIP_MEM_BASE_ADDR_LOW + 0x20;
    wps_register.start_addr_reg = 0x00;
    wps_register.to_send_total_byte_reg = to_send_frame_num * 1920 * 1080 / 8;
    wps_register.pattern_reg = (1920 << 16) | 1080;
    wps_register.one_frame_byte_reg = 1920 * 1080 / 8;
    wps_register.to_send_frame_num_reg = to_send_frame_num;
    //wps_register.start_play_reg = (1 << 31 | 1 << 30);
    wps_register.start_play_reg = (1 << 31);

  //  status = FPGA_read(vendor_id, device_id, (int *)(&wps_register), sizeof(WPS_REG), sizeof(WPS_REG), 0, 0, 1);
    /*
    for (int i = 0; i < 8; i++) {
        WDC_ReadAddr32(hDev, ALTERA_AD_BAR4, (ONCHIP_MEM_BASE_ADDR_LOW + i * 4 ), &read_data);
      printf("Read_data from onchip memory Address %x is %x.\n", (ONCHIP_MEM_BASE_ADDR_LOW + i * 4), read_data);
    }
    */
    // FPGA writes to CPU
    int *rd_data_ptr;
    rd_data_ptr = FPGA_write_to_CPU(DMA_SIZE_PER_DESCRIPTOR, DMA_SIZE_PER_DESCRIPTOR, 0, 0, 0);
    for (int k = 0; k < 16; k++)
        printf("data is %x.\n", *(rd_data_ptr + k));

    free(rd_data_ptr);
    rd_data_ptr = NULL;
    return status;
}