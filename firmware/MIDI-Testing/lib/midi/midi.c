/**
 * @file midi.c
 * @author @s-grundner
 * @brief MIDI driver for ESP32
 * @version 0.1
 * @date 2022-12-23
 *
 * @copyright Copyright (c) 2022
 *
 */
#include "midi.h"

#define MIDI_BYTE_SIZE_DEFAULT 3
#define MIDI_BYTE_SIZE_SHORT 2

static const char *TAG = "MIDI";

typedef struct midi_context_t
{
	midi_config_t cfg;
} midi_context_t;

esp_err_t midi_init(midi_context_t **out_ctx, midi_config_t *cfg)
{
	esp_log_level_set(TAG, MIDI_LOG_LEVEL);
	midi_context_t *ctx = (midi_context_t *)malloc(sizeof(midi_context_t));
	if (!ctx)
		return ESP_ERR_NO_MEM;

	*ctx = (midi_context_t){
		.cfg = *cfg};

	// print config
	ESP_LOGI(TAG, "MIDI config:%d, %d, %d, %d",
			 ctx->cfg.uart_num, ctx->cfg.baudrate,
			 ctx->cfg.rx_io, ctx->cfg.tx_io);

	gpio_config_t rx_pin_config = {
		.pin_bit_mask = (1ULL << ctx->cfg.rx_io),
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE};

	gpio_config_t tx_pin_config = {
		.pin_bit_mask = (1ULL << ctx->cfg.tx_io),
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE};

	ESP_ERROR_CHECK(gpio_config(&rx_pin_config));
	ESP_ERROR_CHECK(gpio_config(&tx_pin_config));

	uart_config_t uart_config = {
		.baud_rate = ctx->cfg.baudrate,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_APB};

	// Configure UART parameters
	ESP_ERROR_CHECK(uart_param_config(ctx->cfg.uart_num, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(ctx->cfg.uart_num, ctx->cfg.tx_io, ctx->cfg.rx_io, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
	ESP_ERROR_CHECK(uart_driver_install(ctx->cfg.uart_num, 1024 * 2, 1024 * 2, 0, NULL, 0));

	*out_ctx = ctx;
	return 0;
}

esp_err_t midi_exit(midi_handle_t midi_handle)
{
	esp_err_t err = ESP_OK;
	err = uart_driver_delete(midi_handle->cfg.uart_num);
	free(midi_handle);
	return err;
}

esp_err_t midi_send(midi_handle_t handle, midi_message_t *msg)
{
	// ------------------------------------------------------------
	// SEND MIDI MESSAGE
	// ------------------------------------------------------------
	int len = 0;

	switch (msg->status)
	{
	case MIDI_STATUS_NOTE_OFF:
	case MIDI_STATUS_NOTE_ON:
	case MIDI_STATUS_CONTROL_CHANGE:
	case MIDI_STATUS_PITCH_BEND:
	case MIDI_STATUS_POLYPHONIC_KEY_PRESSURE:
		len = uart_write_bytes(handle->cfg.uart_num, (const char *)msg, MIDI_BYTE_SIZE_DEFAULT);
		break;
	case MIDI_STATUS_PROGRAM_CHANGE:
	case MIDI_STATUS_CHANNEL_PRESSURE:
		len = uart_write_bytes(handle->cfg.uart_num, (const char *)msg, MIDI_BYTE_SIZE_SHORT);
		break;
	default:
		ESP_LOGE(TAG, "midi_send: invalid status");
		return ESP_ERR_INVALID_ARG;
		break;
	}
	if (len == -1)
	{
		ESP_LOGE(TAG, "uart_write_bytes failed");
		return ESP_FAIL;
	}

	return ESP_OK;
}