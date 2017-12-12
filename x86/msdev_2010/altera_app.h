#ifndef _ALTERA_APP_H_
#define _ALTERA_APP_H_


#include <stdio.h>
#include "wdc_defs.h"
#include "wdc_lib.h"
#include "utils.h"
#include "status_strings.h"
#include "samples/shared/diag_lib.h"
#include "samples/shared/wdc_diag_lib.h"
#include "samples/shared/pci_regs.h"
#include "stratixv_lib.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DS_NUM 128
#define DT_NUM 1


typedef struct
{
    DWORD   status;
}DS;

typedef struct {
    DWORD src_addr_low;
    DWORD src_addr_high;
    DWORD des_addr_low;
    DWORD des_addr_high;
    DWORD id_dma_length;
    DWORD rsv1;
    DWORD rsv2;
    DWORD rsv3;
}DT;

typedef struct
{
    DS status_list[DS_NUM];
    DT descriptor[1];

}ALTERA_DT;
#ifdef __cplusplus
}
#endif

#endif