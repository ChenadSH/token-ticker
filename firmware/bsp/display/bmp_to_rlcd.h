#ifndef TOKEN_TICKER_BMP_TO_RLCD_H
#define TOKEN_TICKER_BMP_TO_RLCD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rlcd_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

bool bmp_to_rlcd_write_bitmap(rlcd_driver_t *driver, const uint8_t *bmp, size_t bmp_size);

#ifdef __cplusplus
}
#endif

#endif
