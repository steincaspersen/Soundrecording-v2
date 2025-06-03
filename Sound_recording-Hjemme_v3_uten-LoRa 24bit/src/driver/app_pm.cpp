
#include <time.h>
#include <sys/time.h>
#include "esp_sleep.h"
#include "driver/uart.h"
#include "driver/rtc_io.h"
#include "rom/rtc.h"
#include "rom/ets_sys.h"
#include "soc/rtc_cntl_reg.h"
//#include "soc/rtc_io_reg.h"
#include "soc/uart_reg.h"
#include "soc/timer_group_reg.h"

#include "esp_pm.h"
#include "app_pm.h"

CONFIG_LOG_TAG(APP_PM)
static RTC_DATA_ATTR struct timeval sleep_enter_time;
RTC_DATA_ATTR wake_up_cause_t wake_up_cause = WAKEUP_FROM_OTHER;

static void RTC_IRAM_ATTR wake_stub( void );


void app_pm_config(void)
{
#if CONFIG_PM_ENABLE
    // Configure dynamic frequency scaling:
    // maximum and minimum frequencies are set in sdkconfig,
    // automatic light sleep is enabled if tickless idle support is enabled.
	// rtc_cpu_freq_t max_freq;
	// rtc_clk_cpu_freq_from_mhz(CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ, &max_freq);

	esp_pm_config_esp32c3_t pm_config =
    {
		.max_freq_mhz = CONFIG_ESP32C3_DEFAULT_CPU_FREQ_MHZ,
		.min_freq_mhz = 40,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
		.light_sleep_enable = true
#endif
    };
    ESP_ERROR_CHECK( esp_pm_configure(&pm_config) );
#endif // CONFIG_PM_ENABLE
}


/**
 * @brief Get wakeup cause
 * 
 * @param None
 * 
 * @return None
 */
void app_pm_get_wakeup_cause(void)
{
	const char* wakeup_reason;
	switch (esp_sleep_get_wakeup_cause()) {
		case ESP_SLEEP_WAKEUP_EXT0: {
			wakeup_reason = "EXT0";
			wake_up_cause = WAKEUP_FROM_EXT;
			break;
		}

		case ESP_SLEEP_WAKEUP_EXT1: {
			wakeup_reason = "EXT1";
			wake_up_cause = WAKEUP_FROM_EXT;
			break;
		}

		case ESP_SLEEP_WAKEUP_TIMER: {
			wakeup_reason = "TIMER";
			wake_up_cause = WAKEUP_FROM_TIMER;
			break;
		}

		case ESP_SLEEP_WAKEUP_TOUCHPAD: {
			wakeup_reason = "TOUCHPAD";
			wake_up_cause = WAKEUP_FROM_TOUCH;
			break;
		}

		case ESP_SLEEP_WAKEUP_GPIO: {
			wakeup_reason = "GPIO";
			wake_up_cause = WAKEUP_FROM_GPIO;
			break;
		}

		default: {
			wakeup_reason = "OTHER";
			wake_up_cause = WAKEUP_FROM_OTHER;
			break;
		}
	}

	struct timeval now;
	gettimeofday(&now, NULL);
	int sleep_time_ms = (now.tv_sec - sleep_enter_time.tv_sec) * 1000 + (now.tv_usec - sleep_enter_time.tv_usec) / 1000;
	DBG_OUT("wakeup from: %s, slept for %d ms\r\n", wakeup_reason, sleep_time_ms);
}


/**
 * @brief doing light sleep
 * 
 * @param None
 * 
 * @return None
 */
void app_pm_light_sleep(void)
{
	// disable wakeup source
	// esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

	DBG_OUT("Enter light sleep\n");
	wake_up_cause = WAKEUP_FROM_OTHER;
	FUNC_DELAY_MS(20);
	/* Get timestamp before entering sleep */
	gettimeofday(&sleep_enter_time, NULL);

	/* To make sure the complete line is printed before entering sleep mode,
	 * need to wait until UART TX FIFO is empty:
	 */
	uart_wait_tx_idle_polling(CONFIG_ESP_CONSOLE_UART_NUM);

	/* Enter sleep mode */
	esp_light_sleep_start();
	// esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
}


/**
 * @brief doing light sleep
 * 
 * @param None
 * 
 * @return None
 */
void app_pm_deep_sleep(void)
{
	// disable wakeup source
	esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

	/* RTC wakeup */
	ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(CONFIG_WAKEUP_PERIOD_S * 1000000));

#if CONFIG_IDF_TARGET_ESP32
    // Isolate GPIO12 pin from external circuits. This is needed for modules
    // which have an external pull-up resistor on GPIO12 (such as ESP32-WROVER)
    // to minimize current consumption.
    rtc_gpio_isolate(GPIO_NUM_12);
#endif

	DBG_OUT("Enter deep sleep\n");
	wake_up_cause = WAKEUP_FROM_OTHER;
	/* Get timestamp before entering sleep */
	gettimeofday(&sleep_enter_time, NULL);
	// rtc_gpio_pulldown_en(PIR_SS_1_PIN);

	/* To make sure the complete line is printed before entering sleep mode,
	 * need to wait until UART TX FIFO is empty:
	 */
	uart_wait_tx_idle_polling(CONFIG_ESP_CONSOLE_UART_NUM);

	/* Enter sleep mode */
	esp_deep_sleep_start();
}