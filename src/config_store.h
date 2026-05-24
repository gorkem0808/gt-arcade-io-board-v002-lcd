#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <stdint.h>

#define GT_CFG_MAGIC 0x47544932u
#define GT_CFG_VERSION 2u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint16_t x_min;
    uint16_t x_max;
    uint16_t y_min;
    uint16_t y_max;
    uint8_t filter_level; // 0=AZ, 1=ORTA, 2=YUKSEK
    uint8_t reserved[13];
    uint32_t checksum;
} gt_config_t;

void config_defaults(gt_config_t *cfg);
void config_load(gt_config_t *cfg);
void config_save(const gt_config_t *cfg);

#endif
