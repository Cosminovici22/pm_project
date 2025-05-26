#include "pms5003.h"

esp_err_t pms5003_init(pms5003_t *sens)
{
	esp_err_t ret;
	uart_config_t uart_config = {
		.baud_rate = 9600,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE
	};

	ret = uart_param_config(sens->uart_num, &uart_config);
	if (ret != ESP_OK)
		return ret;

	ret = uart_set_pin(sens->uart_num, UART_PIN_NO_CHANGE, sens->rx,
		UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	if (ret != ESP_OK)
		return ret;

	ret = uart_driver_install(sens->uart_num, 129, 0, 0, NULL, 0);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}

esp_err_t pms5003_destroy(pms5003_t *sens)
{
	return uart_driver_delete(sens->uart_num);
}

esp_err_t pms5003_read_data(pms5003_t *sens, pms5003_data_t *data)
{
	esp_err_t ret;
	uint8_t buf[32];
	uint16_t checksum;
	int bytec;

	/* Buffer fills up over time so reads fail, so flush it before read */
	ret = uart_flush_input(sens->uart_num);
	if (ret != ESP_OK)
		return ESP_FAIL;

	bytec = uart_read_bytes(sens->uart_num, buf, sizeof buf, portMAX_DELAY);
	if (bytec != sizeof buf)
		return ESP_FAIL;

	/* First four bytes of payload should always be the same */
	if (*(uint32_t *) buf != 0x1C004D42)
		return ESP_FAIL;

	/* Confirm data was sent correctly */
	checksum = 0;
	for (int i = 0; i < 30; i++)
		checksum += buf[i];
	if ((buf[30] << 8) + buf[31] != checksum)
		return ESP_FAIL;

	/* Change endianness */
	data->pm1_0 = (buf[10] << 8) + buf[11];
	data->pm2_5 = (buf[12] << 8) + buf[13];
	data->pm10 = (buf[14] << 8) + buf[15];

	return ESP_OK;
}
