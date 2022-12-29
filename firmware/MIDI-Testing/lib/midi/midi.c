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

#define MIDI_UART_DATA_SIZE 3
#define MIDI_UART_SHORT_DATA_SIZE 2
static const char *TAG = "midi";

esp_err_t midi_init(const uart_port_t uart_port, int baudrate, gpio_num_t rx_pin, gpio_num_t tx_pin)
{
	esp_log_level_set(TAG, MIDI_LOG_LEVEL);

	// ------------------------------------------------------------
	// INIT UART DRIVER
	// ------------------------------------------------------------

	gpio_config_t rx_pin_config = {
		.pin_bit_mask = (1ULL << rx_pin),
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE};

	gpio_config_t tx_pin_config = {
		.pin_bit_mask = (1ULL << tx_pin),
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE};

	ESP_ERROR_CHECK(gpio_config(&rx_pin_config));
	ESP_ERROR_CHECK(gpio_config(&tx_pin_config));

	uart_config_t uart_config = {
		.baud_rate = baudrate,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_APB,
	}; // Configure UART parameters

	ESP_ERROR_CHECK(uart_param_config(uart_port, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(uart_port, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
	ESP_ERROR_CHECK(uart_driver_install(uart_port, 1024 * 2, 1024 * 2, 0, NULL, 0));

	return ESP_OK;
}

esp_err_t midi_pitch_bend(const uart_port_t uart_port, uint8_t channel, uint16_t value)
{
	// ------------------------------------------------------------
	// SEND MIDI PITCH BEND
	// ------------------------------------------------------------

	midi_message_t message = {
		.status = MIDI_STATUS_PITCH_BEND,
		.channel = channel,
		.param1 = value & 0x7F,
		.param2 = (value >> 7) & 0x7F};
	return midi_send(uart_port, &message);
}

esp_err_t midi_send(const uart_port_t uart_port, midi_message_t *message)
{
	// ------------------------------------------------------------
	// SEND MIDI MESSAGE
	// ------------------------------------------------------------

	ESP_LOGD(TAG, "midi_send: status: %02X, channel: %02X, param1: %02X, param2: %02X", message->status, message->channel, message->param1, message->param2);
	int len = 0;

	switch (message->status)
	{
	case MIDI_STATUS_NOTE_OFF:
	case MIDI_STATUS_NOTE_ON:
	case MIDI_STATUS_CONTROL_CHANGE:
	case MIDI_STATUS_PITCH_BEND:
	case MIDI_STATUS_POLYPHONIC_KEY_PRESSURE:
		len = uart_write_bytes(uart_port, (const char *)message, MIDI_UART_DATA_SIZE);
		break;
	case MIDI_STATUS_PROGRAM_CHANGE:
	case MIDI_STATUS_CHANNEL_PRESSURE:
		len = uart_write_bytes(uart_port, (const char *)message, MIDI_UART_SHORT_DATA_SIZE);
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

esp_err_t midi_exit(const uart_port_t uart_port)
{
	return uart_driver_delete(uart_port);
}