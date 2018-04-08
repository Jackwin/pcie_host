#include <stdio.h>
#include "wdc_defs.h"
#include "wdc_lib.h"
#include "utils.h"
#include "status_strings.h"
#include "samples/shared/diag_lib.h"
#include "samples/shared/wdc_diag_lib.h"
#include "samples/shared/pci_regs.h"

#include "pci_driver_lib.h"
#include "windrvr_int_thread.h"

//#include "stratixv_lib.h"
#include "altera_lib.h"
#include <time.h>
#include <string.h>
#include "dma.h"

DWORD VENDOR_ID = 0x1172;
DWORD DEVICE_ID = 0xe003;

typedef struct
{
    DWORD vendor_id;
    DWORD device_id;
    DWORD num;
} CARD_STRUCT;

int main() {
   // WDC_DEVICE_HANDLE hDev = NULL;
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
  
    /*     DWORD status = ALTERA_DMABlock(hALTERA,FALSE, VENDOR_ID, DEVICE_ID);
     if (!status) {
         printf("Fail to Read Altera Device.\n");
         return 0;
     }
     */

    if (!PCI_Get_WD_handle(&hDev)) {
        printf("Fail to get WD_handle.\n");
        return 0;
    }
    // WD_Close(hDev);
    int status = initialize_PCI(VENDOR_ID, DEVICE_ID);
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to initialize PCI, %d.\n", status);
    }
  
    DMAOperation( VENDOR_ID, DEVICE_ID);
    status = close_pci(hDev);
    if (status != 0) {
        printf("Fail to cloese pci.\n");
    }
//    PatternFill("model1_480x270.txt", 480, 270);
    /*
    if (!PCI_Get_WD_handle(&hDev)) {
        printf("Fail to get WD_handle.\n");
        return 0;
    }
    WD_Close(hDev);
    int status = initialize_PCI(VENDOR_ID, DEVICE_ID);
    if (status != WD_STATUS_SUCCESS) {
        printf("Fail to initialize PCI.\n");
    }
    */
    //int read_data;

    //WDC_ReadAddr32(hDev, ALTERA_AD_BAR4, 0x60, &read_data);
    //printf("Read_data from Address 0x60 is %x.\n", read_data);
     


     close_pci(hDev);
}
