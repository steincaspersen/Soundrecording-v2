

#include <arduinoFFT.h>
#include "seek2.h"
#include <iostream>
using namespace std;

// Detection parameters
const float lowRange = 3000.0f;
const float highRange = 10000.0f;
const float peakThresholdStd = 2.0f;
float peak ;
float sumSq ;
struct seek2_result {
    float brakeSoundMagnitude;
};

CONFIG_LOG_TAG(SEEK2)


static seek2_result brake_sound_detector(int32_t *samples, uint32_t samples_size,
            float *vReal, uint32_t vReal_size, float *vImag, uint32_t vImag_size);


/* implement seek0 function to analysic array data */
void seek2_do_calc(seek_data_t *seek_data, uint8_t *array, uint32_t size)
{
    if (seek_data == nullptr) {
        DBG_OUT_I("Seek data is null\r\n");
        return;
    }

    seek_data->warningAlarm_triggered = false;
    
    uint32_t samples_size = size / 3;
    float *vReal = (float *)ps_calloc(samples_size * sizeof(float), 1);
    if (vReal == nullptr) {
        DBG_OUT_E("Failed to allocate memory for vReal\r\n");
        return;
    }
    float *vImag = (float *)ps_calloc(samples_size * sizeof(float), 1);
    if (vImag == nullptr) {
        DBG_OUT_E("Failed to allocate memory for vImag\r\n");
        free(vReal);
        return;
    }
    
	int32_t *i32_array = (int32_t *)ps_calloc(samples_size * sizeof(int32_t), 1);
	int i32_array_len = 0;
	for (int i = 0; i < (size - 3); i+=3) {
		i32_array[i32_array_len++] = ((int32_t)array[i + 0] << 16) |
				((int32_t)array[i + 1] << 8) | ((int32_t)array[i + 2] << 0);
	}
    seek2_result result = brake_sound_detector(i32_array, i32_array_len,
                            vReal, i32_array_len, vImag, i32_array_len);
    seek_data->crestFacter = result.brakeSoundMagnitude;

    free(vReal);
    free(vImag);

    /* if triggered warning/alrm. we can change default state of led or no need */
    seek_data->ledRed_isOn = true;
    seek_data->ledYellow_isOn = true;

    /* end */
}


/* check warning/alarm */
void seek2_checkWarningAlarm(seek_data_t *seek_data)
{
    if (seek_data == nullptr) {
        DBG_OUT_I("Seek data is null\r\n");
        return;
    }

    if (seek_data->warningAlarm_triggered == true) {
        /* doing something here if needed. */
        DBG_OUT("Warning/Alarm enabled. Set LED status");
        memset(seek_data->dataToSend, 0, sizeof(seek_data->dataToSend));
        uint8_t newData[] = {1, 2, 3, 4, 5, 6};
        size_t newDataLen = sizeof(newData);
        if (newDataLen <= sizeof(seek_data->dataToSend)) {
            memcpy(seek_data->dataToSend, newData, newDataLen);
            seek_data->dataToSend_len = newDataLen;
        } else {
            DBG_OUT_E("Not enough space to add data to dataToSend\r\n");
        }

        /* create data to send via lora. below is a example */

        /* doing something here if needed */
    }
    /* doing something here if needed */
}


/* send data via lora */
bool seek2_sendToLora(seek_data_t *seek_data)
{
    if (seek_data == nullptr) {
        DBG_OUT_I("Seek data is null\r\n");
        return false;
    }

    /* send to lora */
    /* call lora sending function here */

    /* example:
    
        if (lora_send(seek_data.dataToSend, seek_data.dataToSend_len) == true) {
            DBG_OUT("send data success\r\n");
            return true;
        }
        DBG_OUT_E("Failed to send data via lora\r\n");
    */
   return false;
}


/*** STATIC FUNCTION ***/

// ------ Brake sound detector function ------
static seek2_result brake_sound_detector(int32_t *samples, uint32_t samples_size,
            float *vReal, uint32_t vReal_size, float *vImag, uint32_t vImag_size)
{
    seek2_result result = {0.0f};
    float mean = 0.0f;
    // Copy and preprocess input data
    for (uint32_t i = 0; i < samples_size; i++) {
        mean += samples[i];
    }
    mean /= samples_size;
    cout << "\n Utskrift av mean etter FFT loop" << mean;

    for (uint32_t i = 0; i < samples_size; i++) {
        vReal[i] = (float)samples[i] - mean;
        vImag[i] = 0.0f;
    }
    cout << "\n Utskrift mean: " << mean;

    // Normalize data
    float maxVal = 0.0f;
    for (int i = 0; i < samples_size; i++) {
        float absVal = abs(vReal[i]);
        if (absVal > maxVal) {
            maxVal = absVal;
        }
    }

    if (maxVal != 0.0f) {
        float invMaxVal = 1.0f / maxVal;
        for (int i = 0; i < samples_size; i++) {
            vReal[i] *= invMaxVal;
        }
    }

    // Perform FFT
    ArduinoFFT<float> g_seek2_fft = ArduinoFFT<float>(vReal, vImag, samples_size, CONFIG_MIC_SAMPLE_RATE);
    g_seek2_fft.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    g_seek2_fft.compute(FFT_FORWARD);
    g_seek2_fft.complexToMagnitude();

    // Calculate the indices for lowRange and highRange frequencies
    int lowIndex = static_cast<int>(lowRange * samples_size / CONFIG_MIC_SAMPLE_RATE);
    int highIndex = static_cast<int>(highRange * samples_size / CONFIG_MIC_SAMPLE_RATE);
    int count = highIndex - lowIndex + 1;

    cout << "\n Utskrift lowIndex" << lowIndex;
    cout << "\n Utskrift highIndex" << highIndex;

    // Calculate the sum of magnitudes for the frequency range
    float sumMagnitudes = 0.0f;
    for (int i = lowIndex; i <= highIndex; i++) {
        sumMagnitudes += vReal[i];
    }

    float meanMagnitude = sumMagnitudes / count;

    // Calculate standard deviation
    float stdDev = 0.0f;
    for (int i = lowIndex; i <= highIndex; i++) {
        float diff = vReal[i] - meanMagnitude;
        stdDev += diff * diff;
    }
    stdDev = sqrt(stdDev / count);

    // Aggregate value of peaks in frequency range above threshold
    float peakThreshold = meanMagnitude + peakThresholdStd * stdDev;
    float sumPeakMagnitudes = 0.0f;
    for (int i = lowIndex; i <= highIndex; i++) {
        if (vReal[i] > peakThreshold) {
            sumPeakMagnitudes += vReal[i];
        }
    }
  //ONLY FOR TESTING **********
    //Compute the crest factor of normalized samples
    //float seek1_computeCrestFactor(float *samples, uint32_t samples_size);

     
   // peak = 0.0f;
   // sumSq = 0.0f;

   // for (size_t i = 0; i < samples_size; i++) {
    //    float absSample = std::fabs(samples[i]);
    //    peak = std::max(peak, absSample);
    //    sumSq += samples[i] * samples[i];
    //}
    
   // peak = peak/ std::sqrt(sumSq / samples_size);

  //  cout << "\n Utskrift peak ( Crest Value)" << peak;

  //   cout << "\n Utskrift stdDev" << stdDev;
    

    result.brakeSoundMagnitude = sumPeakMagnitudes / (highRange - lowRange); // consider dividing by sumMagnitudes;

    cout << "\n Utskrift result.brakesoundMagnitude" << result.brakeSoundMagnitude;
    cout << "\n Utskrift sumPeakMagnitudes " << sumPeakMagnitudes;
  //  cout << "\n Utskrift test ";


    return result;
}
