/* Jungo Confidential. Copyright (c) 2010 Jungo Ltd.  http://www.jungo.com */

/*
 * altera_lib.c
 *
 * Library for accessing the ALTERA card.
 * Accesses the hardware via WinDriver API functions.
 */

#include "altera_lib.h"
#include "pci_driver_lib.h"
#include "windrvr_int_thread.h"
#include "status_strings.h"
#include <stdio.h>
//#include "../x86/msdev_2010/dmd.h"
#define PCI_DRIVER_DEFAULT_DRIVER_NAME "windrvr6"
#define PCI_DRIVER_DEFAULT_LICENSE_STRING "6C3CC2CFE89E7AD0424A070D434A6F6DC49520ED.wps"

#define RP_RD_BUFFER_SZIE ALTERA_DMA_NUM_DWORDS
#define EP_WR_BUFFER_SZIE ALTERA_DMA_NUM_DWORDS

#define DMA_NUM_MAX
//extern FAST_DMD_PATTERN fast_dmd_pattern;
/* This string is set to an error message, if one occurs */
CHAR ALTERA_ErrorString[1024];
typedef struct
{
    WD_INTERRUPT Int;
    HANDLE hThread;
    WD_TRANSFER Trans[1];
    ALTERA_INT_HANDLER funcIntHandler;
} ALTERA_INT_INTERRUPT;

typedef struct
{
    DWORD index;
    BOOL  fIsMemory;
    BOOL  fActive;
} ALTERA_ADDR_DESC;

typedef struct ALTERA_STRUCT
{
    HANDLE hWD;
    ALTERA_INT_INTERRUPT Int;
    WD_PCI_SLOT pciSlot;
    ALTERA_ADDR_DESC addrDesc[ALTERA_ITEMS];
    WD_CARD_REGISTER cardReg;
} ALTERA_STRUCT;

//WDC_DEVICE_HANDLE hDev;

//struct altera_pcie_dma_bookkeep *bk_ptr;
DWORD *rp_rd_buffer;
DWORD *ep_wr_buffer;

// DMA array used to apply for applying pyhsical space for the buffers
WD_DMA *pDMA_array[DMA_NUM_MAX];

//---------------------------- Usr Application -------------------------------------------------------
/*
Return: 0 -- Success
        1 -- Fail to initialize driver library
        2 -- Fail to scan the PCI driver
        3 -- Can not find the PCI handle
*/
DWORD initialize_PCI(DWORD VENDOR_ID, DWORD DEVICE_ID) {
    WDC_PCI_SCAN_RESULT scanResult;
    WD_PCI_CARD_INFO    deviceInfo;
    DWORD  dwStatus = PCI_DRIVER_LibInit();
    if (WD_STATUS_SUCCESS != dwStatus)
    {
        printf("Fail to initiate driver library.\n");
        return 1;
    }

    dwStatus = WDC_PciScanDevices(VENDOR_ID, DEVICE_ID, &scanResult);
    if (WD_STATUS_SUCCESS != dwStatus)
    {
        WDC_Err("Failed to scan the PCI driver. Error 0x%lx - %s\n", dwStatus, Stat2Str(dwStatus));
       return 2;
    }
    deviceInfo.pciSlot = scanResult.deviceSlot[0];//scanResult.dwNumDevices  - 1
    WDC_PciGetDeviceInfo(&deviceInfo);

    hDev = PCI_DRIVER_DeviceOpen(&deviceInfo);
    if (hDev == NULL) {
        WDC_Err("hu: Failed to find the handle to the PCI driver.\n");
        return 3;
    }
    return 0;
}

/*
Return: 0 -- Success
        1 -- Fail to close the PCI driver
        2 -- Fail to uninitialize the PCI driver library
*/
DWORD close_pci (WDC_DEVICE_HANDLE hDev) {
   // TraceLog("Start to close PCI...\n");

    DWORD dwStatus, i = 0;
    dwStatus = PCI_DRIVER_DeviceClose(hDev);
    if (!dwStatus) {
        WDC_Err("Fail to close the PCI driver.\n");
        i++;
    }

    dwStatus = PCI_DRIVER_LibUninit();
    if (WD_STATUS_SUCCESS != dwStatus) {
        WDC_Err("Failed to uninitialize the PCI driver library. Error 0x%lx - %s\n",dwStatus, Stat2Str(dwStatus));
        i++;
    }
   // TraceLog("Success to close the PCI driver.\n");
    return i;
}


BOOL PCI_Get_WD_handle(HANDLE *phWD)
{
    WD_VERSION ver;
    DWORD dwStatus;

    *phWD = WD_Open();
    if (*phWD == INVALID_HANDLE_VALUE)
    {
        printf("Failed opening " WD_PROD_NAME " device\n");
        return FALSE;
    }

    BZERO(ver);
    dwStatus = WD_Version(*phWD, &ver);
    if (dwStatus)
    {
        //printf("WD_Version() failed (0x%lx) - %s\n", dwStatus, Stat2Str(dwStatus));
        goto Error;
    }
    if (ver.dwVer<WD_VER)
    {
        printf("Incorrect " WD_PROD_NAME " version. Expected %d.%02d, got %ld.%02ld\n",
            WD_VER / 100, WD_VER % 100, ver.dwVer / 100, ver.dwVer % 100);
        goto Error;
    }
    return TRUE;
Error:
    WD_Close(*phWD);
    *phWD = INVALID_HANDLE_VALUE;
    return FALSE;
}
/*
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
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTRL_HIGH_DEST_ADDR, RD_CTRL_BUF_BASE_LOW);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTLR_LOW_DEST_ADDR, RD_CTRL_BUF_BASE_LOW);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_TABLE_SIZE, (ALTERA_DMA_DESCRIPTOR_NUM - 1));
    }
    else {
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_RC_HIGH_SRC_ADDR, dt_phy_addr_h);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_RC_LOW_SRC_ADDR, dt_phy_addr_l);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_CTRL_HIGH_DEST_ADDR, WR_CTRL_BUF_BASE_LOW);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_CTLR_LOW_DEST_ADDR, WR_CTRL_BUF_BASE_LOW);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_TABLE_SIZE, (ALTERA_DMA_DESCRIPTOR_NUM-1));
    }
    return TRUE;
}
*/
// Initialize the descriptor table header
int set_lite_table_header(struct lite_dma_header *header) {
    int i;
    for (i = 0; i < 128; i++)
        header->flags[i] = 0x00;
    return 0;
}

WORD init_rp_mem(DWORD *rp_buffer_virt_addr, DWORD num_dword) {
    DWORD i = 0;
    //DWORD increment_value = 0;
    DWORD tmp_rand = 0x01020304;
    for (i = 0; i < num_dword; i++) {
        tmp_rand = tmp_rand + 0x04040404;
        //tmp_rand = rand();
        //printf("init_rp_mem tmp_rand is 0x%X.\n", tmp_rand);
        *(rp_buffer_virt_addr + i) = tmp_rand;
    }
    return 1;
}

// Func:Apply for the physical address
// 1)dwDMA_buf_size: the buffer size in byte
// 2)bfrom_dev: 1->DEVICE_FROM_DEVICE, 0 -> DEVICE_TO_DEVICE
// 3)bSGbuf: 1->SG buffer 0->Continuous buffer
DWORD ApplyPhysicalAddress (WD_DMA *pDMA, BOOL bfrom_dev, DMA_BUFFER *DMA_buf) {
   // TraceLog("Apply physical address for buffers.\n");
    DWORD dwstatus;
    if (bfrom_dev) {
        dwstatus = WDC_DMAContigBufLock(hDev, DMA_buf->pdata, DMA_FROM_DEVICE, DMA_buf->dwpages * PAGE_SIZE, &pDMA);
    }
    else {
        dwstatus = WDC_DMAContigBufLock(hDev, DMA_buf->pdata, DMA_TO_DEVICE, DMA_buf->dwpages * PAGE_SIZE, &pDMA);
    }
    if (WD_STATUS_SUCCESS != dwstatus) {
        WDC_Err("Failed to apply physical addr. Error 0x%lx - %s\n", dwstatus, Stat2Str(dwstatus));
        return 1;
    }

    DMA_buf->phy_addr = pDMA->Page[0].pPhysicalAddr;
    DMA_buf->pdata = pDMA->pUserAddr;
    dwstatus = WDC_DMASyncCpu(pDMA);
    if (WD_STATUS_SUCCESS != dwstatus) {
        WDC_Err("Failed to sync to the cache of CPU with the physical address. Error 0x%lx - %s\n", dwstatus, Stat2Str(dwstatus));
        return 2;
    }
    return 0;
}
/*
Return: 0 -- Success
        1 -- Fail to sync I/O cache
        2 -- Fail to free physical address
*/
DWORD FreePhysicalAddress(WD_DMA *pDMA) {
    DWORD dwstatus;
    dwstatus = WDC_DMASyncIo(pDMA);
    if (WD_STATUS_SUCCESS != dwstatus) {
        WDC_Err("Failed to sync the I/O cachees with the physical address. Error 0x%lx - %s\n", dwstatus, Stat2Str(dwstatus));
        return 1;
    }

    dwstatus = WDC_DMABufUnlock(pDMA);
    if (WD_STATUS_SUCCESS != dwstatus) {
        WDC_Err("Failed to free the physical address. Error 0x%lx - %s\n", dwstatus, Stat2Str(dwstatus));
        return 2;
    }
    return 0;
}
/*
DWORD InitDMABookkeep(WDC_DEVICE_HANDLE hDev, WD_DMA **ppDma, WD_DMA **ppDma_wr,  WD_DMA **ppDMA_rd_buf, WD_DMA **ppDMA_wr_buf) {
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
/*
    //Initialize fast DMD pattern
    init_fast_dmd_pat_dt(1920, 1080, 0, 0, 0, 0xfdfdfdfd);
    init_fast_dma_pat_data();
    status = WDC_DMASGBufLock(hDev, &fast_dmd_pattern, DMA_TO_DEVICE, sizeof(FAST_DMD_PATTERN), ppDMA_rd_buf);
    printf("sizeof fast_dmd_patten is %d.\n", sizeof(FAST_DMD_PATTERN));

    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to initiate DMAContigBuf for rd_buf.\n");
    }
    status = WDC_DMASyncCpu(*ppDMA_rd_buf);
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to CPUSync ppDMA.\n");
    }
   // bk_ptr->dma_status.altera_dma_num_dwords = (*ppDMA_rd_buf)->Page[0].dwBytes;
    // DMA write operation, moving data from device to pcie. Apply space for wr buffer
    ep_wr_buffer = (DWORD *)malloc(RP_RD_BUFFER_SZIE);
    status = WDC_DMASGBufLock(hDev, ep_wr_buffer, DMA_FROM_DEVICE, EP_WR_BUFFER_SZIE, ppDMA_wr_buf);
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to initiate DMASGBuf for ep_wr_buf.\n");
    }
    status = WDC_DMASyncCpu(*ppDMA_wr_buf);
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to CPUSync ppDMA.\n");
    }

    (bk_ptr->rp_rd_buffer_virt_addr) = (*ppDMA_rd_buf)->pUserAddr;
    (bk_ptr->rp_rd_buffer_bus_addr) = (*ppDMA_rd_buf)->Page[0].pPhysicalAddr;
    (bk_ptr->rp_wr_buffer_virt_addr) = (*ppDMA_wr_buf)->pUserAddr;
    (bk_ptr->rp_wr_buffer_bus_addr) = (*ppDMA_wr_buf)->Page[0].pPhysicalAddr;
    return status;
}
/*
WORD init_ep_mem(ALTERA_HANDLE hALTERA, DWORD mem_byte_offset, DWORD num_dwords, BYTE increment) {
    DWORD i = 0;
    DWORD increment_value = 0;
    DWORD tmp_rand;
    for (i = 0; i < num_dwords; i++) {
        if (increment) increment_value = i;
//       iowrite32 (cpu_to_le32(init_value+increment_value), (DWORD *)(bk_ptr->bar[4]+mem_byte_offset)+increment_value);
        //get_random_bytes(&tmp_rand, sizeof(tmp_rand));
       // tmp_rand = rand();

        //iowrite32 (cpu_to_le32(tmp_rand), (DWORD *)(bk_ptr->bar[4]+mem_byte_offset)+i);
        ALTERA_WriteDword(hALTERA, ALTERA_AD_BAR4, mem_byte_offset + i * 4, i);
    }

    return 1;
}
*/
/*
// DMA READ -> Move data from CPU to FPGA
BOOL ALTERA_DMABlock(ALTERA_HANDLE hALTERA, BOOL fromDev, DWORD vendor_id, DWORD device_id) {
    // make sure WinDriver is loaded
    if (!PCI_Get_WD_handle(&hDev)) {
        printf("Fail to get WD_handle.\n");
        return 0;
    }
    WD_Close(hDev);
    DWORD status;
    status = initialize_PCI(vendor_id, device_id);
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to initialize PCI.\n");
    }

    DWORD last_id, write_127 = 0;
    DWORD timeout;
    WD_DMA *ppDMA = NULL, *ppDMA_wr = NULL, *ppDMA_rd_buf= NULL, *ppDMA_wr_buf=NULL;
    PVOID pBuf = NULL;
    status = InitDMABookkeep(hDev, &ppDMA, &ppDMA_wr,  &ppDMA_rd_buf, &ppDMA_wr_buf);
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to initiate DMA bookkeep.\n");
    }

    if (!fromDev) {
        DMA_ADDR onchip_mem_base_addr = (ONCHIP_MEM_BASE_ADDR_HI << 32) + ONCHIP_MEM_BASE_ADDR_LOW;
        DMA_ADDR onchip_mem_start_addr;
        DWORD current_page_size, pre_page_size;
        // Construct the descriptor table
        for (int i = 0; i < ALTERA_DMA_DESCRIPTOR_NUM; i++) {
            (bk_ptr->rp_rd_buffer_bus_addr) = ppDMA_rd_buf->Page[i].pPhysicalAddr;  // Buffer's physical address
            DWORD rd_buf_phy_addr_h = (bk_ptr->rp_rd_buffer_bus_addr >> 32) & 0xffffffff;
            DWORD rd_buf_phy_addr_l = bk_ptr->rp_rd_buffer_bus_addr & 0xffffffff;

            if (i == 0) {
                onchip_mem_start_addr = onchip_mem_base_addr;
                current_page_size = ppDMA_rd_buf->Page[i].dwBytes;
                pre_page_size = 0;

            }
            else {
                current_page_size = ppDMA_rd_buf->Page[i].dwBytes;
                pre_page_size = ppDMA_rd_buf->Page[i - 1].dwBytes;
                onchip_mem_start_addr = onchip_mem_start_addr + pre_page_size;
            }

            DWORD onchip_mem_addr_h = (onchip_mem_start_addr >> 32) & 0xffffffff;
            DWORD onchip_mem_addr_l = onchip_mem_start_addr & 0xffffffff;
            DWORD dma_dword = (current_page_size) / 4; // Dword number

            SetDescTable(&(bk_ptr->lite_table_rd_cpu_virt_addr.descriptors[i]), rd_buf_phy_addr_h, rd_buf_phy_addr_l, onchip_mem_addr_h, onchip_mem_addr_l, dma_dword, i);
        }

        WDC_ReadAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR, &last_id);

        if (last_id == 0xff) {
            ConfigDMADescController(hDev, bk_ptr->lite_table_rd_bus_addr, fromDev);
            last_id = ALTERA_DMA_DESCRIPTOR_NUM - 1;
        }
        last_id = ALTERA_DMA_DESCRIPTOR_NUM - 1;

        ConfigDMADescController(hDev, bk_ptr->lite_table_rd_bus_addr, fromDev);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR, last_id);
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

        WDC_DMASyncIo(ppDMA);
        WDC_DMABufUnlock(ppDMA);
        //WDC_DMASyncIo(ppDMA_wr);
        //WDC_DMABufUnlock(ppDMA_wr);
        WDC_DMASyncIo(ppDMA_rd_buf);
        WDC_DMABufUnlock(ppDMA_rd_buf);
        WDC_DMASyncIo(ppDMA_wr_buf);
        WDC_DMABufUnlock(ppDMA_wr_buf);
        free(rp_rd_buffer);
        free(ep_wr_buffer);
        //free(bk_ptr);

        close_pci(hDev);
        return TRUE;
    }
    else {
        DWORD wr_buf_phy_addr_h = (bk_ptr->rp_wr_buffer_bus_addr >> 32) & 0xffffffff;
        DWORD wr_buf_phy_addr_l = bk_ptr->rp_wr_buffer_bus_addr & 0xffffffff;
        for (int i = 0; i < ALTERA_DMA_DESCRIPTOR_NUM; i++) {
           // SetDescTable(&(bk_ptr->lite_table_wr_cpu_virt_addr.descriptors[i]), DDR_MEM_BASE_ADDR_HI, DDR_MEM_BASE_ADDR_LOW, wr_buf_phy_addr_h, wr_buf_phy_addr_l, bk_ptr->dma_status.altera_dma_num_dwords, i);
            }
        WDC_ReadAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_LAST_PTR, &last_id);

        if (last_id == 0xff) {
            ConfigDMADescController(hDev, bk_ptr->lite_table_wr_bus_addr, fromDev);
            last_id = ALTERA_DMA_DESCRIPTOR_NUM - 1;
        }

        last_id = last_id + ALTERA_DMA_DESCRIPTOR_NUM;
        //Over DMA request
        if (last_id > (ALTERA_DMA_DESCRIPTOR_NUM - 1)) {
            last_id = last_id - ALTERA_DMA_DESCRIPTOR_NUM;
           // if ((bk_ptr->dma_status.altera_dma_descriptor_num > 1) && (last_id != (ALTERA_DMA_DESCRIPTOR_NUM - 1))) write_127 = 1;
        }
        if (write_127)
            WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_LAST_PTR, (ALTERA_DMA_DESCRIPTOR_NUM - 1));
        ConfigDMADescController(hDev, bk_ptr->lite_table_wr_bus_addr, fromDev);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_LAST_PTR, last_id);
        printf("Last_id is %d.\n", last_id);
        timeout = TIMEOUT;
        while (1) {
            if (bk_ptr->lite_table_wr_cpu_virt_addr.header.flags[last_id]) {
                printf("DMA write operation is successful.\n");
                break;
            }
            timeout--;
            if (timeout == 0) {
                printf("DMA write operation is timeout.\n");
                break;
            }
        }
        WDC_DMASyncIo(ppDMA);
        WDC_DMABufUnlock(ppDMA);
        WDC_DMASyncIo(ppDMA_wr);
        WDC_DMABufUnlock(ppDMA_wr);
        WDC_DMASyncIo(ppDMA_rd_buf);
        WDC_DMABufUnlock(ppDMA_rd_buf);
        WDC_DMASyncIo(ppDMA_wr_buf);
        WDC_DMABufUnlock(ppDMA_wr_buf);
        free(rp_rd_buffer);
        free(ep_wr_buffer);
        free(bk_ptr);
        //WDC_PciDeviceClose(hDev);
        close_pci(hDev);
        return TRUE;
    }
    return TRUE;
}
*/