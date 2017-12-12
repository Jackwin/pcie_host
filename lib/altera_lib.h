/* Jungo Confidential. Copyright (c) 2010 Jungo Ltd.  http://www.jungo.com */

/*
 * File - altera_lib.h
 *
 * Library for accessing the ALTERA card,
 * Accesses the hardware via WinDriver API functions.
 */

#ifndef _ALTERA_LIB_H_
#define _ALTERA_LIB_H_

#ifdef __KERNEL__
    #include "kdstdlib.h"
#endif //__KERNEL__
#include "windrvr.h"
#include "samples/shared/pci_regs.h"
#include "samples/shared/bits.h"

#ifdef __cplusplus
extern "C" {
#endif
//------------------------- Altera DMA parameters ------------------------------------------
#define ALTERA_DMA_BAR_NUM (6)
#define ALTERA_DMA_DESCRIPTOR_NUM 128

#define ALTERA_DMA_WR_DMA_CTRL          0x0000
#define ALTERA_DMA_WR_DESC_STATUS       0x0004
#define ALTERA_DMA_WR_RC_DESC_BASE_LOW  0x0008
#define ALTERA_DMA_WR_RC_DESC_BASE_HIGH 0x000C
#define ALTERA_DMA_WR_LAST_DESC_IDX     0x0010
#define ALTERA_DMA_WR_EP_DESC_BASE_LOW  0x0014
#define ALTERA_DMA_WR_EP_DESC_BASE_HIGH 0x0018
#define ALTERA_DMA_WR_DMA_PERF          0x001C

#define ALTERA_DMA_RD_DMA_CTRL          0x0100
#define ALTERA_DMA_RD_DESC_STATUS       0x0104
#define ALTERA_DMA_RD_RC_DESC_BASE_LOW  0x0108
#define ALTERA_DMA_RD_RC_DESC_BASE_HIGH 0x010C
#define ALTERA_DMA_RD_LAST_DESC_IDX     0x0110
#define ALTERA_DMA_RD_EP_DESC_BASE_LOW  0x0114
#define ALTERA_DMA_RD_EP_DESC_BASE_HIGH 0x0118
#define ALTERA_DMA_RD_DMA_PERF          0x011C

#define ALTERA_LITE_DMA_RD_RC_LOW_SRC_ADDR      0x0000
#define ALTERA_LITE_DMA_RD_RC_HIGH_SRC_ADDR     0x0004
#define ALTERA_LITE_DMA_RD_CTLR_LOW_DEST_ADDR   0x0008
#define ALTERA_LITE_DMA_RD_CTRL_HIGH_DEST_ADDR  0x000C
#define ALTERA_LITE_DMA_RD_LAST_PTR             0x0010

#define ALTERA_LITE_DMA_WR_RC_LOW_SRC_ADDR      0x0100
#define ALTERA_LITE_DMA_WR_RC_HIGH_SRC_ADDR     0x0104
#define ALTERA_LITE_DMA_WR_CTLR_LOW_DEST_ADDR   0x0108
#define ALTERA_LITE_DMA_WR_CTRL_HIGH_DEST_ADDR  0x010C
#define ALTERA_LITE_DMA_WR_LAST_PTR             0x0110

#define ALTERA_EPLAST_DIFF              1

#define ALTERA_DMA_NUM_DWORDS           512

// On-chip 4KB FIFO 0x0000_0000_0800_0000 - 0x0000_0000_0800_ffff
#define ONCHIP_MEM_BASE_ADDR_HI         0x00000000
#define ONCHIP_MEM_BASE_ADDR_LOW        0x00008000
#define ONCHIP_MEM_DESC_MEM_BASE        0x0000

//DDR3 128MB 0x0000_0000_0000_0000 - 0x0000_0000_07ff_ffff
#define DDR_MEM_BASE_ADDR_HI            0x00000000
#define DDR_MEM_BASE_ADDR_LOW           0x00000000
#define RD_CTRL_BUF_BASE_LOW            0x80000000
#define RD_CTRL_BUF_BASE_HI             0x0000
#define WR_CTRL_BUF_BASE_LOW            0x80002000
#define WR_CTRL_BUF_BASE_HI             0x0000

struct dma_descriptor {
    WORD src_addr_low;
    WORD src_addr_high;
    WORD dest_addr_low;
    WORD dest_addr_high;
    WORD ctl_dma_len;
    WORD reserved[3];
} __attribute__ ((packed));

struct dma_header {
    u32 eplast;
    u32 reserved[7];
} __attribute__ ((packed));

struct dma_desc_table {
    struct dma_header header;
    struct dma_descriptor descriptors[ALTERA_DMA_DESCRIPTOR_NUM];
} __attribute__ ((packed));

struct lite_dma_header {
    volatile u32 flags[128];
} __attribute__ ((packed));


struct lite_dma_desc_table {
    struct lite_dma_header header;
    struct dma_descriptor descriptors[ALTERA_DMA_DESCRIPTOR_NUM];
} __attribute__ ((packed));

enum { ALTERA_DEFAULT_VENDOR_ID = 0x1172 };
enum { ALTERA_DEFAULT_DEVICE_ID = 0x0004 };

typedef enum
{
    ALTERA_MODE_BYTE   = 0,
    ALTERA_MODE_WORD   = 1,
    ALTERA_MODE_DWORD  = 2
} ALTERA_MODE;

typedef enum
{
    ALTERA_AD_BAR0 = AD_PCI_BAR0,
    ALTERA_AD_BAR1 = AD_PCI_BAR1,
    ALTERA_AD_BAR2 = AD_PCI_BAR2,
    ALTERA_AD_BAR3 = AD_PCI_BAR3,
    ALTERA_AD_BAR4 = AD_PCI_BAR4,
    ALTERA_AD_BAR5 = AD_PCI_BAR5,
    ALTERA_ITEMS = AD_PCI_BARS,
} ALTERA_ADDR;

typedef struct ALTERA_STRUCT *ALTERA_HANDLE;

typedef struct
{
    DWORD dwCounter;   // number of interrupts received
    DWORD dwLost;      // number of interrupts not yet dealt with
    BOOL fStopped;     // was interrupt disabled during wait
} ALTERA_INT_RESULT;

typedef void (DLLCALLCONV *ALTERA_INT_HANDLER)( ALTERA_HANDLE hALTERA, ALTERA_INT_RESULT *intResult);

BOOL ALTERA_Open (ALTERA_HANDLE *phALTERA, DWORD dwVendorID, DWORD dwDeviceID, DWORD nCardNum);
void ALTERA_Close(ALTERA_HANDLE hALTERA);
DWORD ALTERA_CountCards (DWORD dwVendorID, DWORD dwDeviceID);
BOOL ALTERA_IsAddrSpaceActive(ALTERA_HANDLE hALTERA, ALTERA_ADDR addrSpace);

void ALTERA_GetPciSlot(ALTERA_HANDLE hALTERA, WD_PCI_SLOT *pPciSlot);

// General read/write function
void ALTERA_ReadWriteBlock(ALTERA_HANDLE hALTERA, ALTERA_ADDR addrSpace, DWORD dwOffset, BOOL fRead, PVOID buf, DWORD dwBytes, ALTERA_MODE mode);
BYTE ALTERA_ReadByte (ALTERA_HANDLE hALTERA, ALTERA_ADDR addrSpace, DWORD dwOffset);
WORD ALTERA_ReadWord (ALTERA_HANDLE hALTERA, ALTERA_ADDR addrSpace, DWORD dwOffset);
UINT32 ALTERA_ReadDword (ALTERA_HANDLE hALTERA, ALTERA_ADDR addrSpace, DWORD dwOffset);
void ALTERA_WriteByte (ALTERA_HANDLE hALTERA, ALTERA_ADDR addrSpace, DWORD dwOffset, BYTE data);
void ALTERA_WriteWord (ALTERA_HANDLE hALTERA, ALTERA_ADDR addrSpace, DWORD dwOffset, WORD data);
void ALTERA_WriteDword (ALTERA_HANDLE hALTERA, ALTERA_ADDR addrSpace, DWORD dwOffset, UINT32 data);
// handle interrupts
BOOL ALTERA_IntIsEnabled (ALTERA_HANDLE hALTERA);
BOOL ALTERA_IntEnable (ALTERA_HANDLE hALTERA, ALTERA_INT_HANDLER funcIntHandler);
void ALTERA_IntDisable (ALTERA_HANDLE hALTERA);
// access to PCI configuration registers
void ALTERA_WritePCIReg(ALTERA_HANDLE hALTERA, DWORD dwReg, UINT32 dwData);
UINT32 ALTERA_ReadPCIReg(ALTERA_HANDLE hALTERA, DWORD dwReg);

// Function for performing Scatter/Gather DMA
BOOL ALTERA_DMAReadWriteBlock(ALTERA_HANDLE hALTERA, DWORD dwLocalAddr, PVOID pBuffer, BOOL fRead, DWORD dwBytes, BOOL fChained);


// this string is set to an error message, if one occurs
extern CHAR ALTERA_ErrorString[];


enum {
    ALTERA_REG_DMACSR = 0x0, /* 0 Control Status register */
    ALTERA_REG_DMAACR = 0x4, /* 4 PCI Address - Host memory address */
    ALTERA_REG_DMABCR = 0x8, /* 8 Byte Count Register - In SG mode */
    ALTERA_REG_DMAISR = 0xc, /* c Interrupt Status register */
    ALTERA_REG_DMALAR = 0x10,
    ALTERA_REG_SDRAM_BAR = 0x18,
    ALTERA_REG_SDRAM_CFG0 = 0x1c,
    ALTERA_REG_SDRAM_CFG1 = 0x20,
    ALTERA_REG_SDRAM_CFG2 = 0x24,
    ALTERA_REG_TARG_PERF = 0x80,
    ALTERA_REG_MSTR_PERF = 0x84,
};

enum {
    INTERRUPT_ENABLE =  0x0001,
    DMA_ENABLE      =   0x0010,
    TRXCOMPINTDIS   =   0x0020,
    CHAINEN         =   0x0100,
    DIRECTION       =   0x0008,
};

enum { FIFO_BASE_OFFSET = 512 * 1024 }; // was 16 * 1M
enum { SDRAM_BASE_OFFSET = 0 }; // was 32 * 1M
enum { ALTERA_DMA_FIFO_REGS = 128 };

// ISR
enum {
    INTERUPT_PENDING    = 0x01,
    ERROR_PENDING       = 0x02,
    LOCAL_REQ           = 0x04,
    TX_COMP             = 0x08,
    ACR_LOADED          = 0x10,
    CHAIN_STARTED       = 0x20,
};

#ifdef __cplusplus
}
#endif

#endif

