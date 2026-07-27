/*
 * avrOffLinePgm.c - AVR offline programming helper
 *
 * The compressed device table lives in avrDeviceConst.c.
 * This module only uses avrFindDeviceByIndex()/avrFindDeviceByName(),
 * keeping the constant-table internals private to avrDeviceConst.
 */
#include "avrOffLinePgm.h"
#include <string.h>

void avr_init(AvrOfflineCtx* ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->state = OFP_IDLE;
}

int avr_select_device(AvrOfflineCtx *ctx, uint16_t idx)
{
    if (avrFindDeviceByIndex(idx, &ctx->device_params) != 0) return -1;
    ctx->device_index = idx;
    ctx->state = OFP_CONNECTED;
    return 0;
}

int avr_select_by_signature(AvrOfflineCtx *ctx, const uint8_t sig[3])
{
    uint16_t idx;
    avr_prog_params_t p;

    if (sig == NULL) return -1;

    for (idx = 0; avrFindDeviceByIndex(idx, &p) == 0; idx++)
    {
        if (p.signature[0] == sig[0] &&
            p.signature[1] == sig[1] &&
            p.signature[2] == sig[2])
        {
            ctx->device_index = idx;
            ctx->device_params = p;
            ctx->state = OFP_CONNECTED;
            return 0;
        }
    }
    return -1;
}

int avr_get_param_packet(AvrOfflineCtx *ctx, uint8_t *buf, uint16_t sz)
{
    AVRPART *p;
    uint16_t pos;
    uint32_t fs;
    int i;

    if (ctx->state == OFP_IDLE || sz < 100) return -1;
    p = &ctx->device_params;

    pos = 0;
    buf[pos++] = 0x01;
    for (i = 0; i < 3; i++) buf[pos++] = p->signature[i];
    buf[pos++] = p->stk500_devcode;
    buf[pos++] = p->avr910_devcode;
    buf[pos++] = p->timeout;
    buf[pos++] = p->stabdelay;
    buf[pos++] = p->cmdexedelay;
    buf[pos++] = p->synchloops;
    buf[pos++] = p->bytedelay;
    buf[pos++] = p->pollvalue;
    buf[pos++] = p->pollindex;
    buf[pos++] = p->chip_erase_delay & 0xFF;
    buf[pos++] = (p->chip_erase_delay >> 8) & 0xFF;
    buf[pos++] = p->flash_page_size & 0xFF;
    buf[pos++] = (p->flash_page_size >> 8) & 0xFF;
    fs = p->flash_size;
    buf[pos++] = fs & 0xFF;
    buf[pos++] = (fs >> 8) & 0xFF;
    buf[pos++] = (fs >> 16) & 0xFF;
    buf[pos++] = (fs >> 24) & 0xFF;
    buf[pos++] = p->eeprom_size & 0xFF;
    buf[pos++] = (p->eeprom_size >> 8) & 0xFF;
    buf[pos++] = p->fuse_count;
    for (i = 0; i < AVR_MEM_MAX; i++) {
        buf[pos++] = p->mem[i].size & 0xFF;
        buf[pos++] = (p->mem[i].size >> 8) & 0xFF;
        buf[pos++] = p->mem[i].page_size & 0xFF;
        buf[pos++] = (p->mem[i].page_size >> 8) & 0xFF;
        buf[pos++] = p->mem[i].readsize;
        buf[pos++] = p->mem[i].delay;
        buf[pos++] = p->mem[i].flags;
        buf[pos++] = p->mem[i].mode;
    }
    for (i = 0; i < AVR_OP_MAX; i++) {
        buf[pos++] = p->op[i].cmd[0];
        buf[pos++] = p->op[i].cmd[1];
        buf[pos++] = p->op[i].cmd[2];
        buf[pos++] = p->op[i].cmd[3];
        buf[pos++] = p->op[i].data_index;
    }
    return pos;
}

int avr_make_enter_progmode_packet(AvrOfflineCtx *ctx, uint8_t *buf)
{
    AVRPART *p;
    int i;

    if (ctx->state == OFP_IDLE) return -1;
    p = &ctx->device_params;

    buf[0] = 0x10;
    buf[1] = p->timeout;
    buf[2] = p->stabdelay;
    buf[3] = p->cmdexedelay;
    buf[4] = p->synchloops;
    buf[5] = p->bytedelay;
    buf[6] = p->pollvalue;
    buf[7] = p->pollindex;
    for (i = 0; i < 4; i++) buf[8 + i] = p->op[AVR_OP_PGM_ENABLE].cmd[i];
    return 12;
}

int avr_make_leave_progmode_packet(AvrOfflineCtx *ctx, uint8_t *buf)
{
    AVRPART *p;

    if (ctx->state == OFP_IDLE) return -1;
    p = &ctx->device_params;

    buf[0] = 0x11;
    buf[1] = p->predelay;
    buf[2] = p->postdelay;
    return 3;
}

int avr_make_chip_erase_packet(AvrOfflineCtx *ctx, uint8_t *buf)
{
    AVRPART *p;
    int i;

    if (ctx->state == OFP_IDLE) return -1;
    p = &ctx->device_params;

    buf[0] = 0x12;
    buf[1] = p->chip_erase_delay / 1000;
    buf[2] = p->pollmethod;
    for (i = 0; i < 4; i++) buf[3 + i] = p->op[AVR_OP_CHIP_ERASE].cmd[i];
    return 7;
}
