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


DWORD VENDOR_ID = 0x1172;
DWORD DEVICE_ID = 0xe003;

typedef struct 
{
	DWORD vendor_id;
	DWORD device_id;
	DWORD num;
}CARD_STRUCT;

int main() {
	WDC_DEVICE_HANDLE hDev = NULL;
	DWORD dwStatus;
	ALTERA_HANDLE *phALTERA = NULL;
	CARD_STRUCT card;
	card.vendor_id = VENDOR_ID;
	card.device_id = DEVICE_ID;
	card.num = 1;
	printf("\n");

	DWORD card_num = ALTERA_CountCards(VENDOR_ID, DEVICE_ID);
	printf("%d Altera cards found.\n", card_num);

	BOOL status = ALTERA_Open(&phALTERA, card.vendor_id, card.device_id, card.num-1);
	if (!status) {
		printf("Fail to open Altera Device.\n");
		return 0;
	}


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
