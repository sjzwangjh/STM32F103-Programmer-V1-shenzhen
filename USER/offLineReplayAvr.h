#ifndef __OFFLINE_REPLAY_AVR_H__
#define __OFFLINE_REPLAY_AVR_H__

#include <stdint.h>

/*
 * AVR offline replay group: ISP / HVSP / PP program, verify and lock/leave
 * passes. Shared frame reading/execution helpers live in offLinePgmer.c.
 */
uint16_t avrReplayProgramPass(void);
uint16_t avrReplayVerifyPass(void);
uint16_t avrReplayLockAndLeavePass(void);

#endif /* __OFFLINE_REPLAY_AVR_H__ */
