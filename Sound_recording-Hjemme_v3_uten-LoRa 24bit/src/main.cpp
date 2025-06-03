
#include <SPI.h>
#include "app_cfg.h"
#include "driver/sdcard.h"
#include "driver/appI2S.h"
#include "lora/lora_sx1262.h"
#include "common/common.h"
#include "driver/app_pm.h"

#include "task/cli_task.h"
#include "task/rx_task.h"
#include "task/tx_task.h"

CONFIG_LOG_TAG(MAIN)
char device_uid_str[64] = CONFIG_DEVICE_UID;
uint8_t device_uid_hex[6] = {0};

SPIClass *g_spi_1 = NULL;
SPIClass *g_spi_2 = NULL;


void setup() {
    CONFIG_DEBUG_PORT.begin(115200);
    DBG_OUT_RAW_I("\r\n********* Sound Recording v%s *********\r\n", CONFIG_FW_VERSION);
    DBG_OUT_RAW_I("Flash size: %dMB flash. \r\n\r\n\033[0m", ESP.getFlashChipSize() / (1024 * 1024));

    // memset(device_uid_str, 0, sizeof(device_uid_str));
    // common_getUniqueId_hex(device_uid_hex);
    // common_getUniqueId_string(device_uid_str, sizeof(device_uid_str));
    DBG_OUT_I("Device UID: %s\r\n", device_uid_str);

	/* Power management */
	app_pm_config();
	app_pm_get_wakeup_cause();

	/* I/O */
	pinMode(CONFIG_LED_PIN, OUTPUT | PULLUP);
	pinMode(CONFIG_LORA_POWER_PIN, OUTPUT | PULLUP);
	FUNC_LED_STATUS_OFF();
	FUNC_LORA_POWER_OFF();

    /* init SPI */
    g_spi_1 = new SPIClass(FSPI);
    g_spi_2 = new SPIClass(HSPI);
    g_spi_1->begin(CONFIG_SDCARD_CLK_PIN, CONFIG_SDCARD_MISO_PIN,
            CONFIG_SDCARD_MOSI_PIN);
    g_spi_2->begin(CONFIG_LORA_SPI_CLK_PIN, CONFIG_LORA_SPI_MISO_PIN,
            CONFIG_LORA_SPI_MOSI_PIN);

    /* init SD Card */
    sdcard_init(CONFIG_SDCARD_CS_PIN, *g_spi_1);

    /* init lora */
    g_lora_sx1262.init(*g_spi_2);

    /* init I2S-PDM mic */
    appI2S_init();
    appI2S_stop();

    /* create tasks */
#if CONFIG_MODE_RX == false
    xTaskCreate(tx_task     , "tx_task"         , (8  * 1024)   , NULL  , 5, NULL);
#else
    xTaskCreate(rx_task     , "rx_task"         , (8  * 1024)   , NULL  , 5, NULL);
#endif // #if CONFIG_MODE_RX == false
#if (CONFIG_CLI_ENABLED == 1)
    xTaskCreate(cliTask     , "cliTask"         , (4 * 1024)    , NULL  , 5, NULL);
#endif // CONFIG_CLI_ENABLED
}


void loop() {
    vTaskDelete(NULL);
    FUNC_DELAY_MS(5000);
}
