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
            trans.cmdTrans = fMem ? RM_SDWORD : RP_SDWORD;
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
/*
struct dma_descriptor *SetDescTable(ALTERA_HANDLE *phAltera, struct dma_descriptor *dma_desc_table_ptr, BYTE *buffer_virt_addr, DWORD dwSize) {

    dma_desc_table_ptr = (struct dma_desc_table*)malloc(sizeof(struct dma_descriptor));
    BZERO(dma_desc_table_ptr);

    // READ DMA -> Move data from CPU to FPGA
    // Virtual data buffer address in CPU
    dma_desc_table_ptr->src_addr_low = buffer_virt_addr & 0xffffffff;
    dma_desc_table_ptr->src_addr_high = (buffer_virt_addr >> 32) & 0xffffffff;

    // DDR3 address on FPGA
    dma_desc_table_ptr->des_addr_low = DDR_MEM_BASE_ADDR_LOW;
    dma_desc_table_ptr->descriptor[0].des_addr_high = DDR_MEM_BASE_ADDR_HI;
    // Descriptor ID is 0 and DMA size is 64KB
    des_table_ptr->descriptor[0].ctl_dma_len = dwSize;

    des_table_ptr->descriptor[0].reserved[0] = 0;
    des_table_ptr->descriptor[0].reserved[1] = 0;
    des_table_ptr->descriptor[0].reserved[2] = 0;

    return dma_desc_table_ptr;
}
*/
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

BOOL SetDMADescController(ALTERA_HANDLE phAltera, DWORD desc_table_start_addr, BOOL fromDev) {

    // if fromDev  is false, it is DMA read operation, movin data from CPU to FPGA
    if (!fromDev) {
        // Program the address of descriptor table to DMA descriptor controller
        ALTERA_WriteDword(phAltera, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_RC_LOW_SRC_ADDR, desc_table_start_addr);
        ALTERA_WriteDword(phAltera, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_RC_HIGH_SRC_ADDR, 0);

        // Program the on-chip FIFO address to DMA Descriptor Controller, This is the address to which the DMA
        // Descriptor Controller will copy the status and descriptor table from CPU
        ALTERA_WriteDword(phAltera, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTLR_LOW_DEST_ADDR, RD_CTRL_BUF_BASE_LOW);
        ALTERA_WriteDword(phAltera, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_CTRL_HIGH_DEST_ADDR, RD_CTRL_BUF_BASE_HIGH);
    }
    else {
        ALTERA_WriteDword(phAltera, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_RC_LOW_SRC_ADDR, desc_table_start_addr);
        ALTERA_WriteDword(phAltera, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_RC_HIGH_SRC_ADDR, 0);

        // Program the on-chip FIFO address to DMA Descriptor Controller, This is the address to which the DMA
        // Descriptor Controller will copy the status and descriptor table from CPU
        ALTERA_WriteDword(phAltera, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_CTLR_LOW_DEST_ADDR, RD_CTRL_BUF_BASE_LOW);
        ALTERA_WriteDword(phAltera, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_CTRL_HIGH_DEST_ADDR, RD_CTRL_BUF_BASE_HIGH);
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

struct altera_pcie_dma_bookkeep *InitDMABookkeep() {
    struct altera_pcie_dma_bookkeep *bk_ptr;
    bk_ptr = (struct altera_pcie_dma_bookkeep *)malloc(sizeof(struct altera_pcie_dma_bookkeep));
    BZERO(*bk_ptr);
    bk_ptr->dma_status.altera_dma_num_dwords = ALTERA_DMA_NUM_DWORDS;
    bk_ptr->dma_status.altera_dma_descriptor_num = 1; // Set one descriptor
    bk_ptr->dma_status.run_write = 1;
    bk_ptr->dma_status.run_read = 1;
    bk_ptr->dma_status.run_simul = 1;
    bk_ptr->dma_status.offset = 0;
    bk_ptr->dma_status.onchip = 1;
    bk_ptr->dma_status.rand = 0;

    //set_lite_table_header(bk_ptr->lite_table_rd_cpu_virt_addr->header);
    // How to set PAGE_SIZE
    bk_ptr->numpages = (PAGE_SIZE >= MAX_NUM_DWORDS * 4) ? 1 : (int)((MAX_NUM_DWORDS * 4) / PAGE_SIZE);
    // Set descriptor table address
    bk_ptr->lite_table_rd_cpu_virt_addr = (struct lite_dma_desc_table *)malloc(sizeof(struct lite_dma_desc_table));
    bk_ptr->lite_table_wr_cpu_virt_addr = (struct lite_dma_desc_table *)malloc(sizeof(struct lite_dma_desc_table));
    bk_ptr->lite_table_rd_cpu_virt_addr->header.flags[0] = 0x0;

    bk_ptr->lite_table_rd_bus_addr = (DWORD)bk_ptr->lite_table_rd_cpu_virt_addr;
    bk_ptr->lite_table_wr_bus_addr = (DWORD)bk_ptr->lite_table_wr_cpu_virt_addr;

    // Set read buffer and address
    DWORD *rp_rd_buffer = (DWORD *)malloc(sizeof(BYTE) * PAGE_SIZE * bk_ptr->numpages);
    bk_ptr->rp_rd_buffer_virt_addr = rp_rd_buffer;
    bk_ptr->rp_rd_buffer_bus_addr = (DWORD)rp_rd_buffer;

    DWORD *rp_wr_buffer = (DWORD *)malloc(sizeof(BYTE) * PAGE_SIZE * bk_ptr->numpages);
    bk_ptr->rp_wr_buffer_virt_addr = rp_wr_buffer;
    bk_ptr->rp_wr_buffer_bus_addr = (DWORD)rp_wr_buffer;

    return bk_ptr;

}

WORD init_rp_mem(DWORD *rp_buffer_virt_addr, DWORD num_dword) {
    DWORD i = 0;
    //DWORD increment_value = 0;
    DWORD tmp_rand;
    for (i = 0; i < num_dword; i++) {
        tmp_rand = rand();
        printf("init_rp_mem tmp_rand is 0x%X.\n", tmp_rand);
        *(rp_buffer_virt_addr + i*sizeof(DWORD)) = tmp_rand;
    }
    return 1;
}

WORD init_ep_mem(ALTERA_HANDLE hALTERA, DWORD mem_byte_offset, DWORD num_dwords, BYTE increment) {
    DWORD i = 0;
    DWORD increment_value = 0;
    DWORD tmp_rand;
    for (i = 0; i < num_dwords; i++) {
        if (increment) increment_value = i;
//       iowrite32 (cpu_to_le32(init_value+increment_value), (DWORD *)(bk_ptr->bar[4]+mem_byte_offset)+increment_value);
        //get_random_bytes(&tmp_rand, sizeof(tmp_rand));
        tmp_rand = rand();

        //iowrite32 (cpu_to_le32(tmp_rand), (DWORD *)(bk_ptr->bar[4]+mem_byte_offset)+i);
        ALTERA_WriteDword(hALTERA, ALTERA_AD_BAR4, mem_byte_offset + i*4, tmp_rand);
    }

    return 1;
}

// DMA READ -> Move data from CPU to FPGA
BOOL ALTERA_DMABlock(ALTERA_HANDLE hALTERA, BOOL fromDev) {

    BOOL status = FALSE;
    struct altera_pcie_dma_bookkeep *bk_ptr;
    DWORD last_id, write_127 = 0;
    DWORD timeout;
    bk_ptr = InitDMABookkeep();
    DWORD *rp_rd_buffer_virt_addr;
    rp_rd_buffer_virt_addr = bk_ptr->rp_rd_buffer_virt_addr;
    DWORD *rp_wr_buffer_virt_addr;
    rp_wr_buffer_virt_addr = bk_ptr->rp_wr_buffer_virt_addr;
    WD_DMA *dma;
    dma = (WD_DMA *)malloc(sizeof(WD_DMA));
    //dma->hCard = hALTERA->cardReg.hCard;
    //PVOID pbuffer = malloc(sizeof(BYTE) * PAGE_SIZE * bk_ptr->dma_status.altera_dma_num_dwords*4);
    PVOID pbuffer = malloc(2000);
    // Lock memory for DMA
    if (!ALTERA_DMALock(hALTERA, pbuffer, bk_ptr->dma_status.altera_dma_num_dwords, fromDev, dma))
        return FALSE;
    // Init rd_buffer data
    memset(rp_rd_buffer_virt_addr, bk_ptr->dma_status.altera_dma_num_dwords * 4, sizeof(BYTE));
    init_rp_mem(rp_rd_buffer_virt_addr, bk_ptr->dma_status.altera_dma_num_dwords);
    //Initiate the wr_buffer and rd_buffer data
    memset(rp_wr_buffer_virt_addr, bk_ptr->dma_status.altera_dma_num_dwords * 4, sizeof(BYTE));
    init_rp_mem(rp_wr_buffer_virt_addr, bk_ptr->dma_status.altera_dma_num_dwords);

    // Init DDR Memory
    init_ep_mem(hALTERA, ALTERA_AD_BAR4, bk_ptr->dma_status.altera_dma_num_dwords, 0);

    //Set descriptor[0]
    //BZERO(&bk_ptr->lite_table_rd_cpu_virt_addr->descriptor[0]);
    // DMA read operation
    if (!fromDev) {

        //SetDesc(&bk_ptr->lite_table_rd_cpu_virt_addr->descriptors[0],0x0,  dma->Page[0].pPhysicalAddr, ONCHIP_MEM_BASE_ADDR_HI, ONCHIP_MEM_BASE_ADDR_LOW, 0x00081000, 0); // length is 16KB
        SetDesc(&bk_ptr->lite_table_rd_cpu_virt_addr->descriptors[0], 0x0, dma->Page[0].pPhysicalAddr, DDR_MEM_BASE_ADDR_HI, DDR_MEM_BASE_ADDR_LOW, 0x000010, 0);
        // Get the last DMA request ID. If no DMA request, it will return 0xff
        last_id = ALTERA_ReadDword(hALTERA, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR);
        if (last_id == 0xff) {
            SetDMADescController(hALTERA, (DWORD)bk_ptr->lite_table_rd_bus_addr, fromDev);
            last_id = 127;
        }
        //Update ID
        last_id = last_id + bk_ptr->dma_status.altera_dma_descriptor_num;
        //Over DMA request
        if (last_id > 127) {
            last_id = last_id - 128;
            if ((bk_ptr->dma_status.altera_dma_descriptor_num > 1) && (last_id != 127)) write_127 = 1;
        }
        // Start DMA request
        if (write_127)
            ALTERA_WriteDword(hALTERA, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR, 127);
        ALTERA_WriteDword(hALTERA, ALTERA_AD_BAR0, ALTERA_LITE_DMA_RD_LAST_PTR, last_id);
        printf("Last_id is %d.\n",last_id);
        timeout = TIMEOUT;
        while (1) {
            if (bk_ptr->lite_table_rd_cpu_virt_addr->header.flags[last_id]) {
                break;
            }
            timeout--;
            if (timeout == 0) {
                printf("DMA read operation is timeout.\n");
                break;
            }
        }
        return TRUE;
    }
    else {
        // DMA write operation
        SetDesc(&bk_ptr->lite_table_wr_cpu_virt_addr->descriptors[0], 0x0, (DWORD)dma->Page[0].pPhysicalAddr, ONCHIP_MEM_BASE_ADDR_HI, ONCHIP_MEM_BASE_ADDR_LOW,  0x00081000, 0); // length is 16KB
        // Get the last DMA request ID. If no DMA request, it will return 0xff
        last_id = ALTERA_ReadDword(hALTERA, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_LAST_PTR);
        if (last_id == 0xff) {
            SetDMADescController(hALTERA, (DWORD)bk_ptr->lite_table_rd_cpu_virt_addr, fromDev);
            last_id = 127;
        }
        //Update ID
        last_id = last_id + bk_ptr->dma_status.altera_dma_descriptor_num;
        //Over DMA request
        if (last_id > 127) {
            last_id = last_id - 128;
            if ((bk_ptr->dma_status.altera_dma_descriptor_num > 1) && (last_id != 127)) write_127 = 1;
        }
        // Start DMA request
        if (write_127)
            ALTERA_WriteDword(hALTERA, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_LAST_PTR, 127);
        ALTERA_WriteDword(hALTERA, ALTERA_AD_BAR0, ALTERA_LITE_DMA_WR_LAST_PTR, last_id);
        timeout = TIMEOUT;
        while (1) {
            if (bk_ptr->lite_table_wr_cpu_virt_addr->header.flags[last_id]) {
                break;
            }
            timeout--;
            if (timeout == 0) {
                printf("DMA write operation is timeout.\n");
                break;
            }
        }
        ALTERA_DMAUnlock(hALTERA, dma);
        return TRUE;
    }
}