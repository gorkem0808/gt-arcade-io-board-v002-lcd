#include "config_store.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include <string.h>

#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

static uint32_t calc_checksum(const gt_config_t *cfg) {
    const uint8_t *p = (const uint8_t *)cfg;
    uint32_t sum = 0x12345678u;
    for (uint32_t i = 0; i < sizeof(gt_config_t) - sizeof(uint32_t); i++) {
        sum = (sum << 5) ^ (sum >> 27) ^ p[i];
    }
    return sum;
}

void config_defaults(gt_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->magic = GT_CFG_MAGIC;
    cfg->version = GT_CFG_VERSION;
    cfg->x_min = 200;
    cfg->x_max = 3900;
    cfg->y_min = 200;
    cfg->y_max = 3900;
    cfg->filter_level = 1;
    cfg->checksum = calc_checksum(cfg);
}

void config_load(gt_config_t *cfg) {
    const gt_config_t *saved = (const gt_config_t *)(XIP_BASE + FLASH_TARGET_OFFSET);

    if (saved->magic == GT_CFG_MAGIC && saved->version == GT_CFG_VERSION && saved->checksum == calc_checksum(saved)) {
        memcpy(cfg, saved, sizeof(gt_config_t));
    } else {
        config_defaults(cfg);
    }
}

void config_save(const gt_config_t *cfg_in) {
    gt_config_t cfg = *cfg_in;
    cfg.magic = GT_CFG_MAGIC;
    cfg.version = GT_CFG_VERSION;
    cfg.checksum = calc_checksum(&cfg);

    uint8_t page[FLASH_PAGE_SIZE];
    memset(page, 0xFF, sizeof(page));
    memcpy(page, &cfg, sizeof(cfg));

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, page, FLASH_PAGE_SIZE);
    restore_interrupts(ints);
}
