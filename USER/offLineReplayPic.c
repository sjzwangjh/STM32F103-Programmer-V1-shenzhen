/*
 * offLineReplayPic.c - PIC offline replay group (ICSP)
 *
 * Skeleton: the three passes currently fail explicitly (return 1) until the
 * ICSP replay logic is implemented. This prevents a PIC package from being
 * reported as "programmed" while nothing has actually run.
 */
#include "offLineReplayPic.h"
#include "offLinePgmer.h"

uint16_t picReplayProgramPass(void)
{
    /* TODO: ICSP program pass (PROGRAM_FLASH/EEPROM/CONFIG/USER_ID ...) */
    return 1U;
}

uint16_t picReplayVerifyPass(void)
{
    /* TODO: ICSP verify pass (deviceID precheck, read-back compare ...) */
    return 1U;
}

uint16_t picReplayLockAndLeavePass(void)
{
    /* TODO: ICSP lock/leave pass */
    return 1U;
}
