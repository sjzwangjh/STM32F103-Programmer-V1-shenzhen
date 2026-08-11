/*
 * 高压编程器控制实现 - DUT编程/擦除/校验时序控制
 */
/*----------------------------------------------
HVSR  Pin, 
    SDI->PB0->Serial DAta Input
    SII->PB1->Serial Instruction Input
    SDO->PB2->Serial Data Output
    SCI->PB3->Serial Clock Input
    SCK->PB4->校准时钟输入--校准时使用

    PB1 PB4  -  PB5
    SII CLK  -  RST GND
     2   4   6   8   10
     1   3   5   7   9
    SDI SDO  -  SCI VDD
    PB0 PB2     PB3
-----------------------------------------------*/


#include "sys.h"
#include "Hardware_Config.h"

#if ENABLE_HVPROG

#include "delay.h"
#include "timer.h"
#include "dutBus.h"
#include "hvproc.h"

/* control lines for high voltage serial (and parallel) programming */
#define HVCTL_PAGEL (1 << 0)
#define HVCTL_BS2   (1 << 1)
#define HVCTL_nOE   (1 << 2)
#define HVCTL_nWR   (1 << 3)
#define HVCTL_BS1   (1 << 4)
#define HVCTL_XA0   (1 << 5)
#define HVCTL_XA1   (1 << 6)

/* actions */
#define HV_ADDR     0
#define HV_DATA     HVCTL_XA0
#define HV_CMD      HVCTL_XA1
#define HV_NONE     (HVCTL_XA1 | HVCTL_XA0)
#define HV_PAGEL    (HVCTL_XA1 | HVCTL_XA0 | HVCTL_PAGEL)

/* bytes */
#define HV_LOW      0
#define HV_HIGH     HVCTL_BS1
#define HV_EXT      HVCTL_BS2
#define HV_EXT2     (HVCTL_BS1 | HVCTL_BS2)

/* modes */
#define HV_READ     HVCTL_nWR
#define HV_WRITE    HVCTL_nOE
#define HV_NORW     (HVCTL_nWR | HVCTL_nOE)

#define HVCTL(action, byte, mode)   ((action) | (byte) | (mode))

/* high voltage programming commands */
#define HVCMD_CHIP_ERASE    0x80
#define HVCMD_WRITE_FUSE    0x40
#define HVCMD_WRITE_LOCK    0x20
#define HVCMD_WRITE_FLASH   0x10
#define HVCMD_WRITE_EEPROM  0x11
#define HVCMD_READ_SIGCAL   0x08
#define HVCMD_READ_FUSELCK  0x04
#define HVCMD_READ_FLASH    0x02
#define HVCMD_READ_EEPROM   0x03
#define HVCMD_NOP           0x00

#define MODEMASK_PAGEMODE   1
#define MODEMASK_LAST_PAGE  0x40
#define MODEMASK_FLASH_PAGE 0x80

static uint8_t progModeIsPp;
static uint8_t hvPollTimeout;

/* forward declaration */
static uint8_t hvReadFuseInternal(uint8_t highLow);

#define HVSP_DELAY()            timerTicksDelay(1)
#define HVSP_SCI_OUT(value)     (PORT_OUT(HWPIN_HVSP_SCI) = ((value) ? 1 : 0))
#define HVSP_SII_OUT(value)     (PORT_OUT(HWPIN_HVSP_SII) = ((value) ? 1 : 0))
#define HVSP_SDI_OUT(value)     (PORT_OUT(HWPIN_HVSP_SDI) = ((value) ? 1 : 0))
#define HVSP_SDO_IN()           (PORT_IN(HWPIN_HVSP_SDO) ? 1 : 0)

#define HVSP_RESET_RELEASE()    do { PORT_OUT(HWPIN_HVSP_RESET) = 0; } while(0)
#define HVSP_RESET_ASSERT()     do { PORT_OUT(HWPIN_HVSP_RESET) = 1; } while(0)
#define HVSP_HV_OFF()           do { PORT_OUT(HWPIN_HVSP_HVRESET) = 0; } while(0)
#define HVSP_HV_ON()            do { PORT_OUT(HWPIN_HVSP_HVRESET) = 1; } while(0)
#define HVSP_VDD_ON()           DUT_VDD_SET_VDD
#define HVSP_VDD_OFF()          DUT_VDD_SET_FLOAT

#define HVSP_BUS_IDLE()         do { HVSP_SCI_OUT(0); HVSP_SII_OUT(0); HVSP_SDI_OUT(0); } while(0)

static void hvspBusConfig(void)
{
    /* Match AVR-Doper direction roles:
     * SDO is target -> programmer input, SDI/SII/SCI are programmer outputs.
     */
    DUT_PIN4_SET_INPUT;
    DUT_PIN5_SET_OUTPUT;
    DUT_PIN6_SET_OUTPUT;
    DUT_PIN7_SET_OUTPUT;
    HVSP_BUS_IDLE();
}

static uint8_t hvspExecute(uint8_t ctlLines, uint8_t data)
{
    uint8_t cnt;
    uint8_t r = 0;

    HVSP_BUS_IDLE();
    HVSP_SCI_OUT(1);
    HVSP_DELAY();
    HVSP_SCI_OUT(0);

    for (cnt = 0; cnt < 8; cnt++)
    {
        r <<= 1;
        if (HVSP_SDO_IN())
            r |= 1;

        HVSP_SDI_OUT((data & 0x80) != 0);
        HVSP_SII_OUT((ctlLines & 0x80) != 0);

        HVSP_SCI_OUT(1);
        HVSP_DELAY();
        HVSP_SCI_OUT(0);

        HVSP_SII_OUT(0);
        HVSP_SDI_OUT(0);
        ctlLines <<= 1;
        data <<= 1;
    }

    HVSP_SCI_OUT(1);
    HVSP_DELAY();
    HVSP_SCI_OUT(0);
    HVSP_DELAY();
    HVSP_SCI_OUT(1);
    HVSP_DELAY();
    HVSP_SCI_OUT(0);

    return r;
}

static uint8_t hvSetControlAndData(uint8_t ctlLines, uint8_t data)
{
    (void)progModeIsPp;
    return hvspExecute(ctlLines, data);
}

static uint8_t hvspPoll(void)
{
    uint8_t rval = STK_STATUS_CMD_OK;

    timerSetupTimeout(hvPollTimeout);
    while (!HVSP_SDO_IN())
    {
        if (timerTimeoutOccurred())
        {
            rval = STK_STATUS_CMD_TOUT;
            break;
        }
    }
    return rval;
}

static uint8_t hvPoll(void)
{
    return hvspPoll();
}

void hvspEnterProgmode(stkEnterProgHvsp_t *param)
{
    progModeIsPp = 0;
    PORT_OUT(HWPIN_LED) = 1;
    hvspBusConfig();

    /* Port the original AVR-Doper entry ordering:
     * 1. remove VDD/VPP, release RESET
     * 2. apply VDD
     * 3. briefly hold the SDO line low as output
     * 4. assert RESET low
     * 5. apply high voltage to RESET
     * 6. release SDO back to input
     */
    HVSP_HV_OFF();
    HVSP_RESET_RELEASE();
    HVSP_VDD_OFF();
    timerMsDelay(param->powerOffDelay);

    HVSP_RESET_RELEASE();
    HVSP_VDD_ON();
    if (param->stabDelay)
        timerMsDelay(param->stabDelay);

    DUT_PIN4_SET_OUTPUT;
    PORT_OUT(HWPIN_HVSP_SDO) = 0;
    delay_us(param->resetDelay1 ? param->resetDelay1 : 40);

    HVSP_RESET_ASSERT();
    HVSP_HV_ON();
    delay_us(param->resetDelay2 ? param->resetDelay2 : 15);

    DUT_PIN4_SET_INPUT;
    delay_us(300);

    if (param->cmdExeDelay)
        timerMsDelay(param->cmdExeDelay);
}

void hvspLeaveProgmode(stkLeaveProgHvsp_t *param)
{
    if (param->stabDelay)
        timerMsDelay(param->stabDelay);

    HVSP_HV_OFF();
    if (param->resetDelay)
        timerMsDelay(param->resetDelay);

    HVSP_RESET_RELEASE();
    HVSP_BUS_IDLE();
    HVSP_VDD_OFF();
    DUT_BUS_SET_INPUT;
    PORT_OUT(HWPIN_LED) = 0;
}

void ppEnterProgmode(stkEnterProgPp_t *param)
{
    hvspEnterProgmode((stkEnterProgHvsp_t *)param);
    progModeIsPp = 1;
    if (param->progModeDelay)
        timerMsDelay(param->progModeDelay);
}

void ppLeaveProgmode(stkLeaveProgPp_t *param)
{
    hvspLeaveProgmode((stkLeaveProgHvsp_t *)param);
}

static uint8_t hvChipErase(uint8_t eraseTime)
{
    uint8_t rval = STK_STATUS_CMD_OK;

    hvSetControlAndData(HVCTL(HV_CMD, HV_LOW, HV_NORW), HVCMD_CHIP_ERASE);
    hvSetControlAndData(HVCTL(HV_NONE, HV_LOW, HV_WRITE), 0);
    hvSetControlAndData(HVCTL(HV_NONE, HV_LOW, HV_NORW), 0);
    if(hvPollTimeout){
        rval = hvPoll();
    }else{
        timerMsDelay(eraseTime);
    }
    hvSetControlAndData(HVCTL(HV_CMD, HV_LOW, HV_NORW), HVCMD_NOP);
    return rval;
}

uint8_t hvspChipErase(stkChipEraseHvsp_t *param)
{
    hvPollTimeout = param->pollTimeout;
    return hvChipErase(param->eraseTime);
}

uint8_t ppChipErase(stkChipErasePp_t *param)
{
    hvPollTimeout = param->pollTimeout;
    return hvChipErase(param->pulseWidth ? param->pulseWidth : 10);
}

static uint8_t hvProgramMemory(uint8_t *data, uint8_t len, uint8_t mode, uint8_t isEeprom)
{
    uint8_t x;
    uint8_t pageMask = 0xff;
    uint8_t rval = STK_STATUS_CMD_OK;

    x = (uint8_t)(-(mode >> 1)) & 7;
    while(x--){
        pageMask >>= 1;
    }
    if(!isEeprom){
        pageMask >>= 1;
    }

    hvSetControlAndData(HVCTL(HV_CMD, HV_LOW, HV_NORW), isEeprom ? HVCMD_WRITE_EEPROM : HVCMD_WRITE_FLASH);
    do{
        hvSetControlAndData(HVCTL(HV_ADDR, HV_LOW, HV_NORW), stkAddress.bytes[0]);
        if(mode & MODEMASK_PAGEMODE){
            hvSetControlAndData(HVCTL(HV_DATA, HV_LOW, HV_NORW), *data++);
            if(isEeprom){
                hvSetControlAndData(HVCTL(HV_PAGEL, HV_LOW, HV_NORW), 0);
                hvSetControlAndData(HVCTL(HV_NONE, HV_LOW, HV_NORW), 0);
            }else{
                hvSetControlAndData(HVCTL(HV_DATA, HV_HIGH, HV_NORW), *data++);
                hvSetControlAndData(HVCTL(HV_PAGEL, HV_HIGH, HV_NORW), 0);
                hvSetControlAndData(HVCTL(HV_NONE, HV_HIGH, HV_NORW), 0);
            }
            x = (uint8_t)(stkAddress.bytes[0] + 1);
            if((x & pageMask) == 0 && (mode & MODEMASK_FLASH_PAGE)){
                hvSetControlAndData(HVCTL(HV_ADDR, HV_HIGH, HV_NORW), stkAddress.bytes[1]);
                hvSetControlAndData(HVCTL(HV_NONE, HV_LOW, HV_WRITE), 0);
                hvSetControlAndData(HVCTL(HV_NONE, HV_LOW, HV_NORW), 0);
                rval = hvPoll();
            }
        }else{
            hvSetControlAndData(HVCTL(HV_ADDR, HV_HIGH, HV_NORW), stkAddress.bytes[1]);
            hvSetControlAndData(HVCTL(HV_DATA, HV_LOW, HV_NORW), *data++);
            if(isEeprom){
                hvSetControlAndData(HVCTL(HV_PAGEL, HV_LOW, HV_NORW), 0);
                hvSetControlAndData(HVCTL(HV_NONE, HV_LOW, HV_WRITE), 0);
                hvSetControlAndData(HVCTL(HV_NONE, HV_LOW, HV_NORW), 0);
            }else{
                hvSetControlAndData(HVCTL(HV_NONE, HV_LOW, HV_WRITE), 0);
                hvSetControlAndData(HVCTL(HV_NONE, HV_LOW, HV_NORW), 0);
                if((rval = hvPoll()) != STK_STATUS_CMD_OK){
                    break;
                }
                hvSetControlAndData(HVCTL(HV_DATA, HV_HIGH, HV_NORW), *data++);
                hvSetControlAndData(HVCTL(HV_NONE, HV_HIGH, HV_WRITE), 0);
                hvSetControlAndData(HVCTL(HV_NONE, HV_HIGH, HV_NORW), 0);
            }
            if((rval = hvPoll()) != STK_STATUS_CMD_OK){
                break;
            }
        }
        stkIncrementAddress();
        if(!isEeprom && !--len){
            break;
        }
    }while(--len);

    if(!(mode & MODEMASK_PAGEMODE) || (mode & MODEMASK_LAST_PAGE)){
        hvSetControlAndData(HVCTL(HV_CMD, HV_LOW, HV_NORW), HVCMD_NOP);
    }
    return rval;
}

uint8_t hvspProgramMemory(stkProgramFlashHvsp_t *param, uint8_t isEeprom)
{
    hvPollTimeout = param->pollTimeout;
    return hvProgramMemory(param->data, param->numBytes[1], param->mode, isEeprom);
}

static void hvReadMemory(uint8_t *data, uint16_t len, uint8_t isEeprom)
{
    hvSetControlAndData(HVCTL(HV_CMD, HV_LOW, HV_NORW), isEeprom ? HVCMD_READ_EEPROM : HVCMD_READ_FLASH);
    while(len-- > 0){
        hvSetControlAndData(HVCTL(HV_ADDR, HV_LOW, HV_NORW), stkAddress.bytes[0]);
        hvSetControlAndData(HVCTL(HV_ADDR, HV_HIGH, HV_NORW), stkAddress.bytes[1]);
        hvSetControlAndData(HVCTL(HV_NONE, HV_LOW, HV_READ), 0);
        *data++ = hvSetControlAndData(HVCTL(HV_NONE, HV_LOW, HV_NORW), 0);
        if(!isEeprom){
            hvSetControlAndData(HVCTL(HV_NONE, HV_HIGH, HV_READ), 0);
            *data++ = hvSetControlAndData(HVCTL(HV_NONE, HV_HIGH, HV_NORW), 0);
            len--;
        }
        stkIncrementAddress();
    }
    *data = STK_STATUS_CMD_OK;
}


/* Verify data carried by a HV PROGRAM packet. */
uint8_t hvspVerifyMemory(stkProgramFlashHvsp_t *param, uint8_t isEeprom)
{
    uint16_t len = param->numBytes[1];
    uint8_t *expect = param->data;

    hvSetControlAndData(HVCTL(HV_CMD, HV_LOW, HV_NORW), isEeprom ? HVCMD_READ_EEPROM : HVCMD_READ_FLASH);

    while (len)
    {
        uint8_t actual;
        hvSetControlAndData(HVCTL(HV_ADDR, HV_LOW, HV_NORW), stkAddress.bytes[0]);
        hvSetControlAndData(HVCTL(HV_ADDR, HV_HIGH, HV_NORW), stkAddress.bytes[1]);
        hvSetControlAndData(HVCTL(HV_NONE, HV_LOW, HV_READ), 0);
        actual = hvSetControlAndData(HVCTL(HV_NONE, HV_LOW, HV_NORW), 0);

        if (actual != *expect++)
            return 1;

        len--;
        if (!isEeprom && len)
        {
            hvSetControlAndData(HVCTL(HV_NONE, HV_HIGH, HV_READ), 0);
            actual = hvSetControlAndData(HVCTL(HV_NONE, HV_HIGH, HV_NORW), 0);
            if (actual != *expect++)
                return 1;
            len--;
        }
        stkIncrementAddress();
    }
    return 0;
}

uint8_t hvspVerifyFuse(stkProgramFuseHvsp_t *param)
{
    uint8_t highLow;
    if(param->fuseAddress == 0) highLow = HV_LOW;
    else if(param->fuseAddress == 1) highLow = HV_HIGH;
    else highLow = HV_EXT;
    return (hvReadFuseInternal(highLow) == param->fuseByte) ? 0 : 1;
}

uint8_t ppVerifyMemory(stkProgramFlashHvsp_t *param, uint8_t isEeprom)
{
    return hvspVerifyMemory(param, isEeprom);
}

uint8_t ppVerifyFuse(stkProgramFusePp_t *param)
{
    uint8_t highLow;
    if(param->address == 0) highLow = HV_LOW;
    else if(param->address == 1) highLow = HV_HIGH;
    else if(param->address == 2) highLow = HV_EXT;
    else highLow = HV_EXT2;
    return (hvReadFuseInternal(highLow) == param->data) ? 0 : 1;
}

uint16_t hvspReadMemory(stkReadFlashHvsp_t *param, stkReadFlashHvspResult_t *result, uint8_t isEeprom)
{
    utilWord_t numBytes;

    numBytes.bytes[1] = param->numBytes[0];
    numBytes.bytes[0] = param->numBytes[1];
    result->status1 = STK_STATUS_CMD_OK;
    hvReadMemory(result->data, numBytes.word, isEeprom);
    return numBytes.word + 2;
}

static uint8_t hvProgramFuse(uint8_t value, uint8_t cmd, uint8_t highLow)
{
    uint8_t rval;

    hvSetControlAndData(HVCTL(HV_CMD, HV_LOW, HV_NORW), cmd);
    hvSetControlAndData(HVCTL(HV_DATA, HV_LOW, HV_NORW), value);
    hvSetControlAndData(HVCTL(HV_NONE, highLow, HV_WRITE), 0);
    hvSetControlAndData(HVCTL(HV_NONE, highLow, HV_NORW), 0);
    rval = hvPoll();
    hvSetControlAndData(HVCTL(HV_CMD, HV_LOW, HV_NORW), HVCMD_NOP);
    return rval;
}

uint8_t hvspProgramFuse(stkProgramFuseHvsp_t *param)
{
    uint8_t highLow;

    hvPollTimeout = param->pollTimeout;
    if(param->fuseAddress == 0){
        highLow = HV_LOW;
    }else if(param->fuseAddress == 1){
        highLow = HV_HIGH;
    }else{
        highLow = HV_EXT;
    }
    return hvProgramFuse(param->fuseByte, HVCMD_WRITE_FUSE, highLow);
}

uint8_t hvspProgramLock(stkProgramFuseHvsp_t *param)
{
    hvPollTimeout = param->pollTimeout;
    return hvProgramFuse(param->fuseByte, HVCMD_WRITE_LOCK, HV_LOW);
}

uint8_t ppProgramFuse(stkProgramFusePp_t *param)
{
    uint8_t highLow;

    hvPollTimeout = param->pollTimeout;
    if(param->address == 0){
        highLow = HV_LOW;
    }else if(param->address == 1){
        highLow = HV_HIGH;
    }else if(param->address == 2){
        highLow = HV_EXT;
    }else{
        highLow = HV_EXT2;
    }
    return hvProgramFuse(param->data, HVCMD_WRITE_FUSE, highLow);
}

uint8_t ppProgramLock(stkProgramFusePp_t *param)
{
    hvPollTimeout = param->pollTimeout;
    return hvProgramFuse(param->data, HVCMD_WRITE_LOCK, HV_LOW);
}

static uint8_t hvReadFuseInternal(uint8_t highLow)
{
    hvSetControlAndData(HVCTL(HV_CMD, HV_LOW, HV_NORW), HVCMD_READ_FUSELCK);
    hvSetControlAndData(HVCTL(HV_NONE, highLow, HV_READ), 0);
    return hvSetControlAndData(HVCTL(HV_NONE, highLow, HV_NORW), 0);
}

uint8_t hvspReadFuse(stkReadFuseHvsp_t *param)
{
    uint8_t highLow;

    if(param->fuseAddress == 0){
        highLow = HV_LOW;
    }else if(param->fuseAddress == 1){
        highLow = HV_EXT2;
    }else if(param->fuseAddress == 2){
        highLow = HV_EXT;
    }else{
        return STK_STATUS_CMD_FAILED;
    }
    return hvReadFuseInternal(highLow);
}

uint8_t hvspReadLock(void)
{
    return hvReadFuseInternal(HV_HIGH);
}

static uint8_t hvReadSignatureInternal(uint8_t addr, uint8_t highLow)
{
    hvSetControlAndData(HVCTL(HV_CMD, HV_LOW, HV_NORW), HVCMD_READ_SIGCAL);
    hvSetControlAndData(HVCTL(HV_ADDR, HV_LOW, HV_NORW), addr);
    hvSetControlAndData(HVCTL(HV_NONE, highLow, HV_READ), 0);
    return hvSetControlAndData(HVCTL(HV_NONE, highLow, HV_NORW), 0);
}

uint8_t hvspReadSignature(stkReadFuseHvsp_t *param)
{
    return hvReadSignatureInternal(param->fuseAddress, HV_LOW);
}

uint8_t hvspReadOsccal(void)
{
    return hvReadSignatureInternal(0, HV_HIGH);
}

#endif


