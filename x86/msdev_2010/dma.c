#include "dma.h"
#include "pattern.h"
#include "pci_driver_lib.h"
#include "windrvr_int_thread.h"
#include "altera_lib.h"
#include "string.h"
#include "time.h"

WD_DMA *pDMA_memory_space;

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

int *ApplyMemorySpace(int byte_size, DWORD direction) {
    static  DWORD status;
    PVOID ppBuf;
    int *pdata;

    status = WDC_DMAContigBufLock(hDev, &ppBuf, DMA_TO_DEVICE | DMA_ALLOW_CACHE | DMA_ALLOW_64BIT_ADDRESS, byte_size, &pDMA_memory_space);
   // status = WDC_DMASGBufLock(hDev, source_data_ptr, DMA_TO_DEVICE, byte_size, &pDMA_memory_space);
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to apply contiguous memory space.\n");
        printf("status is %x.\n", status);
    }
    status = WDC_DMASyncCpu(pDMA_memory_space);
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to Sync pDMA for applying memory space.\n");
    }
    pdata = (int *)ppBuf;

    return pdata;
}

int DMAToOnchipMem(DMA_ADDR cpu_memory_start_addr, int byte_size, int fpga_onchip_addr_offset) {
    static DWORD status;
    DWORD timeout;
    DWORD last_id = 0;
    PVOID ppBuf;
    WD_DMA *pDMA;
    int page_num;
    status = WDC_DMAContigBufLock(hDev, &ppBuf, DMA_TO_DEVICE, sizeof(struct altera_pcie_dma_bookkeep), &pDMA);
    bk_ptr = (struct altera_pcie_dma_bookkeep *)ppBuf;
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to initiate DMAContigBuf for pDMA.\n");
        printf("status is %x.\n", status);
        return 0;
    }
    //  printf("DMA page number is %d.\n", pDMA->dwPages);
    set_lite_table_header(&(bk_ptr->lite_table_rd_cpu_virt_addr.header));
    set_lite_table_header(&(bk_ptr->lite_table_wr_cpu_virt_addr.header));
    // Get the descriptor table physical address
    bk_ptr->lite_table_rd_bus_addr = pDMA->Page[0].pPhysicalAddr;

    DWORD dma_dword = (byte_size) / 4; // Dword number
    DMA_ADDR onchip_mem_base_addr = (ONCHIP_MEM_BASE_ADDR_HI << 32) + ONCHIP_MEM_BASE_ADDR_LOW + fpga_onchip_addr_offset;

    DWORD cpu_mem_addr_h = (cpu_memory_start_addr >> 32) & 0xffffffff;
    DWORD cpu_mem_addr_l = cpu_memory_start_addr & 0xffffffff;
    DWORD onchip_mem_addr_h = (onchip_mem_base_addr >> 32) & 0xffffffff;
    DWORD onchip_mem_addr_l = onchip_mem_base_addr & 0xffffffff;
    SetDescTable(&(bk_ptr->lite_table_rd_cpu_virt_addr.descriptors[0]), cpu_mem_addr_h, cpu_mem_addr_l, onchip_mem_addr_h, onchip_mem_addr_l, dma_dword, 0);
    status = WDC_DMASyncCpu(pDMA);
    if (WD_STATUS_SUCCESS != status) {
        printf("Fail to CPUSync pDMA.\n");
    }
    ConfigDMADescController(hDev, bk_ptr->lite_table_rd_bus_addr, 0);
    WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTRL_HIGH_DEST_ADDR, RD_CTRL_BUF_BASE_HI);
    WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTLR_LOW_DEST_ADDR, RD_CTRL_BUF_BASE_LOW);

    WDC_ReadAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR, &last_id);
    printf("Before DT, last_id is %x.\n", last_id);

    last_id = 0;
    WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_TABLE_SIZE, last_id);
    WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR, last_id);

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
    WDC_DMASyncIo(pDMA);
    WDC_DMABufUnlock(pDMA);
    return 1;

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
    int fisrt_dt_descriptor; // The first descriptor in DT
    int last_dt_descriptor;


    // Total descriptor number
    int total_descriptor_num = (data_size % byte_per_descriptor == 0) ? (data_size / byte_per_descriptor) : (data_size / byte_per_descriptor + 1);
  //  int last_descriptor_dma_size = data_size - (total_descriptor_num - 1) * byte_per_descriptor;

    if (total_descriptor_num > 128) {
        printf(" \033[40;31m The descriptor number is over the maximum 128.\033[0m \n"); // black back-groud and red characters
        exit(0);
    }
    WDC_ReadAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR, &last_id);
    if (last_id == 0xff || last_id == 127)
            last_id = - 1;
    if (last_id + total_descriptor_num > 127) {
        fisrt_dt_descriptor = last_id + 1;
        last_id = last_id + total_descriptor_num - 128;
        write_127 = 1;
    }
    else {
        write_127 = 0;
        fisrt_dt_descriptor = last_id + 1;
        last_id = last_id + total_descriptor_num;
    }
    last_dt_descriptor = last_id;

    printf(" The total descriptor number is %d, the first descriptor is %d, the last_descriptor is %d.\n", total_descriptor_num, fisrt_dt_descriptor, last_dt_descriptor);

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

    //-------------------------Move data from CPU memory to FPGA--------------------------------------------
    for (int k = 0; k < total_descriptor_num; k++) {
        int dt_index = k + fisrt_dt_descriptor;
        if (dt_index > 127) dt_index = dt_index - 128;

        //---------------------------------------------------------------------------------------------------
        DWORD rd_buf_phy_addr_h, rd_buf_phy_addr_l;
        status = WDC_DMAContigBufLock(hDev, &ppBuf_array[dt_index], DMA_TO_DEVICE, byte_per_descriptor, &pDMA_rd_buf_array[dt_index]);

        //ppBuf = (struct altera_pcie_dma_bookkeep *) malloc(sizeof(struct altera_pcie_dma_bookkeep));
        pdata = (int *)ppBuf_array[dt_index];
        if (status != WD_STATUS_SUCCESS) {
            printf("Fail to initiate DMAContigBuf for dmd_pattern.\n");
            printf("status is %x.\n", status);
        }

        int source_data_addr = source_data_ptr + k * byte_per_descriptor / 4;

        memcpy(pdata, (int *)(source_data_addr), byte_per_descriptor);
        status = WDC_DMASyncCpu(pDMA_rd_buf_array[dt_index]);
        if (status != WD_STATUS_SUCCESS) {
            printf("Fail to Sync pDMA_rd_buf_array[%d].\n", dt_index);
        }
        //  Construct the descriptor table
        if (pDMA_rd_buf_array[dt_index]->dwPages > 1) {
            printf("Over-page!! The default page is 1.\n");
         //   break;
        }

        // CPU memory physical address
        rd_buf_phy_addr_h = (pDMA_rd_buf_array[dt_index]->Page[0].pPhysicalAddr >> 32) & 0xffffffff;
        rd_buf_phy_addr_l = (pDMA_rd_buf_array[dt_index]->Page[0].pPhysicalAddr) & 0xffffffff;


        if (k == 0) {
            if (target == 0)
                mem_start_addr = offchip_mem_base_addr;
            else
                mem_start_addr = onchip_mem_base_addr;
            current_page_size = pDMA_rd_buf_array[dt_index]->Page[0].dwBytes;
        }
        else {
            // Addr problem??
            mem_start_addr = mem_start_addr + pre_page_size;
          //  printf("mem_start_addr is %x.\n", mem_start_addr);
            current_page_size = pDMA_rd_buf_array[dt_index]->Page[0].dwBytes;
        }
        pre_page_size = current_page_size ;


        DWORD mem_addr_h = (mem_start_addr >> 32) & 0xffffffff;
        DWORD mem_addr_l = mem_start_addr & 0xffffffff;
        DWORD dma_dword = (current_page_size) / 4; // Dword number
        printf("FPGA addrh is %x, and addrl is %x. CPU addrh is %x, CPU addrl is %x.\n", mem_addr_h, mem_addr_l, rd_buf_phy_addr_h, rd_buf_phy_addr_l);
        SetDescTable(&(bk_ptr->lite_table_rd_cpu_virt_addr.descriptors[dt_index]), rd_buf_phy_addr_h, rd_buf_phy_addr_l, mem_addr_h, mem_addr_l, dma_dword, dt_index);
        }

        status = WDC_DMASyncCpu(pDMA);
            if (WD_STATUS_SUCCESS != status) {
            printf("Fail to CPUSync ppDMA.\n");
        }

        WDC_ReadAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR, &last_id);

        printf("The original last_id is %d.\n", last_id);

        ConfigDMADescController(hDev, bk_ptr->lite_table_rd_bus_addr, 0);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTRL_HIGH_DEST_ADDR, RD_CTRL_BUF_BASE_HI);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTLR_LOW_DEST_ADDR, RD_CTRL_BUF_BASE_LOW);

        table_size = last_id;

      //  printf("Table size is %d.\n", table_size );
        if (write_127) {
            WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_TABLE_SIZE, 127);
            WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR, 127);
        }
        else {
             WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_TABLE_SIZE, table_size);
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
            int dt_index = k + fisrt_dt_descriptor;
            if (dt_index > 127) dt_index = dt_index - 128;

            WDC_DMASyncIo(pDMA_rd_buf_array[dt_index]);
            WDC_DMABufUnlock(pDMA_rd_buf_array[dt_index]);
        }
        if (flag == 0) {
            WDC_DMASyncIo(pDMA);
            WDC_DMABufUnlock(pDMA);
            //close_pci(hDev);
            return 0;
        }
        WDC_DMASyncIo(pDMA);
        WDC_DMABufUnlock(pDMA);

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
      strcat(pattern_name, ".");
      strcat(pattern_name, format);
      FILE *fp = OpenPattern(pattern_name, "r");
      // The maximum number of DMA DT is 128, starting from 0 to 127
      int pattern_size = CalcualtePatternSize(fp);
      printf("pattern ID is %d, pattern name is %s, pattern_size is %d.\n", k, pattern_name, pattern_size);
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

void DMAReadIntHandler(WDC_DEVICE_HANDLE hDev_0, PCI_DRIVER_INT_RESULT *pIntResult) {
    printf("Interrupt from DMAReadIntHandler.\n");
    DWORD rd_data;
    WDC_ReadAddr32(hDev, ALTERA_AD_BAR2, USR_INTR_ADDR, &rd_data);
    if (rd_data == 1) {
        printf("User interrrupt.\n");
        //clear the interrupt register
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR2, USR_INTR_ADDR, 0);
    }

}

// Select the section on FPGA DDR3, and the setion ID ranges from 0 to 15.
BOOL SelectSection(int sectionID) {
    if (sectionID > 15) {
        printf("Section ID is beyongd 15.\n");
        return FALSE;
    }
    else WDC_WriteAddr32(hDev, ALTERA_AD_BAR2, SEC_CTRL_ADDR, sectionID * SECTION_SIZE);
    DWORD rd_data;
    WDC_ReadAddr32(hDev, ALTERA_AD_BAR2, SEC_CTRL_ADDR, &rd_data);
    if (rd_data == sectionID * SECTION_SIZE) {
        printf("Choose section %d successfully.\n", sectionID);
        return TRUE;
    }
    else {
        printf("Fail to choose section %d.\n", sectionID);
        return false;
    }
}


BOOL WriteReg(DWORD addr, DWORD wr_data) {
    WDC_WriteAddr32(hDev, ALTERA_AD_BAR4, addr, wr_data);
    DWORD rd_data;
    WDC_ReadAddr32(hDev, ALTERA_AD_BAR4, addr, &rd_data);
    if (wr_data != rd_data) {
        printf("Fail to write data %x to addr %x\n", wr_data, addr);
        return FALSE;
    }
    else
        return TRUE;
}

DWORD ReadReg(DWORD addr) {
    DWORD rd_data;
    WDC_ReadAddr32(hDev, ALTERA_AD_BAR4, addr, &rd_data);
    return rd_data;
}

// configure WPS controller register
BOOL CfgWPSReg(WPS_REG wps_register) {
    BOOL status;
    status = WriteReg(RSV_ADDR, wps_register.rsv_reg);
    if (!status) return FALSE;

     status = WriteReg(TO_SEND_FRAME_NUM_REG_ADDR, wps_register.to_send_frame_num_reg);
     if (!status) return FALSE;

     status = WriteReg(ONE_FRAME_BYTE_REG_ADDR, wps_register.one_frame_byte_reg);
     if (!status) return FALSE;

     status = WriteReg(PATTERN_REG_ADDR, wps_register.pattern_reg);
     if (!status) return FALSE;

     status = WriteReg(TO_SNED_TOTAL_BYTE_REG_ADDR, wps_register.to_send_total_byte_reg);
     if (!status) return FALSE;

     status = WriteReg(START_ADDR_REG_ADDR, wps_register.start_addr_reg);
     if (!status) return FALSE;

     status = WriteReg(CAPTURE_PULSE_CYCLE_REG_ADDR, wps_register.capture_pulse_cycle_reg);
     if (!status) return FALSE;

     status = WriteReg(START_PLAY_REG_ADDR, wps_register.start_play_reg);
     if (!status) return FALSE;

     return TRUE;
}


int DMAOperation(DWORD vendor_id, DWORD device_id) {
    int status;
    int to_send_frame_num = 1;
    GeneratePatternData(to_send_frame_num, "model", "txt", 1920, 1080);

    clock_t start, finish;
    double  duration;

    // Every FPGA DDR3 section can store 256 1920x1080 patterns(frames). Here to get the section number and the left patterns/frames
    int sec_num = to_send_frame_num % 256 == 0 ? to_send_frame_num / 256 : to_send_frame_num / 256 + 1;
    int to_send_frame_num_left =  to_send_frame_num % 256;

    // By default, the section starts from section 0, corresponding to FPGA DDR3 address 0x0. Every section size is 0x4000000 bytes
    // The start section can be changed as your wish, but it's better to keep the offet address in the section at 0x00, otherwise it would involves with complicated
    // control logics in FPGA
    int start_sec_id = 0;
    for (int sec_id = 0; sec_id < sec_num; sec_id++) {
        DWORD addr_offset = 0;
        status =  SelectSection(start_sec_id + sec_id);
        if (!status){
            printf("Fail to select section %d.\n", sec_id);
            return;
        }
        if (sec_id == (sec_num - 1)) {
            if (to_send_frame_num_left == 0) {
                status = FPGA_read(vendor_id, device_id, (int *)(dmd_pattern_data), sizeof(DMD_PATTERN) * 128, DMA_SIZE_PER_DESCRIPTOR, 0x0, 0x0, 0);
                addr_offset = 128 * wps_register.one_frame_byte_reg;
                status = FPGA_read(vendor_id, device_id, (int *)(dmd_pattern_data), sizeof(DMD_PATTERN) * 128, DMA_SIZE_PER_DESCRIPTOR, addr_offset, 0x0, 0);
            }
            else if (to_send_frame_num_left > 128) {
                status = FPGA_read(vendor_id, device_id, (int *)(dmd_pattern_data), sizeof(DMD_PATTERN) * 128, DMA_SIZE_PER_DESCRIPTOR, 0x0, 0x0, 0);
                addr_offset = (to_send_frame_num_left - 128) * wps_register.one_frame_byte_reg;
                status = FPGA_read(vendor_id, device_id, (int *)(dmd_pattern_data), sizeof(DMD_PATTERN) * (to_send_frame_num_left - 128), DMA_SIZE_PER_DESCRIPTOR, addr_offset, 0x0, 0);
            }
            else
                status = FPGA_read(vendor_id, device_id, (int *)(dmd_pattern_data), sizeof(DMD_PATTERN) * to_send_frame_num_left, DMA_SIZE_PER_DESCRIPTOR, 0x0, 0x0, 0);
        }
        else {
            status = FPGA_read(vendor_id, device_id, (int *)(dmd_pattern_data), sizeof(DMD_PATTERN) * 128, DMA_SIZE_PER_DESCRIPTOR, 0x0, 0x0, 0);
            addr_offset = 128 * wps_register.one_frame_byte_reg;
            status = FPGA_read(vendor_id, device_id, (int *)(dmd_pattern_data), sizeof(DMD_PATTERN) * 128, DMA_SIZE_PER_DESCRIPTOR, addr_offset, 0x0, 0);
        }
    }

    WPS_REG wps_register;
    wps_register.rsv_reg = 0xffffeeee;
    wps_register.capture_pulse_cycle_reg = 100;
    //wps_register.start_addr_reg = ONCHIP_MEM_BASE_ADDR_LOW + 0x20;
    wps_register.start_addr_reg = DDR_MEM_BASE_ADDR_LOW + SECTION_SIZE * start_sec_id;
    wps_register.to_send_total_byte_reg = to_send_frame_num * 1920 * 1080 / 8;
    wps_register.pattern_reg = (1920 << 16) | 1080;
    wps_register.one_frame_byte_reg = 1920 * 1080 / 8;
    wps_register.to_send_frame_num_reg = to_send_frame_num;
    // wps_register.start_play_reg = (1 << 31 | 1 << 30);
    wps_register.start_play_reg = (1 << 31);
    status = CfgWPSReg(wps_register);
    if (!status) printf("Configure WPS REG fails.\n");

    //while(1);


    /*
    int *pdata;
    start = clock();
    // The applied maximum contiguous physical memory address is not clear until now. In the test, if to_send_frame_num is larger than 1000, it fails sometimes.
    pdata = ApplyMemorySpace(sizeof(DMD_PATTERN) * to_send_frame_num,DMA_TO_DEVICE);

    for (int k = 0; k < to_send_frame_num; k++)
        memcpy((pdata + sizeof(DMD_PATTERN) * k / 4), (int *) dmd_pattern_data, sizeof(DMD_PATTERN));
    DMA_ADDR cpu_memory_start_addr = pDMA_memory_space->Page[0].pPhysicalAddr;
    DMA_ADDR cpu_memory_addr;
    start = clock();

    for (int j = 0; j < 100; j++) {
        for (int k = 0; k < to_send_frame_num; k++) {

            printf("(j,k) is %d,%d.\n", j, k);
            printf("cpu_memory address is %lx.\n", cpu_memory_start_addr);
            cpu_memory_addr = cpu_memory_start_addr + k * sizeof(DMD_PATTERN);
            DMAToOnchipMem(cpu_memory_addr, sizeof(DMD_PATTERN), 0x20);
            // delay is inserted to ensure the WPS send is done
            //for (int delay = 10000; delay > 0; delay--);
            status = FPGA_read(vendor_id, device_id, (int *)(&wps_register), sizeof(WPS_REG), sizeof(WPS_REG), 0, 0, 1);
        }
    }

    WDC_DMASyncIo(pDMA_memory_space);
    WDC_DMABufUnlock(pDMA_memory_space);

    finish = clock();
    duration = (double)(finish - start) / CLOCKS_PER_SEC;
    printf("%f seconds\n", duration);
    */

    //for (int k = 0; k < to_send_frame_num; k++) {
       // DMAToOnchipMem(cpu_start_mem_addr, sizeof(DMD_PATTERN), 0x20);
      //  WDC_DMASyncIo(pDMA_memory_space);
      //  WDC_DMABufUnlock(pDMA_memory_space);
       // status = FPGA_read(vendor_id, device_id, (int *)(&wps_register), sizeof(WPS_REG), sizeof(WPS_REG), 0, 0, 1);
   // }
  //  WDC_DMABufUnlock(pDMA_memory_space);
    //free(pDMA_memory_space);
     //WDC_DMASyncIo(pDMA_memory_space);
    //WDC_DMABufUnlock(pDMA_memory_space);
    //free(pdata);

    //status = FPGA_read(vendor_id, device_id, (int *)(&wps_register), sizeof(WPS_REG), sizeof(WPS_REG), 0, 0, 1);
    /*
    for (int i = 0; i < 8; i++) {
        WDC_ReadAddr32(hDev, ALTERA_AD_BAR4, (ONCHIP_MEM_BASE_ADDR_LOW + i * 4 ), &read_data);
      printf("Read_data from onchip memory Address %x is %x.\n", (ONCHIP_MEM_BASE_ADDR_LOW + i * 4), read_data);
    }
    */
    // FPGA writes to CPU
    /*
    int *rd_data_ptr;
    rd_data_ptr = FPGA_write_to_CPU(DMA_SIZE_PER_DESCRIPTOR, DMA_SIZE_PER_DESCRIPTOR, 0, 0, 0);
    for (int k = 0; k < 16; k++)
        printf("data is %x.\n", *(rd_data_ptr + k));

    free(rd_data_ptr);
    rd_data_ptr = NULL;
    */
    return status;
}