#include <stdio.h>
#include "bme280.h"
#include "driver/ledc.h"
#include "hd44780.h"
#include "i2c_bus.h"
#include "pms5003.h"

esp_err_t setup_bme280(bme280_handle_t *bme280)
{
	i2c_bus_handle_t i2c_bus;
	i2c_config_t i2c_config = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = GPIO_NUM_21,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_io_num = GPIO_NUM_22,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = 100000,
	};

	i2c_bus = i2c_bus_create(I2C_NUM_0, &i2c_config);
	*bme280 = bme280_create(i2c_bus, BME280_I2C_ADDRESS_DEFAULT);
	return bme280_default_init(*bme280);
}

esp_err_t setup_pms5003(pms5003_t *pms5003)
{
	*pms5003 = (pms5003_t) {
		.rx = GPIO_NUM_16,
		.uart_num = UART_NUM_2
	};

	return pms5003_init(pms5003);
}

esp_err_t hd44780_lcd_init(hd44780_t *hd44780)
{
	esp_err_t ret;
	ledc_timer_config_t ledc_timer = {
		.speed_mode = LEDC_HIGH_SPEED_MODE,
		.duty_resolution = LEDC_TIMER_10_BIT,
		.timer_num = LEDC_TIMER_0,
		.freq_hz = 5000,
		.clk_cfg = LEDC_AUTO_CLK
	};
	ledc_channel_config_t ledc_channel = {
		.gpio_num = GPIO_NUM_4,
		.speed_mode = ledc_timer.speed_mode,
		.channel = LEDC_CHANNEL_0,
		.intr_type = LEDC_INTR_DISABLE,
		.timer_sel = ledc_timer.timer_num,
		.duty = 650
	};

	*hd44780 = (hd44780_t) {
		.write_cb = NULL,
		.pins = {
			.rs = GPIO_NUM_32,
			.e = GPIO_NUM_14,
			.d4 = GPIO_NUM_33,
			.d5 = GPIO_NUM_25,
			.d6 = GPIO_NUM_26,
			.d7 = GPIO_NUM_27,
			.bl = HD44780_NOT_USED
		},
		.font = HD44780_FONT_5X8,
		.lines = 4,
		.backlight = 0
	};

	ret = hd44780_init(hd44780);
	if (ret != ESP_OK)
		return ret;

	ret = ledc_timer_config(&ledc_timer);
	if (ret != ESP_OK)
		return ret;

	ret = ledc_channel_config(&ledc_channel);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}

void app_main(void)
{
	bme280_handle_t bme280;
	pms5003_t pms5003;
	hd44780_t hd44780;

	ESP_ERROR_CHECK(setup_pms5003(&pms5003));
	ESP_ERROR_CHECK(setup_bme280(&bme280));
	ESP_ERROR_CHECK(hd44780_lcd_init(&hd44780));

	ESP_ERROR_CHECK(hd44780_puts(&hd44780, "Powering on.."));
	vTaskDelay(pdMS_TO_TICKS(30000));

	while (1) {
		// esp_err_t ret;
		char buf[21];
		pms5003_data_t data = {0};
		float temperature, humidity, pressure;

		temperature = humidity = pressure = 0;
		bme280_read_temperature(bme280, &temperature);
		bme280_read_humidity(bme280, &humidity);
		bme280_read_pressure(bme280, &pressure);
		pms5003_read_data(&pms5003, &data);

		hd44780_clear(&hd44780);
		snprintf(buf, sizeof buf, "PM2.5: %u ""\xE4""g/m3", data.pm2_5);
		hd44780_puts(&hd44780, buf);

		hd44780_gotoxy(&hd44780, 0, 1);
		snprintf(buf, sizeof buf, " PM10: %u ""\xE4""g/m3", data.pm10);
		hd44780_puts(&hd44780, buf);

		hd44780_gotoxy(&hd44780, 0, 2);
		snprintf(buf, sizeof buf, "    T: %.0f""\xDF""C RH: %.0f%%",
			temperature, humidity);
		hd44780_puts(&hd44780, buf);

		hd44780_gotoxy(&hd44780, 0, 3);
		snprintf(buf, sizeof buf, "    p: %.1f mb", pressure);
		hd44780_puts(&hd44780, buf);

		vTaskDelay(pdMS_TO_TICKS(60000));
	}
}
