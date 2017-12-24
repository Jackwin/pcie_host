#include <stdio.h>
#include "wdc_defs.h"
#include "wdc_lib.h"
#include "utils.h"
#include "status_strings.h"
#include "samples/shared/diag_lib.h"
#include "samples/shared/wdc_diag_lib.h"
#include "samples/shared/pci_regs.h"
//#include "stratixv_lib.h"
#include "altera_lib.h"
#include <time.h>


DWORD VENDOR_ID = 0x1172;
DWORD DEVICE_ID = 0xe003;

typedef struct
{
    DWORD vendor_id;
    DWORD device_id;
    DWORD num;
} CARD_STRUCT;

int main() {
    WDC_DEVICE_HANDLE hDev = NULL;
    DWORD dwStatus;
    ALTERA_HANDLE hALTERA =NULL;
    CARD_STRUCT card;
    card.vendor_id = VENDOR_ID;
    card.device_id = DEVICE_ID;
    card.num = 1;
    printf("\n");

	printf("long size is %d.\n", sizeof(long));
	printf("double size is %d.\n", sizeof(double));
	printf("int size is %d.\n", sizeof(int));
	printf("word size is %d.\n", sizeof(WORD));
	printf("dword size is %d.\n", sizeof(DWORD));

   // DWORD card_num = ALTERA_CountCards(VENDOR_ID, DEVICE_ID);
   // printf("%d Altera cards found.\n", card_num);
    printf("ALTERA diagnostic utility.\n");
    printf("Application accesses hardware using " WD_PROD_NAME ".\n");
    // make sure WinDriver is loaded
    if (!PCI_Get_WD_handle(&hDev)) return 0;
    WD_Close(hDev);

    hDev = initialize_PCI(VENDOR_ID, DEVICE_ID);

    
/*
    BOOL status = ALTERA_Open(&phALTERA, card.vendor_id, card.device_id, card.num - 1);
    if (!status) {
        printf("Fail to open Altera Device.\n");
        return 0;
    }
*/


   // BOOL status = DeviceFindAndOpen(&hALTERA,card.vendor_id, card.device_id);

    
  
     DWORD status = ALTERA_DMABlock(hDev,hALTERA,FALSE);
     if (!status) {
         printf("Fail to Read Altera Device.\n");
         return 0;

     }
     WDC_PciDeviceClose(hDev);
     /*
     DWORD *pbuf, *prdbuf;
     pbuf = (DWORD*)(malloc(sizeof(DWORD) * 256));
     prdbuf = (DWORD*)(malloc(sizeof(DWORD) * 256));
     DWORD rdbuf[256];
    BZERO(rdbuf);
     DWORD dwBytes = sizeof(DWORD) * 256;

     init_rp_mem(pbuf, 2014);
     time_t start, finish;
     double result;
     double totaltime;
     start = clock();
     ALTERA_ReadWriteBlock(hALTERA, AD_PCI_BAR4, 0x10000, 0, pbuf, dwBytes, ALTERA_MODE_DWORD);
     
     for (long loop = 0; loop < 10000000; loop++)
         result = 3.63 * 5.27; ;
     
     for (int i = 0; i < 100; i++) {
         ALTERA_ReadWriteBlock(hALTERA, AD_PCI_BAR4, 0x00 + 0x1000*i, 0, pbuf, dwBytes, ALTERA_MODE_DWORD);
     }
     
    
 
     for (int j = 0; j < 100; j++)
        for (int i = 0; i < 256; i++) {
             rdbuf[i] = ALTERA_ReadDword(hALTERA, AD_PCI_BAR4, i * 4);
         }
   
  
    // system("pause");
     //time(&finish);
     finish = clock();

     totaltime = difftime(finish, start);
     printf("total time is %lf.\n", totaltime);
     */
    // printf("the data before last data in prdbuf is %d.\n", *(prdbuf + 254));
    // printf("last data in prdbuf is %d.\n", *(prdbuf + 255));
     ///for(int i = 0; i < 16; i++)
     //   printf("rddata in rdbuf is %x.\n", rdbuf[i]);

//    ALTERA_Close(hALTERA);

    /* Initialize the STRATIXV library */
    /*
    dwStatus = STRATIXV_LibInit();
    if (WD_STATUS_SUCCESS != dwStatus)
    {
        STRATIXV_ERR("stratixv_diag: Failed to initialize the STRATIXV library: %s",
            STRATIXV_GetLastErr());
        return dwStatus;
    }
    */
    /* Find and open a STRATIXV device (by default ID) */
    /*
    if (STRATIXV_DEFAULT_VENDOR_ID)
        hDev = DeviceFindAndOpen(STRATIXV_DEFAULT_VENDOR_ID, STRATIXV_DEFAULT_DEVICE_ID);
    */
    return 0;
}
