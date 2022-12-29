/**
 * @file gitcon.c
 * @author @s-grundner
 * @brief
 * @version 0.1
 * @date 2022-12-23
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "gitcon.h"

static const char *TAG = "gitcon";

// ------------------------------------------------------------
// ISR
// ------------------------------------------------------------

static void IRAM_ATTR dma_task(void *arg)
{
	gitcon_handle_t handle = (gitcon_handle_t)arg;
	for (;;)
	{
		// Do DMA here

		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

static void IRAM_ATTR dsp_task(void *arg)
{
	midi_status_t status = MIDI_STATUS_NOTE_OFF;

	fft_config_t *fft_config = fft_init(FFT_SIZE, FFT_REAL, FFT_FORWARD, NULL, NULL);
	gitcon_handle_t handle = (gitcon_handle_t)arg;
	for (;;)
	{
		// Do DSP here

		fft_execute(fft_config);

		// ------------------------------------------------------------
		// DSP STEPS
		// ------------------------------------------------------------

		// 1. read ADC to DMA buffer
		// 2. analyze audio data (FFT, etc.)
		// 3. detect fundamental frequencies and convert to note number on piano roll
		// 4. detect if frequency is transient
		// 4.1 save note on transient
		// 5. check if already on notes are below a certain threshold
		// 5.1 delete saved note
		// 6. send saved notes to MIDI queue

		fft_destroy(fft_config);
		vTaskDelay(1000 / portTICK_PERIOD_MS);

		// toggle note on/off for testing purposes
		status = (status == MIDI_STATUS_NOTE_ON) ? MIDI_STATUS_NOTE_OFF : MIDI_STATUS_NOTE_ON;

		// send processed audio data to MIDI queue
		ESP_LOGI(TAG, "Sending MIDI message from DSP task");

		// send dummy message
		midi_message_t message = {
			.status = status,
			.channel = 0,
			.param1 = 0x3C, // C4
			.param2 = 0x7F};

		xQueueSend(handle->midi_queue, &message, portMAX_DELAY);
	}
}

static void midi_task(void *arg)
{
	gitcon_handle_t handle = (gitcon_handle_t)arg;
	midi_message_t message;
	for (;;)
	{
		if (xQueueReceive(handle->midi_queue, &message, portMAX_DELAY) == pdTRUE)
		{
			// Send MIDI here
			ESP_LOGI(TAG, "Receiving MIDI message and sending to UART");
			midi_send(MIDI_UART, &message);
		}
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

// ------------------------------------------------------------
// non-static functions
// ------------------------------------------------------------

esp_err_t gitcon_init(gitcon_context_t **out_handle)
{
	esp_log_level_set(TAG, GITCON_LOG_LEVEL);

	gitcon_context_t *cfg = (gitcon_context_t *)malloc(sizeof(gitcon_context_t));
	if (!cfg)
		return ESP_ERR_NO_MEM;

#ifdef USE_MCP3201
	// ------------------------------------------------------------
	// SPI
	// ------------------------------------------------------------
	spi_bus_config_t bus_cfg = {
		.miso_io_num = SPI_MISO,
		.mosi_io_num = SPI_MOSI,
		.sclk_io_num = SPI_SCLK,
		.max_transfer_sz = 32,
	};
	spi_bus_initialize(SPI_DEV, &bus_cfg, DMA_CHAN);

	// ------------------------------------------------------------
	// MCP3201 (ADC)
	// ------------------------------------------------------------

	mcp3201_config_t adc_cfg = {
		.host = SPI_DEV,
		.cs_io = SPI_CS,
		.miso_io = SPI_MISO,
		.mosi_io = SPI_MOSI};

	mcp3201_handle_t adc_handle;
	ESP_ERROR_CHECK(mcp3201_init(&adc_handle, &adc_cfg));
	cfg->mcp3201 = adc_handle;
#endif

	// ------------------------------------------------------------
	// SETUP INTERNAL I2S ADC
	// ------------------------------------------------------------

#ifdef USE_INTERNAL_ADC
	i2s_config_t i2s_cfg = {
		.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
		.sample_rate = 40000,
		.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
		.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
		.communication_format = I2S_COMM_FORMAT_STAND_I2S,
		.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
		.dma_buf_count = 4,
		.dma_buf_len = 1024,
		.use_apll = false,
		.fixed_mclk = 0};

	ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM_0, &i2s_cfg, 0, NULL));
	ESP_ERROR_CHECK(i2s_set_adc_mode((adc_unit_t)INTERNAL_ADC_UNIT, (adc1_channel_t)INTERNAL_ADC));
	ESP_ERROR_CHECK(i2s_adc_enable(I2S_NUM_0));
#endif

	// ------------------------------------------------------------
	// MIDI
	// ------------------------------------------------------------

	ESP_ERROR_CHECK(midi_init(MIDI_UART, MIDI_BAUD, MIDI_TX, MIDI_RX));

	// ------------------------------------------------------------
	// INIT RTOS
	// ------------------------------------------------------------

	// Create a queue to send midi messages whenever a note is detected
	if (!(cfg->midi_queue = xQueueCreate(10, sizeof(midi_message_t))))
		return ESP_ERR_NO_MEM;

	// Midi task: receives midi messages from DSP task and sends them to UART
	if (xTaskCreatePinnedToCore(midi_task, "midi_task", 2048, cfg, 5, NULL, 0) == pdFALSE)
		return ESP_ERR_NO_MEM;
	// DMA task: reads audio data from ADC and sends it to DSP task
	if (xTaskCreatePinnedToCore(dma_task, "dma_task", 2048, cfg, 5, NULL, 0) == pdFALSE)
		return ESP_ERR_NO_MEM;
	// DSP task: receives audio data from DMA task and sends midi messages to midi task
	if (xTaskCreatePinnedToCore(dsp_task, "dsp_task", 2048, cfg, 5, NULL, 1) == pdFALSE)
		return ESP_ERR_NO_MEM;

	*out_handle = cfg;
	return ESP_OK;
}

esp_err_t gitcon_exit(gitcon_handle_t handle)
{
	ESP_ERROR_CHECK(midi_exit(MIDI_UART));

#ifdef USE_MCP3201
	ESP_ERROR_CHECK(mcp3201_exit(handle->mcp3201));
#endif

#ifdef USE_INTERNAL_ADC
	ESP_ERROR_CHECK(i2s_driver_uninstall(I2S_NUM_0));
#endif

	free(handle);
	return ESP_OK;
}