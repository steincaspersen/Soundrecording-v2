
#include "tx_task.h"
#include "driver/recordWav.h"
#include "driver/recordRaw.h"
#include "driver/sdcard.h"
#include "driver/appI2S.h"
#include "seek/seekTable.h"
#include "lora/lora_sx1262.h"
#include "driver/app_pm.h"

#if CONFIG_MODE_RX == false

CONFIG_LOG_TAG(TX_TASK)
extern char device_uid_str[32];
/* bytes array to store audio data */
static uint8_t audio_buf[CONFIG_MIC_SAMPLE_RATE * CONFIG_MIC_SAMPLE_BIT / 8] = {0};

/* implement seek */
#define CONFIG_SEEK_IMPLEMENT_NUM           2
RTC_IRAM_ATTR uint32_t g_seek_result_num = 0;
static uint8_t g_seek_implemented[CONFIG_SEEK_IMPLEMENT_NUM] = {
    ENUM_SEEK_1_INDEX,
    ENUM_SEEK_2_INDEX
};


/* fileName needs to start by '/' */
static void save_audio_toSDCard(char *fileName, uint8_t *data, uint32_t data_len);
/* fileName needs to start by '/' */
static void save_audioWav_toSDCard(char *fileName, uint8_t *data,
            uint32_t data_len, uint8_t *header, int header_len);
static void write_seekResult_toSdcard(float *seek_crestfactor, uint8_t num, char *uid);


void tx_task(void *pArg)
{
    DBG_OUT_I("app_task started\r\n");

    while (1) {
        FUNC_DELAY_MS(10);

		DBG_OUT_I("started to record audio\r\n");
		/* start to record and saving to ram (audio_buf) */
		recordRaw_toRam_start(audio_buf, sizeof(audio_buf));
		DBG_OUT_I("Record Done!\r\n");

		/* save to file .csv */
		char seekFilePath[64] = {0};
		snprintf(seekFilePath, sizeof(seekFilePath), "/raw_audio_%08d.csv", g_seek_result_num);
		DBG_OUT_I("save audio buffer to file \"%s\"\r\n", seekFilePath);
		save_audio_toSDCard(seekFilePath, audio_buf, sizeof(audio_buf));
		DBG_OUT_I("Finished\r\n");

		/* implement seek */
		int seek_count = 0;
		float g_seek_crestfactor[CONFIG_SEEK_IMPLEMENT_NUM] = {0.0f};

		while (seek_count < CONFIG_SEEK_IMPLEMENT_NUM) {
			DBG_OUT_RAW_I("=====> Doing Seek%d\r\n", g_seek_implemented[seek_count]);

			/* implement seek do_calc function */
			if (g_seekTable[g_seek_implemented[seek_count]].func.func_do_calc != nullptr) {
				g_seekTable[g_seek_implemented[seek_count]].func.func_do_calc(
						&g_seekTable[g_seek_implemented[seek_count]].data, audio_buf, sizeof(audio_buf));
				/* check warning/alarm */
				if (g_seekTable[g_seek_implemented[seek_count]].func.func_warningAlarm != nullptr) {
					g_seekTable[g_seek_implemented[seek_count]].func.func_warningAlarm(
							&g_seekTable[g_seek_implemented[seek_count]].data);
					// write_seekResult_toSdcard(g_seek_implemented[seek_count], device_uid_str);
					g_seek_crestfactor[seek_count] = g_seekTable[g_seek_implemented[seek_count]].data.crestFacter;
				}
			}
			
			seek_count++;
		}

		FUNC_LORA_POWER_ON();

		/* get result and saving to sdcard */
		write_seekResult_toSdcard(g_seek_crestfactor, CONFIG_SEEK_IMPLEMENT_NUM, device_uid_str);

		/* send to lora */
		uint8_t dataToLora[64] = {0};
		uint8_t dataToLora_len = 0;
		for (uint8_t i = 0; i < strlen(device_uid_str); i++) {
			dataToLora[dataToLora_len++] = device_uid_str[i];
		}
		memcpy(&dataToLora[dataToLora_len], &g_seek_result_num, sizeof(g_seek_result_num)); /* seek result num */
		dataToLora_len += sizeof(g_seek_result_num);

		for (uint8_t i = 0; i < CONFIG_SEEK_IMPLEMENT_NUM; i++) {
			memcpy(&dataToLora[dataToLora_len], &g_seek_crestfactor[i], sizeof(g_seek_crestfactor[i]));
			dataToLora_len += sizeof(g_seek_crestfactor[i]);
		}
		
		DBG_OUT("Lora TX: len: %d, data\r\n", dataToLora_len);
		for (uint16_t i = 0; i < dataToLora_len; i++) {
			DBG_OUT_RAW("%02X ", dataToLora[i]);
		}
		DBG_OUT_RAW("\r\n");
		if (g_lora_sx1262.send(dataToLora, dataToLora_len) == false) {
			DBG_OUT_E("failed to send data via lora\r\n");
		} else {
			DBG_OUT_I("send OK\r\n");
		}

		g_seek_result_num++;

		/* Deep sleep */
		FUNC_LED_STATUS_OFF();
		FUNC_LORA_POWER_OFF();
		appI2S_stop();
		sdcard_deinit();
		app_pm_deep_sleep();
    }
}


/* fileName needs to start by '/' */
static void save_audio_toSDCard(char *fileName, uint8_t *data, uint32_t data_len)
{
    File file;
    bool rc = sdcard_openToWrie(&file, (const char *)fileName);
    if (rc == false) {
        DBG_OUT_E("failed to open \"%s\" for writing\r\n", fileName);
        return;
    }

    uint32_t wrote_len = 0;
    char buf[32] = {0};
    do {
		int32_t value = ((int32_t)data[wrote_len + 0] << 16) |
				((int32_t)data[wrote_len + 1] << 8) | ((int32_t)data[wrote_len + 2] << 0);
        int len = snprintf(buf, sizeof(buf), "%d\r\n", value);
        if (sdcard_writeStream(file, (uint8_t *)buf, len) != len) {
            DBG_OUT_E("wrote error at bytes %d\r\n", wrote_len);
        } else {
            // DBG_OUT("wrote: %s\r\n", buf);
            wrote_len += 2;
        }
    } while (wrote_len < data_len);

    DBG_OUT_I("Wrote audio data to file \"%s\"\r\n", fileName);

    sdcard_closeFile(file);
}


/* fileName needs to start by '/' */
static void save_audioWav_toSDCard(char *fileName, uint8_t *data,
            uint32_t data_len, uint8_t *header, int header_len)
{
    File file;
    bool rc = sdcard_openToWrie(&file, (const char *)fileName);
    if (rc == false) {
        DBG_OUT_E("failed to open \"%s\" for writing\r\n", fileName);
        return;
    }

    /* write header */
    sdcard_writeStream(file, header, header_len);

    uint32_t wrote_len = 0;
    do {
        int availen = ((data_len - wrote_len) >= CONFIG_RECORD_FRAME)?
                    CONFIG_RECORD_FRAME : (data_len - wrote_len);
        sdcard_writeStream(file, &data[wrote_len], availen);
        wrote_len += availen;
    } while (wrote_len < data_len);

    DBG_OUT_I("Wrote audio data to file \"%s\"\r\n", fileName);

    sdcard_closeFile(file);
}


static void write_seekResult_toSdcard(float *seek_crestfactor, uint8_t num, char *uid)
{
    char buf[128] = {0};
    int buf_len = snprintf(buf, sizeof(buf), "%s,%d", uid, g_seek_result_num);
    
    for (uint8_t i = 0; i < num; i++) {
        buf_len += snprintf(&buf[buf_len], sizeof(buf) - buf_len, ",%f", seek_crestfactor[i]);
    }

    buf[buf_len++] = '\r';
    buf[buf_len++] = '\n';

    char filePath[128] = {0};
    DBG_OUT_I("seek to file: %s\r\n", buf);
    snprintf(filePath, sizeof(filePath), "/seek_result_num_%04d.csv", g_seek_result_num);
    if (sdcard_writeFile((const char *)filePath, (uint8_t *)buf, buf_len) == buf_len) {
        DBG_OUT_I("Wrote seek result to file \"%s\"\r\n", filePath);
    } else {
        DBG_OUT_E("failed to write seek result to file\r\n");
    }
}

#endif // #if CONFIG_MODE_RX == false