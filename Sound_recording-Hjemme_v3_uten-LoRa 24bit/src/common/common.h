
#ifndef __COMMON_H
#define __COMMON_H

#include "app_cfg.h"

/**
 * @brief Get device uinique Id of esp in hex type
 * 
 * @param deviceId output buffer
 * @param max_len size of output buffer
 * 
 * @return None
 */
void common_getUniqueId_hex(uint8_t *deviceId);

/**
 * @brief Get device uinique Id of esp in string type
 * 
 * @param deviceId output buffer
 * @param max_len size of output buffer
 * 
 * @return None
 */
void common_getUniqueId_string(char *deviceId, int max_len);

#endif // __COMMON_H