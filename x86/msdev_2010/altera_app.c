
#include "altera_app.h"

BOOL DeviceFindAndOpen(ALTERA_HANDLE *phAltera, DWORD dwVendorID, DWORD dwDeviceID) {

    DOWRD dwCardNum = ALTERA_CountCards(dwVendorID, dwDeviceID);
    if (dwCardNum == 0) {
        printf("No cards found.\n");
        return FALSE;
    }
    BOOL status = ALTERA_Open(&phAltera, dwVendorID, dwDeviceID, dwCardNum-1);
    if (!status) {
        printf("Fail to open the device.\n");
        return FALSE;
    }

    for (DWORD i = 0; i < dwCardNum; i++) {
        printf("\n");
        printf("%2ld. Vendor ID: 0x%lX, Device ID: 0x%lX\n",
            i + 1,
            phAltera->deviceId[i].dwVendorId,
            phAltera->deviceId[i].dwDeviceId);

        WDC_DIAG_PciDeviceInfoPrint(&phaltera->deviceSlot[i], FALSE);
    }
    return TRUE;
}

BOOL SetDescriptorTable() {

    ALTERA_DT *des_table_ptr;
    des_table_ptr = (ALTERA_DT)malloc(sizeof (ALTERA_DT));
    // DDR3 addr in Qsys
    des_table_ptr->descriptor[0].src_addr_low = 0x00000000;
    des_table_ptr->descriptor[0].src_addr_high = 0x8000000;
    // Host memory addr
    des_table_ptr->descriptor[0].des_addr_low = 0x80000000;
    des_table_ptr->descriptor[0].des_addr_high = 0xd0000000;
    // Descriptor ID is 0 and DMA size is 64KB
    des_table_ptr->descriptor[0].id_dma_length = 0x00004000;

    des_table_ptr->descriptor[0].rsv1 = 0;
    des_table_ptr->descriptor[0].rsv2 = 0;
    des_table_ptr->descriptor[0].rsv3 = 0;


}