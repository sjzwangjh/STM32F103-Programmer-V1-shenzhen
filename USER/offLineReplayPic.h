#ifndef __OFFLINE_REPLAY_PIC_H__
#define __OFFLINE_REPLAY_PIC_H__

#include <stdint.h>

/*
 * PIC offline replay group: ICSP program, verify and lock/leave passes.
 * Shared frame reading/execution helpers live in offLinePgmer.c.
 */
uint16_t picReplayProgramPass(void);
uint16_t picReplayVerifyPass(void);
uint16_t picReplayLockAndLeavePass(void);

#endif /* __OFFLINE_REPLAY_PIC_H__ */
