/* Jungo Confidential. Copyright (c) 2010 Jungo Ltd.  http://www.jungo.com */

/*
 * altera_lib.c
 *
 * Library for accessing the ALTERA card.
 * Accesses the hardware via WinDriver API functions.
 */

#include "altera_lib.h"
#include "windrvr_int_thread.h"
#include "status_strings.h"
#include <stdio.h>

#define PCI_DRIVER_DEFAULT_DRIVER_NAME "windrvr6"
#define PCI_DRIVER_DEFAULT_LICENSE_STRING "6C3CC2CFE89E7AD0424A070D434A6F6DC49520ED.wps"

#define RP_RD_BUFFER_SZIE ALTERA_DMA_NUM_DWORDS
#define EP_WR_BUFFER_SZIE ALTERA_DMA_NUM_DWORDS
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

struct altera_pcie_dma_bookkeep *bk_ptr1;
DWORD *rp_rd_buffer;
DWORD *ep_wr_buffer;

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

static CHAR gsPCI_DRIVER_LastErr[256];

static DWORD LibInit_count = 0;
static inline BOOL IsValidDevice(PWDC_DEVICE pDev, const CHAR *sFunc)
{
    if (!pDev || !WDC_GetDevContext(pDev))
    {
        snprintf(gsPCI_DRIVER_LastErr, sizeof(gsPCI_DRIVER_LastErr) - 1, "%s: NULL device %s\n",
            sFunc, !pDev ? "handle" : "context");
        //printf(gsPCI_DRIVER_LastErr);
        return FALSE;
    }

    return TRUE;
}

static BOOL DeviceValidate(const PWDC_DEVICE pDev)
{
    DWORD i, dwNumAddrSpaces = pDev->dwNumAddrSpaces;

    /* TODO: You can modify the implementation of this function in order to
    verify that the device has all expected resources. */

    /* Verify that the device has at least one active address space */
    for (i = 0; i < dwNumAddrSpaces; i++)
    {
        if (WDC_AddrSpaceIsActive(pDev, i))
            return TRUE;
    }

    printf("Device does not have any active memory or I/O address spaces\n");
    return TRUE;
}

WDC_DEVICE_HANDLE PCI_DRIVER_DeviceOpen( const WD_PCI_CARD_INFO *pDeviceInfo)
{
    WDC_DEVICE_HANDLE hDev;
    DWORD dwStatus;
    WDC_DEVICE * pDevCtx = NULL;
    //WDC_DEVICE_HANDLE hDev = NULL;

    /* Validate arguments */
    if (!pDeviceInfo)
    {
        printf("PCI_DRIVER_DeviceOpen: Error - NULL device information struct pointer\n");
        return NULL;
    }

    /* Allocate memory for the PCI_DRIVER device context */

    pDevCtx = malloc(sizeof(WDC_DEVICE));
    /*
    if (!pDevCtx)
    {
        printf("Failed allocating memory for PCI_DRIVER device context\n");
        return NULL;
    }
    */
   // BZERO(*pDevCtx);

    /* Open a WDC device handle */
    dwStatus = WDC_PciDeviceOpen(&hDev, pDeviceInfo, pDevCtx, NULL, NULL, NULL);

    if (WD_STATUS_SUCCESS != dwStatus)
    {
        //printf("Failed opening a WDC device handle. Error 0x%lx - %s\n",
         //   dwStatus, Stat2Str(dwStatus));
        goto Error;
    }

    /* Validate device information */
    if (!DeviceValidate((PWDC_DEVICE)hDev))
        goto Error;

    /* Return handle to the new device */
    printf("PCI_DRIVER_DeviceOpen: Opened a PCI_DRIVER device (handle 0x%p)\n", hDev);
    return hDev;

Error:
    /*
    if (hDev)
        PCI_DRIVER_DeviceClose(hDev);
    else
        free(pDevCtx);
        */
    return NULL;
}

DWORD PCI_DRIVER_LibInit(void)
{
    DWORD dwStatus;

    /* init only once */
    if (++LibInit_count > 1)
        return WD_STATUS_SUCCESS;

#if defined(WD_DRIVER_NAME_CHANGE)
    /* Set the driver name */
    if (!WD_DriverName(PCI_DRIVER_DEFAULT_DRIVER_NAME))
    {
        printf("Failed to set the driver name for WDC library.\n");
        return WD_SYSTEM_INTERNAL_ERROR;
    }
#endif

    /* Set WDC library's debug options (default: level TRACE, output to Debug Monitor) */
    dwStatus = WDC_SetDebugOptions(WDC_DBG_DEFAULT, NULL);
    if (WD_STATUS_SUCCESS != dwStatus)
    {
        printf("Failed to initialize debug options for WDC library.\n"
            "Error 0x%lx - %s\n", dwStatus, Stat2Str(dwStatus));

        return dwStatus;
    }

    /* Open a handle to the driver and initialize the WDC library */
    dwStatus = WDC_DriverOpen(WDC_DRV_OPEN_DEFAULT, PCI_DRIVER_DEFAULT_LICENSE_STRING);
    if (WD_STATUS_SUCCESS != dwStatus)
    {
        printf("Failed to initialize the WDC library. Error 0x%lx - %s\n",
            dwStatus, Stat2Str(dwStatus));

        return dwStatus;
    }

    return WD_STATUS_SUCCESS;
}

WDC_DEVICE_HANDLE initialize_PCI( DWORD VENDOR_ID, DWORD DEVICE_ID) {
    WDC_PCI_SCAN_RESULT scanResult;
    WD_PCI_CARD_INFO    deviceInfo;
    DWORD  dwStatus = PCI_DRIVER_LibInit();
    if (WD_STATUS_SUCCESS != dwStatus)
    {
        printf("Fail to initiate driver library.\n");
//        return 3;
    }

    dwStatus = WDC_PciScanDevices(VENDOR_ID, DEVICE_ID, &scanResult);
    if (WD_STATUS_SUCCESS != dwStatus)
    {
        WDC_Err("hu: Failed to scan the PCI driver. Error 0x%lx - %s\n", dwStatus, Stat2Str(dwStatus));
      //  return 1;
    }
    //dwNumDevices = scanResult.dwNumDevices;//## 搜索到的设备个数
    deviceInfo.pciSlot = scanResult.deviceSlot[0];//scanResult.dwNumDevices  - 1
    WDC_PciGetDeviceInfo(&deviceInfo);

    WDC_DEVICE_HANDLE  hDev = PCI_DRIVER_DeviceOpen(&deviceInfo);
    return hDev;

    // return 4;

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

BOOL SetDesc(struct dma_descriptor *dma_desc, DWORD source_addr_high, DWORD source_addr_low, DWORD dest_addr_high, DWORD dest_addr_low, DWORD ctl_dma_len, WORD id) {
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

DWORD InitDMABookkeep(WDC_DEVICE_HANDLE hDev, WD_DMA **ppDma, WD_DMA **ppDma_wr, WD_DMA **ppDMA_rd_buf, WD_DMA **ppDMA_wr_buf) {
    //struct altera_pcie_dma_bookkeep *bk_ptr = NULL;
    printf("altera_pcie_dma_bookkeep size is %d.\n", sizeof(struct altera_pcie_dma_bookkeep));
    bk_ptr1 = (struct altera_pcie_dma_bookkeep *) malloc(sizeof(struct altera_pcie_dma_bookkeep));
    //BZERO(bk_ptr);
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

    //BZERO(bk_ptr);
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

    bk_ptr1->dma_status.altera_dma_num_dwords = ALTERA_DMA_NUM_DWORDS;
    bk_ptr1->dma_status.altera_dma_descriptor_num = ALTERA_DMA_DESCRIPTOR_NUM;
    bk_ptr1->dma_status.run_write = 1;
    bk_ptr1->dma_status.run_read = 1;
    bk_ptr1->dma_status.run_simul = 1;
    bk_ptr1->dma_status.offset = 0;
    bk_ptr1->dma_status.onchip = 1;
    bk_ptr1->dma_status.rand = 0;

    // How to set PAGE_SIZE
    bk_ptr1->numpages = (PAGE_SIZE >= MAX_NUM_DWORDS * 4) ? 1 : (int)((MAX_NUM_DWORDS * 4) / PAGE_SIZE);

    set_lite_table_header(&(bk_ptr1->lite_table_rd_cpu_virt_addr.header));
    set_lite_table_header(&(bk_ptr1->lite_table_wr_cpu_virt_addr.header));

    bk_ptr1->lite_table_rd_bus_addr = (*ppDma)->Page[0].pPhysicalAddr;
    bk_ptr1->lite_table_wr_bus_addr = (*ppDma_wr)->Page[0].pPhysicalAddr;

    // DMA read operation, moving data from host to Device. Apply space for rd buffer
    rp_rd_buffer = (DWORD *)malloc(RP_RD_BUFFER_SZIE);
     init_rp_mem(rp_rd_buffer,RP_RD_BUFFER_SZIE/4);
    status = WDC_DMASGBufLock(hDev, rp_rd_buffer, DMA_TO_DEVICE, RP_RD_BUFFER_SZIE, ppDMA_rd_buf);
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to initiate DMAContigBuf for rd_buf.\n");
    }
    status = WDC_DMASyncCpu(*ppDMA_rd_buf);
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to CPUSync ppDMA.\n");
    }

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
    //free(bk_ptr1);
   // free(rp_rd_buffer);
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
/* DMAOpen: Locks a Scatter/Gather DMA buffer */
BOOL DMAOpen (WDC_DEVICE_HANDLE hDev, PVOID *ppBuf, DWORD dwDMABufSize, BOOL fToDev, WD_DMA **ppDMA) {

    DWORD dwStatus;
    DWORD dwOptions = fToDev ? DMA_TO_DEVICE : DMA_FROM_DEVICE;
    /* Lock a Scatter/Gather DMA buffer */
    dwStatus = WDC_DMASGBufLock(hDev, ppBuf, dwOptions, dwDMABufSize, ppDMA);
    if (WD_STATUS_SUCCESS != dwStatus)
    {
        printf("Failed locking a Scatter/Gather DMA buffer. Error 0x%lx - %s\n",
               dwStatus, Stat2Str(dwStatus));
        return FALSE;
    }
    /* Program the device's DMA registers for each physical page */
//    MyDMAProgram((*ppDma)->Page, (*ppDma)->dwPages, fToDev);
    return TRUE;
}
// DMA READ -> Move data from CPU to FPGA
BOOL ALTERA_DMABlock(WDC_DEVICE_HANDLE hDev, ALTERA_HANDLE hALTERA, BOOL fromDev) {

    BOOL status;
    //struct altera_pcie_dma_bookkeep *bk_ptr1;
    DWORD last_id, write_127 = 0;
    DWORD timeout;
    WD_DMA *ppDMA = NULL, *ppDMA_wr = NULL, *ppDMA_rd_buf= NULL, *ppDMA_wr_buf=NULL;
    PVOID pBuf = NULL;
    //pDMA = (WD_DMA *)(malloc(sizeof(WD_DMA)));
    status = InitDMABookkeep(hDev, &ppDMA, &ppDMA_wr,  &ppDMA_rd_buf, &ppDMA_wr_buf);
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to initiate DMA bookkeep.\n");
    }

    if (!fromDev) {
        DWORD rd_buf_phy_addr_h = (bk_ptr1->rp_rd_buffer_bus_addr >> 32) & 0xffffffff;
        DWORD rd_buf_phy_addr_l = bk_ptr1->rp_rd_buffer_bus_addr & 0xffffffff;
        for (int i = 0; i < ALTERA_DMA_DESCRIPTOR_NUM; i++) {
            SetDesc(&(bk_ptr1->lite_table_rd_cpu_virt_addr.descriptors[i]), rd_buf_phy_addr_h, rd_buf_phy_addr_l, DDR_MEM_BASE_ADDR_HI, DDR_MEM_BASE_ADDR_LOW, bk_ptr1->dma_status.altera_dma_num_dwords, i);
            }
        WDC_ReadAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR, &last_id);

        if (last_id == 0xff) {
            SetDMADescController(hDev, bk_ptr1->lite_table_rd_bus_addr, fromDev);
            last_id = ALTERA_DMA_DESCRIPTOR_NUM - 1;
        }

        last_id = last_id + ALTERA_DMA_DESCRIPTOR_NUM;
        //Over DMA request
        if (last_id > (ALTERA_DMA_DESCRIPTOR_NUM - 1)) {
            last_id = last_id - ALTERA_DMA_DESCRIPTOR_NUM;
            if ((bk_ptr1->dma_status.altera_dma_descriptor_num > 1) && (last_id != (ALTERA_DMA_DESCRIPTOR_NUM - 1))) write_127 = 1;
        }
        if (write_127)
            WDC_WriteAddr32(hDev, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR, (ALTERA_DMA_DESCRIPTOR_NUM - 1));
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
        return TRUE;
    }

    else {

        DWORD wr_buf_phy_addr_h = (bk_ptr1->rp_wr_buffer_bus_addr >> 32) & 0xffffffff;
        DWORD wr_buf_phy_addr_l = bk_ptr1->rp_wr_buffer_bus_addr & 0xffffffff;
        for (int i = 0; i < ALTERA_DMA_DESCRIPTOR_NUM; i++) {
            SetDesc(&(bk_ptr1->lite_table_wr_cpu_virt_addr.descriptors[i]), DDR_MEM_BASE_ADDR_HI, DDR_MEM_BASE_ADDR_LOW, wr_buf_phy_addr_h, wr_buf_phy_addr_l, bk_ptr1->dma_status.altera_dma_num_dwords, i);
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
        return TRUE;
    }
    return TRUE;
}