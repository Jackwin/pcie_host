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
#include "../x86/msdev_2010/dmd.h"
#define PCI_DRIVER_DEFAULT_DRIVER_NAME "windrvr6"
#define PCI_DRIVER_DEFAULT_LICENSE_STRING "6C3CC2CFE89E7AD0424A070D434A6F6DC49520ED.wps"

#define RP_RD_BUFFER_SZIE ALTERA_DMA_NUM_DWORDS
#define EP_WR_BUFFER_SZIE ALTERA_DMA_NUM_DWORDS

#define DMA_NUM_MAX
extern FAST_DMD_PATTERN fast_dmd_pattern;
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

WDC_DEVICE_HANDLE hDev;

struct altera_pcie_dma_bookkeep *bk_ptr1;
DWORD *rp_rd_buffer;
DWORD *ep_wr_buffer;

// DMA array used to apply for applying pyhsical space for the buffers
WD_DMA *pDMA_array[DMA_NUM_MAX];

/* Internal function used by ALTERA_Open() */
BOOL ALTERA_DetectCardElements(ALTERA_HANDLE hALTERA);
/**
 * Function: ALTERA_CountCards()
 *   Count the number of PCI cards that meets the given criteria.
 * Parameters:
 *   hALTERA [in] handle to the card as received from ALTERA_Open().
 *   dwVendorID [in] indicates the vendor ID of the PCI card to search.
 *     0 indicates all vendor IDs.
 *   dwDeviceID [in] indicates the Device ID of the PCI card to search.
 *     0 indicates all device IDs.
 * Return Value:
 *   the number of PCI cards found.
 **/
DWORD ALTERA_CountCards(DWORD dwVendorID, DWORD dwDeviceID)
{
    WD_VERSION ver;
    WD_PCI_SCAN_CARDS pciScan;
    HANDLE hWD = INVALID_HANDLE_VALUE;
    DWORD dwStatus;

    ALTERA_ErrorString[0] = '\0';
    hWD = WD_Open();
    /* Check if handle valid & version OK */
    if (hWD == INVALID_HANDLE_VALUE)
    {
        sprintf(ALTERA_ErrorString, "Failed opening " WD_PROD_NAME " device\n");
        return 0;
    }

    BZERO(ver);
    WD_Version(hWD, &ver);
    if (ver.dwVer < WD_VER)
    {
        sprintf(ALTERA_ErrorString, "Incorrect " WD_PROD_NAME " version\n");
        WD_Close(hWD);
        return 0;
    }

    BZERO(pciScan);
    pciScan.searchId.dwVendorId = dwVendorID;
    pciScan.searchId.dwDeviceId = dwDeviceID;
    dwStatus = WD_PciScanCards(hWD, &pciScan);
    WD_Close(hWD);
    if (dwStatus)
    {
        sprintf(ALTERA_ErrorString, "WD_PciScanCards() failed with status "
                "0x%lx - %s\n", dwStatus, Stat2Str(dwStatus));
    }
    else if (pciScan.dwCards == 0)
        sprintf(ALTERA_ErrorString, "no cards found\n");
    return pciScan.dwCards;
}

BOOL ALTERA_Open(ALTERA_HANDLE *phALTERA, DWORD dwVendorID, DWORD dwDeviceID,
                 DWORD nCardNum)
{
    ALTERA_HANDLE hALTERA = (ALTERA_HANDLE) malloc(sizeof(ALTERA_STRUCT));

    WD_VERSION ver;
    WD_PCI_SCAN_CARDS pciScan;
    WD_PCI_CARD_INFO pciCardInfo;
    DWORD dwStatus;

    *phALTERA = NULL;
    ALTERA_ErrorString[0] = '\0';
    BZERO(*hALTERA);

    hALTERA->hWD = WD_Open();

    /* Check if handle valid & version OK */
    if (hALTERA->hWD == INVALID_HANDLE_VALUE)
    {
        sprintf(ALTERA_ErrorString, "Failed opening " WD_PROD_NAME " device\n");
        goto Exit;
    }

    BZERO(ver);
    WD_Version(hALTERA->hWD, &ver);
    if (ver.dwVer < WD_VER)
    {
        sprintf(ALTERA_ErrorString, "Incorrect " WD_PROD_NAME " version\n");
        goto Exit;
    }

    BZERO(pciScan);
    pciScan.searchId.dwVendorId = dwVendorID;
    pciScan.searchId.dwDeviceId = dwDeviceID;
    dwStatus = WD_PciScanCards(hALTERA->hWD, &pciScan);
    if (dwStatus)
    {
        sprintf(ALTERA_ErrorString, "WD_PciScanCards() failed with status "
                "0x%lx - %s\n", dwStatus, Stat2Str(dwStatus));
        goto Exit;
    }
    if (pciScan.dwCards == 0) /* Found at least one card */
    {
        sprintf(ALTERA_ErrorString, "Could not find PCI card\n");
        goto Exit;
    }
    if (pciScan.dwCards <= nCardNum)
    {
        sprintf(ALTERA_ErrorString, "Card out of range of available cards\n");
        goto Exit;
    }

    BZERO(pciCardInfo);
    pciCardInfo.pciSlot = pciScan.cardSlot[nCardNum];
    WD_PciGetCardInfo(hALTERA->hWD, &pciCardInfo);
    hALTERA->pciSlot = pciCardInfo.pciSlot;
    hALTERA->cardReg.Card = pciCardInfo.Card;

    hALTERA->cardReg.fCheckLockOnly = FALSE;
    dwStatus = WD_CardRegister(hALTERA->hWD, &hALTERA->cardReg);
    if (dwStatus)
    {
        sprintf(ALTERA_ErrorString, "WD_CardRegister() failed with status "
                "0x%lx - %s\n", dwStatus, Stat2Str(dwStatus));
    }
    if (hALTERA->cardReg.hCard == 0)
    {
        sprintf(ALTERA_ErrorString, "Failed locking device.\n");
        goto Exit;
    }

    if (!ALTERA_DetectCardElements(hALTERA))
    {
        sprintf(ALTERA_ErrorString, "Card does not have all items expected "
                "for ALTERA\n");
        goto Exit;
    }

    /* Open finished OK */
    *phALTERA = hALTERA;
    return TRUE;

Exit:
    /* Error during Open */
    if (hALTERA->cardReg.hCard)
        WD_CardUnregister(hALTERA->hWD, &hALTERA->cardReg);
    if (hALTERA->hWD != INVALID_HANDLE_VALUE)
        WD_Close(hALTERA->hWD);
    free(hALTERA);
    return FALSE;
}

void ALTERA_Close(ALTERA_HANDLE hALTERA)
{
    /* Disable interrupts */
    if (ALTERA_IntIsEnabled(hALTERA))
        ALTERA_IntDisable(hALTERA);

    /* Unregister card */
    if (hALTERA->cardReg.hCard)
        WD_CardUnregister(hALTERA->hWD, &hALTERA->cardReg);

    /* Close WinDriver */
    WD_Close(hALTERA->hWD);

    free(hALTERA);
}

void ALTERA_WritePCIReg(ALTERA_HANDLE hALTERA, DWORD dwReg, UINT32 dwData)
{
    WD_PCI_CONFIG_DUMP pciCnf;

    BZERO(pciCnf);
    pciCnf.pciSlot = hALTERA->pciSlot;
    pciCnf.pBuffer = &dwData;
    pciCnf.dwOffset = dwReg;
    pciCnf.dwBytes = 4;
    pciCnf.fIsRead = FALSE;
    WD_PciConfigDump(hALTERA->hWD, &pciCnf);
}

UINT32 ALTERA_ReadPCIReg(ALTERA_HANDLE hALTERA, DWORD dwReg)
{
    WD_PCI_CONFIG_DUMP pciCnf;
    UINT32 dwVal;

    BZERO(pciCnf);
    pciCnf.pciSlot = hALTERA->pciSlot;
    pciCnf.pBuffer = &dwVal;
    pciCnf.dwOffset = dwReg;
    pciCnf.dwBytes = 4;
    pciCnf.fIsRead = TRUE;
    WD_PciConfigDump(hALTERA->hWD, &pciCnf);
    return dwVal;
}

BOOL ALTERA_DetectCardElements(ALTERA_HANDLE hALTERA)
{
    DWORD i;
    DWORD ad_sp;
    BZERO(hALTERA->Int);
    BZERO(hALTERA->addrDesc);

    for (i = 0; i < hALTERA->cardReg.Card.dwItems; i++)
    {
        WD_ITEMS *pItem = &hALTERA->cardReg.Card.Item[i];

        switch (pItem->item)
        {
        case ITEM_MEMORY:
            ad_sp = pItem->I.Mem.dwBar;
            hALTERA->addrDesc[ad_sp].fIsMemory = TRUE;
            hALTERA->addrDesc[ad_sp].index = i;
            hALTERA->addrDesc[ad_sp].fActive = TRUE;
            break;
        case ITEM_IO:
            ad_sp = pItem->I.IO.dwBar;

            hALTERA->addrDesc[ad_sp].fIsMemory = FALSE;
            hALTERA->addrDesc[ad_sp].index = i;
            hALTERA->addrDesc[ad_sp].fActive = TRUE;
            break;
        case ITEM_INTERRUPT:
            if (hALTERA->Int.Int.hInterrupt) return FALSE;
            hALTERA->Int.Int.hInterrupt = pItem->I.Int.hInterrupt;
            break;
        }
    }

    /* Check that at least one memory space was found */
    for (i = 0; i < ALTERA_ITEMS; i++)
        if (ALTERA_IsAddrSpaceActive(hALTERA, (ALTERA_ADDR) i)) break;
    if (i == ALTERA_ITEMS)
        return FALSE;
    return TRUE;
}

BOOL ALTERA_IsAddrSpaceActive(ALTERA_HANDLE hALTERA, ALTERA_ADDR addrSpace)
{
    return hALTERA->addrDesc[addrSpace].fActive;
}

void ALTERA_GetPciSlot(ALTERA_HANDLE hALTERA, WD_PCI_SLOT *pPciSlot)
{
    memcpy((PVOID)pPciSlot, &(hALTERA->pciSlot), sizeof(WD_PCI_SLOT));
}

/* General read/write function */
void ALTERA_ReadWriteBlock(ALTERA_HANDLE hALTERA, ALTERA_ADDR addrSpace,
                           DWORD dwOffset, BOOL fRead, PVOID buf, DWORD dwBytes, ALTERA_MODE mode)
{
    WD_TRANSFER trans;
    BOOL fMem = hALTERA->addrDesc[addrSpace].fIsMemory;

    /* Safety check: is the address range active */
    if (!ALTERA_IsAddrSpaceActive(hALTERA, addrSpace))
        return;

    BZERO(trans);
    if (fRead)
    {

        if (mode == ALTERA_MODE_BYTE)
            trans.cmdTrans = fMem ? RM_SBYTE : RP_SBYTE;
        else if (mode == ALTERA_MODE_WORD)
            trans.cmdTrans = fMem ? RM_SWORD : RP_SWORD;
        else if (mode == ALTERA_MODE_DWORD)
            //trans.cmdTrans = fMem ? RM_SDWORD : RP_SDWORD;
            trans.cmdTrans = fMem ? RM_DWORD : RP_SDWORD;

    }
    else
    {
        if (mode == ALTERA_MODE_BYTE)
            trans.cmdTrans = fMem ? WM_SBYTE : WP_SBYTE;
        else if (mode == ALTERA_MODE_WORD)
            trans.cmdTrans = fMem ? WM_SWORD : WP_SWORD;
        else if (mode == ALTERA_MODE_DWORD)
            trans.cmdTrans = fMem ? WM_SDWORD : WP_SDWORD;
    }
    if (fMem)
        trans.dwPort = hALTERA->cardReg.Card.Item[hALTERA->addrDesc[addrSpace].index].I.Mem.dwTransAddr;
    else trans.dwPort = hALTERA->cardReg.Card.Item[hALTERA->addrDesc[addrSpace].index].I.IO.dwAddr;
    trans.dwPort += dwOffset;

    trans.fAutoinc = TRUE;
    trans.dwBytes = dwBytes;
    trans.dwOptions = 0;
    trans.Data.pBuffer = buf;
    WD_Transfer(hALTERA->hWD, &trans);
    DWORD *ptr = trans.Data.pBuffer;

   // for (int i = 0; i < 16; i++)
    //    printf("rddata in rdbuf is %x.\n", *(ptr+i));
}

BYTE ALTERA_ReadByte(ALTERA_HANDLE hALTERA, ALTERA_ADDR addrSpace,
                     DWORD dwOffset)
{
    BYTE data;
    if (hALTERA->addrDesc[addrSpace].fIsMemory)
    {
        PBYTE pData = (PBYTE)(hALTERA->cardReg.Card.Item[hALTERA->addrDesc[addrSpace].index].I.Mem.dwUserDirectAddr + dwOffset);
        data = *pData; /* Read from the memory mapped range directly */
    }
    else
    {
        ALTERA_ReadWriteBlock(hALTERA, addrSpace, dwOffset, TRUE, &data,
                              sizeof(BYTE), ALTERA_MODE_BYTE);
    }
    return data;
}

WORD ALTERA_ReadWord(ALTERA_HANDLE hALTERA, ALTERA_ADDR addrSpace,
                     DWORD dwOffset)
{
    WORD data;
    if (hALTERA->addrDesc[addrSpace].fIsMemory)
    {
        PWORD pData = (PWORD)(hALTERA->cardReg.Card.Item[hALTERA->addrDesc[addrSpace].index].I.Mem.dwUserDirectAddr + dwOffset);
        data = dtoh16(*pData); /* Read from the memory mapped range directly */
    }
    else
    {
        ALTERA_ReadWriteBlock(hALTERA, addrSpace, dwOffset, TRUE, &data,
                              sizeof(WORD), ALTERA_MODE_WORD);
    }
    return data;
}

UINT32 ALTERA_ReadDword(ALTERA_HANDLE hALTERA, ALTERA_ADDR addrSpace,
                        DWORD dwOffset)
{
    UINT32 data;
    if (hALTERA->addrDesc[addrSpace].fIsMemory)
    {
        UINT32 *pData = (UINT32 *)(hALTERA->cardReg.Card.Item[hALTERA->addrDesc[addrSpace].index].I.Mem.dwUserDirectAddr + dwOffset);
        data = dtoh32(*pData); /* Read from the memory mapped range directly */
    }
    else
    {
        ALTERA_ReadWriteBlock(hALTERA, addrSpace, dwOffset, TRUE, &data,
                              sizeof(UINT32), ALTERA_MODE_DWORD);
    }
    return data;
}

void ALTERA_WriteByte(ALTERA_HANDLE hALTERA, ALTERA_ADDR addrSpace,
                      DWORD dwOffset, BYTE data)
{
    if (hALTERA->addrDesc[addrSpace].fIsMemory)
    {
        PBYTE pData = (PBYTE)(hALTERA->cardReg.Card.Item[hALTERA->addrDesc[addrSpace].index].I.Mem.dwUserDirectAddr + dwOffset);
        *pData = data; /* Write to the memory mapped range directly */
    }
    else
    {
        ALTERA_ReadWriteBlock(hALTERA, addrSpace, dwOffset, FALSE, &data,
                              sizeof(BYTE), ALTERA_MODE_BYTE);
    }
}

void ALTERA_WriteWord(ALTERA_HANDLE hALTERA, ALTERA_ADDR addrSpace,
                      DWORD dwOffset, WORD data)
{
    if (hALTERA->addrDesc[addrSpace].fIsMemory)
    {
        PWORD pData = (PWORD)(hALTERA->cardReg.Card.Item[hALTERA->addrDesc[addrSpace].index].I.Mem.dwUserDirectAddr + dwOffset);
        *pData = dtoh16(data); /* Write to the memory mapped range directly */
    }
    else
    {
        ALTERA_ReadWriteBlock(hALTERA, addrSpace, dwOffset, FALSE, &data,
                              sizeof(WORD), ALTERA_MODE_WORD);
    }
}

void ALTERA_WriteDword(ALTERA_HANDLE hALTERA, ALTERA_ADDR addrSpace,
                       DWORD dwOffset, UINT32 data)
{
    if (hALTERA->addrDesc[addrSpace].fIsMemory)
    {
        UINT32 *pData = (UINT32 *)(hALTERA->cardReg.Card.Item[hALTERA->addrDesc[addrSpace].index].I.Mem.dwUserDirectAddr + dwOffset);
        *pData = dtoh32(data); /* Write to the memory mapped range directly */
    }
    else
    {
        ALTERA_ReadWriteBlock(hALTERA, addrSpace, dwOffset, FALSE, &data,
                              sizeof(DWORD), ALTERA_MODE_DWORD);
    }
}

BOOL ALTERA_IntIsEnabled(ALTERA_HANDLE hALTERA)
{
    if (!hALTERA->Int.hThread)
        return FALSE;
    return TRUE;
}

void DLLCALLCONV ALTERA_IntHandler(PVOID pData)
{
    ALTERA_HANDLE hALTERA = (ALTERA_HANDLE)pData;
    ALTERA_INT_RESULT intResult;
    intResult.dwCounter = hALTERA->Int.Int.dwCounter;
    intResult.dwLost = hALTERA->Int.Int.dwLost;
    intResult.fStopped = hALTERA->Int.Int.fStopped;
    hALTERA->Int.funcIntHandler(hALTERA, &intResult);
}

BOOL ALTERA_IntEnable(ALTERA_HANDLE hALTERA, ALTERA_INT_HANDLER funcIntHandler)
{
    ALTERA_ADDR addrSpace;
    DWORD dwStatus;

    /* Check if interrupt is already enabled */
    if (hALTERA->Int.hThread)
        return FALSE;

    BZERO(hALTERA->Int.Trans);
    /*
     * This is a sample of handling interrupts:
     * One transfer command is issued to CANCEL the source of the interrupt,
     * otherwise, the PC will hang when an interrupt occurs!
     * You will need to modify this code to fit your specific hardware.
     */
    addrSpace = ALTERA_AD_BAR0; /* Put the address space of the register here */
    if (hALTERA->addrDesc[addrSpace].fIsMemory)
    {
        hALTERA->Int.Trans[0].dwPort = hALTERA->cardReg.Card.Item[hALTERA->addrDesc[addrSpace].index].I.Mem.dwTransAddr;
        hALTERA->Int.Trans[0].cmdTrans = WM_DWORD;
    }
    else
    {
        hALTERA->Int.Trans[0].dwPort = hALTERA->cardReg.Card.Item[hALTERA->addrDesc[addrSpace].index].I.IO.dwAddr;
        hALTERA->Int.Trans[0].cmdTrans = WP_DWORD;
    }
    hALTERA->Int.Trans[0].dwPort += 0; /* Put the offset of the register from
                                          the beginning of the address space
                                          here */
    hALTERA->Int.Trans[0].Data.Dword = 0x0;
    hALTERA->Int.Int.dwCmds = 1;
    hALTERA->Int.Int.Cmd = hALTERA->Int.Trans;
    hALTERA->Int.Int.dwOptions |= INTERRUPT_CMD_COPY;

    /* This calls WD_IntEnable() and creates an interrupt handler thread */
    hALTERA->Int.funcIntHandler = funcIntHandler;
    dwStatus = InterruptEnable(&hALTERA->Int.hThread, hALTERA->hWD,
                               &hALTERA->Int.Int, ALTERA_IntHandler, (PVOID) hALTERA);
    if (dwStatus)
    {
        sprintf(ALTERA_ErrorString, "InterruptEnable() failed with status "
                "0x%lx - %s\n", dwStatus, Stat2Str(dwStatus));
        return FALSE;
    }

    return TRUE;
}

void ALTERA_IntDisable(ALTERA_HANDLE hALTERA)
{
    if (!hALTERA->Int.hThread)
        return;

    /* This calls WD_IntDisable() */
    InterruptDisable(hALTERA->Int.hThread);

    hALTERA->Int.hThread = NULL;
}

/* TODO: through away the WD_Sleep() call */
BOOL ALTERA_DMAWait(ALTERA_HANDLE hALTERA)
{
    BOOL fOk = FALSE;
    DWORD i = 10 * 1000 * 1000 / 2; /* Wait 10 seconds (each loop waits 2 usecs) */
    for (; i; i--)
    {
        WD_SLEEP sleep = {2, 0}; /* 2 microseconds */
        DWORD dwDMAISR = ALTERA_ReadDword(hALTERA, ALTERA_AD_BAR0,
                                          ALTERA_REG_DMAISR);
        if (dwDMAISR & TX_COMP)
        {
            fOk = TRUE;
            break;
        }

        if (dwDMAISR & ERROR_PENDING)
        {
            sprintf(ALTERA_ErrorString, "hardware dma failure\n");
            break;
        }
        WD_Sleep(hALTERA->hWD, &sleep);
    }
    if (!i)
        sprintf(ALTERA_ErrorString, "dma transfer timeout\n");

    return fOk;
}

BOOL ALTERA_DMALock(ALTERA_HANDLE hALTERA, PVOID pBuffer, DWORD dwBytes,
                    BOOL fFromDev, WD_DMA *pDma)
{
    DWORD dwStatus;

    /* Perform Scatter/Gather DMA: */
    BZERO(*pDma);
    pDma->pUserAddr = pBuffer;
    pDma->dwBytes = dwBytes;
    pDma->dwOptions = fFromDev ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
    pDma->hCard = hALTERA->cardReg.hCard;
    dwStatus = WD_DMALock(hALTERA->hWD, pDma);
    if (dwStatus)
    {
        sprintf(ALTERA_ErrorString, "cannot lock down buffer. "
                "Status 0x%lx - %s\n", dwStatus, Stat2Str(dwStatus));
        return FALSE;
    }
    return TRUE;
}

BOOL ALTERA_ContinueDMALock(ALTERA_HANDLE hALTERA, PVOID pBuffer, DWORD dwBytes,
                            BOOL fFromDev, WD_DMA *pDma)
{
    DWORD dwStatus;

    /* Perform Scatter/Gather DMA: */
    BZERO(*pDma);
    pDma->pUserAddr = pBuffer;
    pDma->dwBytes = dwBytes;
    pDma->dwOptions = DMA_KERNEL_BUFFER_ALLOC | (fFromDev ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
    pDma->hCard = hALTERA->cardReg.hCard;
    dwStatus = WD_DMALock(hALTERA->hWD, pDma);
    if (dwStatus)
    {
        sprintf(ALTERA_ErrorString, "cannot lock down buffer. "
                "Status 0x%lx - %s\n", dwStatus, Stat2Str(dwStatus));
        return FALSE;
    }
    return TRUE;
}

void ALTERA_DMAUnlock(ALTERA_HANDLE hALTERA, WD_DMA *pDma)
{
    WD_DMAUnlock(hALTERA->hWD, pDma);
}

BOOL ALTERA_DMAChainTransfer(ALTERA_HANDLE hALTERA, DWORD dwLocalAddr,
                             BOOL fFromDev, DWORD dwBytes, WD_DMA *pDma)
{
    UINT32 DMACsr;
    DWORD i;
    if (pDma->dwPages > ALTERA_DMA_FIFO_REGS)
    {
        sprintf(ALTERA_ErrorString, "too many pages for chain transfer\n");
        return FALSE;
    }

    DMACsr = (fFromDev ? DIRECTION : 0)
             | DMA_ENABLE | TRXCOMPINTDIS | INTERRUPT_ENABLE
             | CHAINEN;

    for (i = 0; i < pDma->dwPages; i++)
    {
        ALTERA_WriteDword(hALTERA, ALTERA_AD_BAR0, FIFO_BASE_OFFSET,
                          (DWORD) pDma->Page[i].pPhysicalAddr);
        ALTERA_WriteDword(hALTERA, ALTERA_AD_BAR0, FIFO_BASE_OFFSET, pDma->Page[i].dwBytes);
    }
    ALTERA_WriteDword(hALTERA, ALTERA_AD_BAR0, ALTERA_REG_DMALAR, dwLocalAddr);
    ALTERA_WriteDword(hALTERA, ALTERA_AD_BAR0, ALTERA_REG_DMACSR, DMACsr); /* this starts the dma */

    return ALTERA_DMAWait(hALTERA);
}

BOOL ALTERA_DMABlockTransfer(ALTERA_HANDLE hALTERA, DWORD dwLocalAddr,
                             BOOL fFromDev, DWORD dwBytes, WD_DMA *pDma)
{
    DWORD i, dwTotalBytes = 0;
    UINT32 DMACsr;

    DMACsr = (fFromDev ? DIRECTION : 0)
             | DMA_ENABLE | TRXCOMPINTDIS | INTERRUPT_ENABLE;

    ALTERA_WriteDword(hALTERA, ALTERA_AD_BAR0, ALTERA_REG_DMACSR, DMACsr);
    for (i = 0; i < pDma->dwPages; i++)
    {
        ALTERA_WriteDword(hALTERA, ALTERA_AD_BAR0, ALTERA_REG_DMALAR,
                          dwLocalAddr + dwTotalBytes);
        ALTERA_WriteDword(hALTERA, ALTERA_AD_BAR0, ALTERA_REG_DMABCR,
                          pDma->Page[i].dwBytes);
        ALTERA_WriteDword(hALTERA, ALTERA_AD_BAR0, ALTERA_REG_DMAACR,
                          (DWORD) pDma->Page[i].pPhysicalAddr); /* this starts the dma */
        if (!ALTERA_DMAWait(hALTERA))
            return FALSE;
        dwTotalBytes += pDma->Page[i].dwBytes;
    }
    return TRUE;
}

BOOL ALTERA_DMAReadWriteBlock(ALTERA_HANDLE hALTERA, DWORD dwLocalAddr,
                              PVOID pBuffer, BOOL fFromDev, DWORD dwBytes, BOOL fChained)
{
    BOOL fOk = FALSE;
    WD_DMA dma;

    if (dwBytes == 0)
        return FALSE;

    if (!ALTERA_DMALock(hALTERA, pBuffer, dwBytes, fFromDev, &dma))
        return FALSE;

    if (fChained)
    {
        fOk = ALTERA_DMAChainTransfer(hALTERA, dwLocalAddr, fFromDev, dwBytes,
                                      &dma);
    }
    else
    {
        fOk = ALTERA_DMABlockTransfer(hALTERA, dwLocalAddr, fFromDev, dwBytes,
                                      &dma);
    }

    ALTERA_DMAUnlock(hALTERA, &dma);
    return fOk;
}
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
BOOL DeviceFindAndOpen(ALTERA_HANDLE *phAltera, DWORD dwVendorID, DWORD dwDeviceID) {

    ALTERA_HANDLE hAltera = (ALTERA_HANDLE)malloc(sizeof(ALTERA_STRUCT));
    // *phAltera = NULL;
    //BZERO(*hAltera);
    DWORD dwCardNum = ALTERA_CountCards(dwVendorID, dwDeviceID);
    if (dwCardNum == 0) {
        printf("No cards found.\n");
        return FALSE;
    }
    BOOL status = ALTERA_Open(&hAltera, dwVendorID, dwDeviceID, dwCardNum - 1);
    if (!status) {
        printf("Fail to open the device.\n");
        return FALSE;
    }
    printf("Bus No. is %2ld, slot is %2ld, and function is %2ld\n",
           hAltera->pciSlot.dwBus, hAltera->pciSlot.dwSlot, hAltera->pciSlot.dwFunction);
    printf("BAR0 addr is 0x%1X, size is 0x%1X.\n", hAltera->cardReg.Card.Item[hAltera->addrDesc[ALTERA_AD_BAR0].index].I.Mem.dwPhysicalAddr, hAltera->cardReg.Card.Item[hAltera->addrDesc[ALTERA_AD_BAR0].index].I.Mem.dwBytes);
    printf("BAR4 addr is 0x%1X, size is 0x%1X\n", hAltera->cardReg.Card.Item[hAltera->addrDesc[ALTERA_AD_BAR4].index].I.Mem.dwPhysicalAddr, hAltera->cardReg.Card.Item[hAltera->addrDesc[ALTERA_AD_BAR4].index].I.Mem.dwBytes);
    *phAltera = hAltera;
    return TRUE;
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
    return 0;
}

BOOL SetDMADescController(WDC_DEVICE_HANDLE hDev, DMA_ADDR desc_table_start_addr, BOOL fromDev) {
    DWORD dt_phy_addr_h = (desc_table_start_addr >> 32) & 0xffffffff;
    DWORD dt_phy_addr_l = desc_table_start_addr & 0xffffffff;

    // if fromDev  is false, it is DMA read operation, movin data from CPU to FPGA
    if (!fromDev) {
        // Program the address of descriptor table to DMA descriptor controller
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_RC_HIGH_SRC_ADDR, dt_phy_addr_h);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_RC_LOW_SRC_ADDR, dt_phy_addr_l);

        // Program the on-chip FIFO address to DMA Descriptor Controller, This is the address to which the DMA
        // Descriptor Controller will copy the status and descriptor table from CPU
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTRL_HIGH_DEST_ADDR, RD_CTRL_BUF_BASE_LOW);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTLR_LOW_DEST_ADDR, RD_CTRL_BUF_BASE_LOW);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_TABLE_SIZE, (ALTERA_DMA_DESCRIPTOR_NUM - 1));
    }
    else {
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_RC_HIGH_SRC_ADDR, dt_phy_addr_h);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_RC_LOW_SRC_ADDR, dt_phy_addr_l);

        // Program the on-chip FIFO address to DMA Descriptor Controller, This is the address to which the DMA
        // Descriptor Controller will copy the status and descriptor table from CPU
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_CTRL_HIGH_DEST_ADDR, WR_CTRL_BUF_BASE_LOW);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_CTLR_LOW_DEST_ADDR, WR_CTRL_BUF_BASE_LOW);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_TABLE_SIZE, (ALTERA_DMA_DESCRIPTOR_NUM-1));

    }
    return TRUE;
    // Start DMA
    //ALTERA_WriteDWord(phAltera, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR,0);

}

// Initialize the descriptor table header
static int set_lite_table_header(struct lite_dma_header *header) {
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

DWORD InitDMABookkeep(WDC_DEVICE_HANDLE hDev, WD_DMA **ppDma, WD_DMA **ppDma_wr,  WD_DMA **ppDMA_rd_buf, WD_DMA **ppDMA_wr_buf) {
    printf("altera_pcie_dma_bookkeep size is %d.\n", sizeof(struct altera_pcie_dma_bookkeep));
    bk_ptr1 = (struct altera_pcie_dma_bookkeep *) malloc(sizeof(struct altera_pcie_dma_bookkeep));
    DWORD status = WDC_DMASGBufLock(hDev, bk_ptr1, DMA_TO_DEVICE, sizeof(struct altera_pcie_dma_bookkeep), ppDma);
    // Because of the size of structure altera_pcie_dma_bookkeep is over than 4KB page size, it needs to use ContigBufLock to apply space for altera_pcie_dma_bookkeep
    // DWORD status = WDC_DMAContigBufLock(hDev, bk_ptr1, DMA_TO_DEVICE, sizeof(struct altera_pcie_dma_bookkeep), ppDma);
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to initiate DMAContigBuf.\n");
    }
    printf("DMA page number is %d.\n", (*ppDma)->dwPages);

    status = WDC_DMASyncCpu(*ppDma);
    if (WD_STATUS_SUCCESS != status) {
        printf("Fail to CPUSync ppDMA.\n");
    }
    status = WDC_DMASGBufLock(hDev, &(bk_ptr1->lite_table_wr_cpu_virt_addr), DMA_TO_DEVICE, sizeof(struct altera_pcie_dma_bookkeep), ppDma_wr);
    // Because of the size of structure altera_pcie_dma_bookkeep is over than 4KB page size, it needs to use ContigBufLock to apply space for altera_pcie_dma_bookkeep
    // DWORD status = WDC_DMAContigBufLock(hDev, bk_ptr1, DMA_TO_DEVICE, sizeof(struct altera_pcie_dma_bookkeep), ppDma);
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to initiate DMAContigBuf for ppDma_wr.\n");
    }
    printf("DMA page number in wr is %d.\n", (*ppDma_wr)->dwPages);

    status = WDC_DMASyncCpu(*ppDma_wr);
    if (WD_STATUS_SUCCESS != status) {
        printf("Fail to CPUSync ppDMA_wr.\n");
    }

    //bk_ptr1->dma_status.altera_dma_num_dwords = ALTERA_DMA_NUM_DWORDS;
    bk_ptr1->dma_status.altera_dma_descriptor_num = ALTERA_DMA_DESCRIPTOR_NUM;
    bk_ptr1->dma_status.run_write = 1;
    bk_ptr1->dma_status.run_read = 1;
    bk_ptr1->dma_status.run_simul = 1;
    bk_ptr1->dma_status.offset = 0;
    bk_ptr1->dma_status.onchip = 1;
    bk_ptr1->dma_status.rand = 0;
    bk_ptr1->numpages = (PAGE_SIZE >= MAX_NUM_DWORDS * 4) ? 1 : (int)((MAX_NUM_DWORDS * 4) / PAGE_SIZE);

    set_lite_table_header(&(bk_ptr1->lite_table_rd_cpu_virt_addr.header));
    set_lite_table_header(&(bk_ptr1->lite_table_wr_cpu_virt_addr.header));

    bk_ptr1->lite_table_rd_bus_addr = (*ppDma)->Page[0].pPhysicalAddr;
    bk_ptr1->lite_table_wr_bus_addr = (*ppDma_wr)->Page[0].pPhysicalAddr;

    // DMA read operation, moving data from host to Device. Apply space for rd buffer
    /*
    rp_rd_buffer = (DWORD *)malloc(RP_RD_BUFFER_SZIE);
     init_rp_mem(rp_rd_buffer,RP_RD_BUFFER_SZIE/4);
    status = WDC_DMASGBufLock(hDev, rp_rd_buffer, DMA_TO_DEVICE, RP_RD_BUFFER_SZIE, ppDMA_rd_buf);
    */
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
    bk_ptr1->dma_status.altera_dma_num_dwords = (*ppDMA_rd_buf)->Page[0].dwBytes;
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

    (bk_ptr1->rp_rd_buffer_virt_addr) = (*ppDMA_rd_buf)->pUserAddr;
    (bk_ptr1->rp_rd_buffer_bus_addr) = (*ppDMA_rd_buf)->Page[0].pPhysicalAddr;
    (bk_ptr1->rp_wr_buffer_virt_addr) = (*ppDMA_wr_buf)->pUserAddr;
    (bk_ptr1->rp_wr_buffer_bus_addr) = (*ppDMA_wr_buf)->Page[0].pPhysicalAddr;
    return status;
}

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
        /*
        DWORD rd_buf_phy_addr_h = (bk_ptr1->rp_rd_buffer_bus_addr >> 32) & 0xffffffff;
        DWORD rd_buf_phy_addr_l = bk_ptr1->rp_rd_buffer_bus_addr & 0xffffffff;
        for (int i = 0; i < ALTERA_DMA_DESCRIPTOR_NUM; i++) {
            SetDescTable(&(bk_ptr1->lite_table_rd_cpu_virt_addr.descriptors[i]), rd_buf_phy_addr_h, rd_buf_phy_addr_l, ONCHIP_MEM_BASE_ADDR_HI, ONCHIP_MEM_BASE_ADDR_LOW, bk_ptr1->dma_status.altera_dma_num_dwords, i);
            }
            */

        DMA_ADDR onchip_mem_base_addr = (ONCHIP_MEM_BASE_ADDR_HI << 32) + ONCHIP_MEM_BASE_ADDR_LOW;
        DMA_ADDR onchip_mem_start_addr;
        DWORD current_page_size, pre_page_size;
        // Construct the descriptor table
        for (int i = 0; i < ALTERA_DMA_DESCRIPTOR_NUM; i++) {
            (bk_ptr1->rp_rd_buffer_bus_addr) = ppDMA_rd_buf->Page[i].pPhysicalAddr;  // Buffer's physical address
            DWORD rd_buf_phy_addr_h = (bk_ptr1->rp_rd_buffer_bus_addr >> 32) & 0xffffffff;
            DWORD rd_buf_phy_addr_l = bk_ptr1->rp_rd_buffer_bus_addr & 0xffffffff;

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

            SetDescTable(&(bk_ptr1->lite_table_rd_cpu_virt_addr.descriptors[i]), rd_buf_phy_addr_h, rd_buf_phy_addr_l, onchip_mem_addr_h, onchip_mem_addr_l, dma_dword, i);
        }

        WDC_ReadAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR, &last_id);

        if (last_id == 0xff) {
            SetDMADescController(hDev, bk_ptr1->lite_table_rd_bus_addr, fromDev);
            last_id = ALTERA_DMA_DESCRIPTOR_NUM - 1;
        }
        last_id = ALTERA_DMA_DESCRIPTOR_NUM - 1;
        /*
        last_id = last_id + ALTERA_DMA_DESCRIPTOR_NUM;
        //Over DMA request
        if (last_id > (ALTERA_DMA_DESCRIPTOR_NUM - 1)) {
            last_id = last_id - ALTERA_DMA_DESCRIPTOR_NUM;
            if ((bk_ptr1->dma_status.altera_dma_descriptor_num > 1) && (last_id != (ALTERA_DMA_DESCRIPTOR_NUM - 1))) write_127 = 1;
        }
        if (write_127)
            WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR, (ALTERA_DMA_DESCRIPTOR_NUM - 1));
            */
        SetDMADescController(hDev, bk_ptr1->lite_table_rd_bus_addr, fromDev);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR, last_id);
        printf("Last_id is %d.\n", last_id);
        timeout = TIMEOUT;
        while (1) {
            if (bk_ptr1->lite_table_rd_cpu_virt_addr.header.flags[last_id]) {
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
        WDC_DMASyncIo(ppDMA_wr);
        WDC_DMABufUnlock(ppDMA_wr);
        WDC_DMASyncIo(ppDMA_rd_buf);
        WDC_DMABufUnlock(ppDMA_rd_buf);
        WDC_DMASyncIo(ppDMA_wr_buf);
        WDC_DMABufUnlock(ppDMA_wr_buf);
        free(rp_rd_buffer);
        free(ep_wr_buffer);
        free(bk_ptr1);
        close_pci(hDev);
        return TRUE;
    }
    else {
        DWORD wr_buf_phy_addr_h = (bk_ptr1->rp_wr_buffer_bus_addr >> 32) & 0xffffffff;
        DWORD wr_buf_phy_addr_l = bk_ptr1->rp_wr_buffer_bus_addr & 0xffffffff;
        for (int i = 0; i < ALTERA_DMA_DESCRIPTOR_NUM; i++) {
            SetDescTable(&(bk_ptr1->lite_table_wr_cpu_virt_addr.descriptors[i]), DDR_MEM_BASE_ADDR_HI, DDR_MEM_BASE_ADDR_LOW, wr_buf_phy_addr_h, wr_buf_phy_addr_l, bk_ptr1->dma_status.altera_dma_num_dwords, i);
            }
        WDC_ReadAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_LAST_PTR, &last_id);

        if (last_id == 0xff) {
            SetDMADescController(hDev, bk_ptr1->lite_table_wr_bus_addr, fromDev);
            last_id = ALTERA_DMA_DESCRIPTOR_NUM - 1;
        }

        last_id = last_id + ALTERA_DMA_DESCRIPTOR_NUM;
        //Over DMA request
        if (last_id > (ALTERA_DMA_DESCRIPTOR_NUM - 1)) {
            last_id = last_id - ALTERA_DMA_DESCRIPTOR_NUM;
            if ((bk_ptr1->dma_status.altera_dma_descriptor_num > 1) && (last_id != (ALTERA_DMA_DESCRIPTOR_NUM - 1))) write_127 = 1;
        }
        if (write_127)
            WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_LAST_PTR, (ALTERA_DMA_DESCRIPTOR_NUM - 1));
        SetDMADescController(hDev, bk_ptr1->lite_table_wr_bus_addr, fromDev);
        WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_LAST_PTR, last_id);
        printf("Last_id is %d.\n", last_id);
        timeout = TIMEOUT;
        while (1) {
            if (bk_ptr1->lite_table_wr_cpu_virt_addr.header.flags[last_id]) {
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
        free(bk_ptr1);
        //WDC_PciDeviceClose(hDev);
        close_pci(hDev);
        return TRUE;
    }
    return TRUE;
}