
#include "common.h"


/**
 * @brief Get device uinique Id of esp in hex type
 * 
 * @param deviceId output buffer
 * @param max_len size of output buffer
 * 
 * @return None
 */
void common_getUniqueId_hex(uint8_t *deviceId)
{
    esp_efuse_mac_get_default(deviceId);
}


/**
 * @brief Get device uinique Id of esp in string type
 * 
 * @param deviceId output buffer
 * @param max_len size of output buffer
 * 
 * @return None
 */
void common_getUniqueId_string(char *deviceId, int max_len)
{
    uint8_t mac_addr[6] = {0};
    esp_efuse_mac_get_default(mac_addr);
    snprintf(deviceId, max_len, "%02X%02X%02X%02X%02X%02X",
            mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
}