#ifndef __BOOT_APP_COMMON_H_INCLUDED__
#define __BOOT_APP_COMMON_H_INCLUDED__

#include "boot_App.h"

/* Backward-compatible aliases while the projects migrate to boot_App.h. */
#define APP_START_ADDER         APP_CODE_BASE
#define FLASH_APP_END           APP_REGION_END
#define APP_FLASH_SIZE          APP_REGION_SIZE

#endif /* __BOOT_APP_COMMON_H_INCLUDED__ */
