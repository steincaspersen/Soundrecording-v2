
#ifndef __APP_PM_H
#define __APP_PM_H

#include "app_cfg.h"

typedef enum {
	WAKEUP_FROM_OTHER,
	WAKEUP_FROM_EXT,
	WAKEUP_FROM_GPIO,
	WAKEUP_FROM_TIMER,
	WAKEUP_FROM_TOUCH,
} wake_up_cause_t;

void app_pm_config(void);

/**
 * @brief Get wakeup cause
 * 
 * @param None
 * 
 * @return None
 */
void app_pm_get_wakeup_cause(void);

/**
 * @brief doing light sleep
 * 
 * @param None
 * 
 * @return None
 */
void app_pm_light_sleep(void);

/**
 * @brief doing light sleep
 * 
 * @param None
 * 
 * @return None
 */
void app_pm_deep_sleep(void);

#endif // __APP_PM_H