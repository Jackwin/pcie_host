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
  
    /*     DWORD status = ALTERA_DMABlock(hALTERA,FALSE, VENDOR_ID, DEVICE_ID);
     if (!status) {
         printf("Fail to Read Altera Device.\n");
         return 0;
     }
     */
    DMAOperation(3, "model_480x270_","txt", 480, 270, 0, VENDOR_ID, DEVICE_ID);
//    PatternFill("model1_480x270.txt", 480, 270);
     
}
