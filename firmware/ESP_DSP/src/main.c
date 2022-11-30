#define NFFT 8192
#define F_ABT 44100

#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <math.h>
#include "fft.h"

float test_buffer[NFFT];
float fft_buffer[NFFT];
float magnitude[NFFT / 2];
float frequency[NFFT / 2];
float keyNR[NFFT/2];
float ratio = (float)F_ABT / (float)NFFT;

void init_test_buffer()
{
    for (int i = 0; i < NFFT; i++)
    {
        test_buffer[i] = sin(2 * M_PI * 440 * i/F_ABT) + sin(2*M_PI * 1108.7 * i/F_ABT); // sin(2 * PI * f * 1/t)
        // 3.3 * (sin(2 * M_PI * 440 * i/1000 ) + sin(2 * M_PI * i / 1)) + 1.666
        printf("%f\n", test_buffer[i]);
    }
}

void app_main(void)
{
    init_test_buffer();
    fft_config_t *real_fft_plan = fft_init(NFFT, FFT_REAL, FFT_FORWARD, test_buffer, fft_buffer);

    fft_execute(real_fft_plan);

    //printf("ratio: %f\n", ratio);

    //printf("DC component : %f\n", fft_buffer[0] / NFFT); // DC is at [0]

    for (int k = 1; k < NFFT / 2; k++)
    {
        magnitude[k] = 2 * sqrt(pow(fft_buffer[2 * k], 2) + pow(fft_buffer[2 * k + 1], 2)) / NFFT;
        frequency[k] = k * ratio;
        keyNR[k] = log2(frequency[k]/440)*12+49;
        // printf("%d-th freq : %f+(%f)j\n", k, fft_buffer[2*k], fft_buffer[2*k+1]);
        // printf("%f\t(%f)j\n", fft_buffer[2*k], fft_buffer[2*k+1]);
        if (magnitude[k] >= 0.5)
        {
            //printf("%d-th magnitude: %f => corresponds to %f Hz\n", k, magnitude[k] ,frequency[k]);
            //printf("keyNR: %d\n", (int)round(keyNR[k]));
        }
        //printf("%f\n", magnitude[k]);
    }
    //printf("Middle component : %f\n", fft_buffer[1]); // N/2 is real and stored at [1]

    fft_destroy(real_fft_plan);
}
