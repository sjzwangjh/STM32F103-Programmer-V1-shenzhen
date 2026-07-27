/*
 * ISP在线编程实现 - 通过SPI或ICSP协议对MCU编程
 */

/*
 * Name: isp.c
 * Project: STM32F103VET6_Programmer (AVR ISP Programmer)
 * Author: Ported from AVR-Doper by Christian Starkjohann <cs@obdev.at>
 *
 * Porting notes:
 *   - AVR PORT_OUT/PORT_IN macros -> STM32 bit-band PORT_OUT/PORT_IN from sys.h
 *   - AVR cli()/sei() -> __disable_irq()/__enable_irq()
 *   - AVR wdt_reset() -> IWDG_ReloadCounter() (via stm32f10x_iwdg.h)
 *   - Timing: timerMsDelay() for ms, timerTicksDelay() for bit-bang ticks
 *   - Hardware: DUT bus control via Hardware_Config.h HWPIN_ISP_* pins
 *
 * Original AVR-Doper ISP timing (ispBlockTransfer):
 *   Minimum clock pulse width: 5 + 4 * delay clock cycles -> Tmin = 750ns
 *   Total clock period: 12 + 8 * delay -> fmax = 600 kHz
 */

#include "sys.h"
#include "Hardware_Config.h"
#include "timer.h"
#include "isp.h"
#include "dutBus.h"

/* ------------------------------------------------------------------------- */
/* Static variables (mirror of AVR-Doper) */
static uint8_t      ispClockDelay;
static uint8_t      cmdBuffer[4];

/* ------------------------------------------------------------------------- */
/*
 * ispBlockTransfer - SPI bit-bang transfer (same sequential logic as AVR-Doper)
 *
 * Sends 'len' bytes from 'block' over the SPI bus. Returns the last byte
 * received. Interrupts are disabled during each byte transfer to ensure
 * nominal timing (matching AVR-Doper behavior).
 *
 * Clock polarity: SCK idle low, data clocked on rising edge, device changes
 * data on falling edge. MOSI is set before clock pulse.
 *
 * Timing: Between SCK transitions, timerTicksDelay(ispClockDelay) provides
 * the clock pulse width control.
 */
static uint8_t ispBlockTransfer(uint8_t *block, uint8_t len)
{
    uint8_t cnt, shift = 0, delay = ispClockDelay;

    __disable_irq();            /* matching AVR cli() */

    while (len--)               /* len may be 0 */
    {
        cnt = 8;
        shift = *block++;
        do
        {
            /* Set MOSI according to data bit */
            if (shift & 0x80)
                PORT_OUT(HWPIN_ISP_MOSI) = 1;
            else
                PORT_OUT(HWPIN_ISP_MOSI) = 0;

            __enable_irq();     /* matching AVR sei() */
            timerTicksDelay(delay);
            __disable_irq();    /* matching AVR cli() */

            /* SCK rising edge - device samples data */
            PORT_OUT(HWPIN_ISP_SCK) = 1;

            /* Shift and read MISO */
            shift <<= 1;
            if (PORT_IN(HWPIN_ISP_MISO))
                shift |= 1;

            __enable_irq();     /* matching AVR sei() */
            timerTicksDelay(delay);
            __disable_irq();    /* matching AVR cli() */

            /* SCK falling edge - device changes data */
            PORT_OUT(HWPIN_ISP_SCK) = 0;
        }
        while (--cnt);
    }

    __enable_irq();             /* matching AVR final sei() */
    return shift;
}

/* ------------------------------------------------------------------------- */
/*
 * ispAttachToDevice - Enter programming mode (same logic as AVR-Doper)
 *
 * Sets up ISP clock delay based on STK500v2 SCK duration parameter.
 * Initializes ISP pins, powers on device, applies RESET sequence.
 */
static void ispAttachToDevice(uint8_t stk500Delay, uint8_t stabDelay)
{
    u8 kk = 0;
    /* Calculate ispClockDelay from STK500v2 sckDuration parameter.
     *
     * AVR-Doper original algorithm:
     * - stk500Delay==0: 1.8MHz nominal -> ispClockDelay = 0
     * - stk500Delay==1: 460kHz nominal -> ispClockDelay = 0
     * - stk500Delay==2: 115kHz nominal -> ispClockDelay = 1
     * - stk500Delay==3: 58kHz  nominal -> ispClockDelay = 2
     * - else: formula: F_CPU > 14MHz ? (stk500+1)/2-(stk500+1)/8+(stk500+1)/32
     *                                 : (stk500+1)/4+(stk500+1)/16
     *
     * On STM32 @72MHz, TIMER_TICK_US=5us, we need to adjust to get
     * approximately the same SCK frequency as AVR-Doper.
     * AVR tick at 12MHz = 64/12e6*1e6 = 5.333us, roughly same as our 5us.
     * So we can use a simplified mapping.
     */
    if (stk500Delay == 0){ /* 1.8 MHz nominal */
        ispClockDelay = 0;
    }else if (stk500Delay == 1){ /* 460 kHz nominal */
        ispClockDelay = 0;
    }else if (stk500Delay == 2){ /* 115 kHz nominal */
        ispClockDelay = 1;
    }else if (stk500Delay == 3){ /* 58 kHz nominal */
        ispClockDelay = 2;
    }else{
        /* AVR @12MHz tick = 5.333us, STM32 tick = 5us (~6% faster).
         * The original formula for 12MHz was used in AVR-Doper.
         * Since our tick is similar, reuse the same approximation.
         * ispClockDelay = (stk500Delay + 1)/4 + (stk500Delay + 1)/16
         */
        ispClockDelay = (stk500Delay + 1) / 4 + (stk500Delay + 1) / 16;
    }
    #if 1
        if(ispClockDelay < 10) ispClockDelay = 1;
    #endif
    /* === Hardware initialization (STM32 DUT bus) === */
    PORT_OUT(HWPIN_LED) = 1;    // LED显示当前状态
    ISP_POWER_ON;   /* VDD上电 不要改动此语句 */
    /* setup initial condition: SCK, MOSI = 0 */
    DUT_PIN6_SET_OUTPUT;        // RESET设置为输出模式
    DUT_PIN7_SET_OUTPUT;        // MOSI设置为输出模式
    DUT_PIN5_SET_OUTPUT;        // SCK设置为输出模式
    DUT_PIN4_SET_INPUT;         // MISO设置为输入模式
    /* 针对硬件，将初始值0改成了1 */
    PORT_OUT(HWPIN_ISP_RESET) = 0;  // RESET = 0
    PORT_OUT(HWPIN_ISP_SCK) = 0;    // ISP SCK输出0
    PORT_OUT(HWPIN_ISP_MOSI) = 0;   // ISP MOSI输出0
    /* set TIM9_CH1 toggle on compare match mode -> activate clock */
#if 1
     timerMsDelay(stabDelay);
#else   // 调试代码
    for (u16 i = 0; i < (u16)stabDelay*200; i++)
    {
        PORT_OUT(HWPIN_ISP_SCK) = kk;    // ISP SCK输出0
        PORT_OUT(HWPIN_ISP_MOSI) = kk;   // ISP MOSI输出0
        kk^=1;
    }
#endif
    PORT_OUT(HWPIN_ISP_SCK) = 0;    // ISP SCK输出0
    PORT_OUT(HWPIN_ISP_MOSI) = 0;   // ISP MOSI输出0
    timerTicksDelay(ispClockDelay*2);     /* stabDelay may have been 0 */
    /* We now need to give a positive pulse on RESET since we can't guarantee
     * that SCK was low during power up (according to instructions in Atmel's
     * data sheets).
     */
    PORT_OUT(HWPIN_ISP_RESET) = 1;    /* give a positive RESET pulse */
    timerTicksDelay(ispClockDelay*4);
    PORT_OUT(HWPIN_ISP_RESET) = 0;
}

/* ------------------------------------------------------------------------- */
/*
 * ispDetachFromDevice - Leave programming mode (same logic as AVR-Doper)
 */
static void ispDetachFromDevice(uint8_t removeResetDelay)
{
    /* Set SCK, MOSI to 0 */
    PORT_OUT(HWPIN_ISP_SCK) = 0;
    PORT_OUT(HWPIN_ISP_MOSI) = 0;
    
    /* Release RESET */
    PORT_OUT(HWPIN_ISP_RESET) = 1;  /* RESET high -> normal operation */
    timerMsDelay(removeResetDelay);
    /* clear toggle on compare match mode */

    /* ISP Power Off */
    ISP_POWER_OFF;
    /* Disable ISP outputs - put pins back to input */
    DUT_PIN4_SET_INPUT;     // MISO
    DUT_PIN5_SET_INPUT;     // SCK
    DUT_PIN6_SET_INPUT;     // RESET
    DUT_PIN7_SET_INPUT;     // MOSI
    /* Turn off ISP active LED */
    PORT_OUT(HWPIN_LED) = 0;
}

/* ------------------------------------------------------------------------- */

uint8_t ispEnterProgmode(stkEnterProgIsp_t *param)
{
    uint8_t i, rval;
    /* 调用进入ISP函数 */
    ispAttachToDevice(stkParam.s.sckDuration, param->stabDelay);
    timerMsDelay(param->cmdExeDelay);

    /* Sync loop: send programming enable command until device responds.
     * avrdude often sends synchLoops == 0, so we use a fixed count of 32. */
    for (i = 0; i<32; i++)
    {
        rval = ispBlockTransfer(param->cmd, param->pollIndex);
        if (param->pollIndex < 4)
            ispBlockTransfer(param->cmd + param->pollIndex, 4 - param->pollIndex);
        if (rval == param->pollValue){   /* success: we are in sync */
            return STK_STATUS_CMD_OK;
        }
        /* Insert one extra clock pulse and try again */
        timerTicksDelay(ispClockDelay);
        PORT_OUT(HWPIN_ISP_SCK) = 1;
        timerTicksDelay(ispClockDelay);
        PORT_OUT(HWPIN_ISP_SCK) = 0;
    }

    ispDetachFromDevice(0);
    return STK_STATUS_CMD_FAILED;   /* failure */
}

/* ------------------------------------------------------------------------- */

void ispLeaveProgmode(stkLeaveProgIsp_t *param)
{
    ispDetachFromDevice(param->preDelay);
    timerMsDelay(param->postDelay);
}

/* ------------------------------------------------------------------------- */

static uint8_t deviceIsBusy(void)
{
    cmdBuffer[0] = 0xF0;        /* read status register */
    cmdBuffer[1] = 0;
    return ispBlockTransfer(cmdBuffer, 4) & 1;
}

static uint8_t waitUntilReady(uint8_t msTimeout)
{
    timerSetupTimeout(msTimeout);
    while (deviceIsBusy())
    {
        if (timerTimeoutOccurred())
            return STK_STATUS_RDY_BSY_TOUT;
    }
    return STK_STATUS_CMD_OK;
}

/* ------------------------------------------------------------------------- */

uint8_t ispChipErase(stkChipEraseIsp_t *param)
{
    uint8_t maxDelay = param->eraseDelay;
    uint8_t rval = STK_STATUS_CMD_OK;

    ispBlockTransfer(param->cmd, 4);

    if (param->pollMethod != 0)
    {
        if (maxDelay < 10)      /* allow at least 10 ms */
            maxDelay = 10;
        rval = waitUntilReady(maxDelay);
    }
    else
    {
        timerMsDelay(maxDelay);
    }
    return rval;
}

/* ------------------------------------------------------------------------- */

uint8_t ispProgramMemory(stkProgramFlashIsp_t *param, uint8_t isEeprom)
{
    utilWord_t  numBytes;
    uint8_t     rval = STK_STATUS_CMD_OK;
    uint8_t     valuePollingMask, rdyPollingMask;
    uint16_t    i;

    numBytes.bytes[1] = param->numBytes[0];
    numBytes.bytes[0] = param->numBytes[1];

    if (param->mode & 1)    /* page mode */
    {
        valuePollingMask = 0x20;
        rdyPollingMask = 0x40;
    }
    else                    /* word mode */
    {
        valuePollingMask = 4;
        rdyPollingMask = 8;
    }

    if (!isEeprom && stkAddress.bytes[3] & 0x80)
    {
        cmdBuffer[0] = 0x4D;    /* load extended address */
        cmdBuffer[1] = 0x00;
        cmdBuffer[2] = stkAddress.bytes[2];
        cmdBuffer[3] = 0x00;
        ispBlockTransfer(cmdBuffer, 4);
    }

    for (i = 0; rval == STK_STATUS_CMD_OK && i < numBytes.word; i++)
    {
        uint8_t x;

        cmdBuffer[1] = stkAddress.bytes[1];
        cmdBuffer[2] = stkAddress.bytes[0];
        cmdBuffer[3] = param->data[i];
        x = param->cmd[0];

        if (!isEeprom)
        {
            x &= ~0x08;
            if ((uint8_t)i & 1)
            {
                x |= 0x08;
                stkIncrementAddress();
            }
        }
        else
        {
            stkIncrementAddress();
        }
        cmdBuffer[0] = x;

        ispBlockTransfer(cmdBuffer, 4);

        if (param->mode & 1)    /* page mode */
        {
            if (i < numBytes.word - 1 || !(param->mode & 0x80))
                continue;       /* not last byte written */
            cmdBuffer[0] = param->cmd[1];   /* write program memory page */
            ispBlockTransfer(cmdBuffer, 4);
        }

        /* Poll for ready after each byte (word mode) or page (page mode) */
        if (param->mode & valuePollingMask)   /* value polling */
        {
            uint8_t d = param->data[i];
            if (d == param->poll[0] || (isEeprom && d == param->poll[1]))
            {
                timerMsDelay(param->delay);
            }
            else
            {
                uint8_t xr = param->cmd[2];  /* read memory */
                if (!isEeprom)
                {
                    xr &= ~0x08;
                    if ((uint8_t)i & 1)
                        xr |= 0x08;
                }
                cmdBuffer[0] = xr;
                timerSetupTimeout(param->delay);
                while (ispBlockTransfer(cmdBuffer, 4) != d)
                {
                    if (timerTimeoutOccurred())
                    {
                        rval = STK_STATUS_CMD_TOUT;
                        break;
                    }
                }
            }
        }
        else if (param->mode & rdyPollingMask)  /* rdy/bsy polling */
        {
            rval = waitUntilReady(param->delay);
        }
        else                                    /* timed delay */
        {
            timerMsDelay(param->delay);
        }
    }
    return rval;
}

/* ------------------------------------------------------------------------- */
/*
 * ispVerifyMemory - Verify Flash/EEPROM data carried by a PROGRAM packet.
 *
 * The input format is identical to ispProgramMemory(). The expected bytes are
 * taken from param->data[]. The target memory is read back using the read
 * command contained in param->cmd[2], following the same address and low/high
 * byte handling as ispReadMemory().
 *
 * Return value:
 *   0 - all bytes match
 *   1 - read-back data mismatch
 */
uint8_t ispVerifyMemory(stkProgramFlashIsp_t *param, uint8_t isEeprom)
{
    utilWord_t numBytes;
    uint8_t readCmd;
    uint16_t i;

    numBytes.bytes[1] = param->numBytes[0];
    numBytes.bytes[0] = param->numBytes[1];

    cmdBuffer[3] = 0;

    /* AVR devices with more than 128 Kbytes of Flash use an extended
     * address byte. This is the same sequence used by ispReadMemory().
     */
    if (!isEeprom && (stkAddress.bytes[3] & 0x80))
    {
        cmdBuffer[0] = 0x4D;
        cmdBuffer[1] = 0x00;
        cmdBuffer[2] = stkAddress.bytes[2];
        ispBlockTransfer(cmdBuffer, 4);
    }

    readCmd = param->cmd[2];

    for (i = 0; i < numBytes.word; i++)
    {
        uint8_t actual;

        cmdBuffer[1] = stkAddress.bytes[1];
        cmdBuffer[2] = stkAddress.bytes[0];

        if (!isEeprom)
        {
            /* Flash LOAD_ADDRESS is a word address. Bit 3 selects the high
             * byte; advance the word address after reading the high byte.
             */
            if ((uint8_t)i & 1U)
            {
                readCmd |= 0x08U;
                stkIncrementAddress();
            }
            else
            {
                readCmd &= (uint8_t)~0x08U;
            }
        }
        else
        {
            /* EEPROM uses byte addresses. ispReadMemory() advances the
             * protocol address once for every byte, after copying it to the
             * command buffer.
             */
            stkIncrementAddress();
        }

        cmdBuffer[0] = readCmd;
        actual = ispBlockTransfer(cmdBuffer, 4);

        if (actual != param->data[i])
            return 1;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */

uint16_t ispReadMemory(stkReadFlashIsp_t *param, stkReadFlashIspResult_t *result, uint8_t isEeprom)
{
    utilWord_t  numBytes;
    uint8_t     *p, cmd0;
    uint16_t    i;

    cmdBuffer[3] = 0;

    if (!isEeprom && stkAddress.bytes[3] & 0x80)
    {
        cmdBuffer[0] = 0x4D;    /* load extended address */
        cmdBuffer[1] = 0x00;
        cmdBuffer[2] = stkAddress.bytes[2];
        ispBlockTransfer(cmdBuffer, 4);
    }

    numBytes.bytes[1] = param->numBytes[0];
    numBytes.bytes[0] = param->numBytes[1];

    p = result->data;
    result->status1 = STK_STATUS_CMD_OK;
    cmd0 = param->cmd;

    for (i = 0; i < numBytes.word; i++)
    {
        cmdBuffer[1] = stkAddress.bytes[1];
        cmdBuffer[2] = stkAddress.bytes[0];

        if (!isEeprom)
        {
            if ((uint8_t)i & 1)
            {
                cmd0 |= 0x08;
                stkIncrementAddress();
            }
            else
            {
                cmd0 &= ~0x08;
            }
        }
        else
        {
            stkIncrementAddress();
        }

        cmdBuffer[0] = cmd0;
        *p++ = ispBlockTransfer(cmdBuffer, 4);
    }

    *p = STK_STATUS_CMD_OK;     /* status2 */
    return numBytes.word + 2;
}

/* ------------------------------------------------------------------------- */

uint8_t ispProgramFuse(stkProgramFuseIsp_t *param)
{
    ispBlockTransfer(param->cmd, 4);
    return STK_STATUS_CMD_OK;
}


/* ------------------------------------------------------------------------- */
/* Verify fuse value after PROGRAM_FUSE command. */
uint8_t ispVerifyFuse(stkProgramFuseIsp_t *param)
{
    uint8_t value;

    /*
     * stkProgramFuseIsp only contains the 4-byte ISP command.
     * The expected fuse value is stored in the fourth command byte.
     * Execute the SPI transfer and compare the returned value.
     */
    value = ispBlockTransfer(param->cmd, 4);

    if (value != param->cmd[3])
        return 1;

    return 0;
}

/* ------------------------------------------------------------------------- */

uint8_t ispReadFuse(stkReadFuseIsp_t *param)
{
    uint8_t rval;

    rval = ispBlockTransfer(param->cmd, param->retAddr);
    if (param->retAddr < 4)
        ispBlockTransfer(param->cmd + param->retAddr, 4 - param->retAddr);
    return rval;
}

/* ------------------------------------------------------------------------- */

uint16_t ispMulti(stkMultiIsp_t *param, stkMultiIspResult_t *result)
{
    uint8_t cnt1, i;
    uint8_t *p;

    cnt1 = param->numTx;
    if (cnt1 > param->rxStartAddr)
        cnt1 = param->rxStartAddr;

    ispBlockTransfer(param->txData, cnt1);

    p = result->rxData;
    for (i = 0; i < param->numTx - cnt1; i++)
    {
        uint8_t b = ispBlockTransfer(&param->txData[cnt1] + i, 1);
        if (i < param->numRx)
            *p++ = b;
    }

    for (; i < param->numRx; i++)
    {
        cmdBuffer[0] = 0;
        *p++ = ispBlockTransfer(cmdBuffer, 1);
    }

    *p = result->status1 = STK_STATUS_CMD_OK;
    return (uint16_t)param->numRx + 2;
}

/* ------------------------------------------------------------------------- */


