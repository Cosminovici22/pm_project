#include <stdio.h>
#include "bme280.h"
#include "driver/ledc.h"
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "hd44780.h"
#include "i2c_bus.h"
#include "nvs_flash.h"
#include "pms5003.h"

#define EXAMPLE_ESP_WIFI_SSID "Cospot"
#define EXAMPLE_ESP_WIFI_PASS "I'll never tell"

#define PM2_5_BP_0 0.0
#define PM2_5_BP_1 9.0
#define PM2_5_BP_2 35.5
#define PM2_5_BP_3 55.5
#define PM2_5_BP_4 125.5
#define PM2_5_BP_5 225.5
#define PM10_BP_0 0.0
#define PM10_BP_1 55.0
#define PM10_BP_2 155.0
#define PM10_BP_3 255.0
#define PM10_BP_4 355.0
#define PM10_BP_5 425.0

#define AQI_SUB(BP_LOW, BP_HIGH, C_PM) (50 / ((BP_HIGH) - (BP_LOW)) * (C_PM \
	- (BP_LOW)) + (int) (BP_LOW) / 50 * 50)
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static volatile uint8_t connected = 0;
static uint8_t retryc = 0;

static float features[24];
static uint8_t featurec = 0;

static int get_feature_data(size_t offset, size_t length, float *out_ptr) {
	memcpy(out_ptr, features + offset, length * sizeof(float));
	return 0;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
	int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT) {
		if (event_id == WIFI_EVENT_STA_START) {
			esp_wifi_connect();
		} else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
			if (retryc++ < 5)
				esp_wifi_connect();
			else
				connected = 1;
		}

	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		retryc = 0;
		connected = -1;
	}
}

static esp_err_t setup_wifi(void)
{
	esp_err_t ret;
	wifi_init_config_t cfg;
	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	wifi_config_t wifi_config = { .sta = {
		.ssid = EXAMPLE_ESP_WIFI_SSID,
		.password = EXAMPLE_ESP_WIFI_PASS,
		.threshold = { .authmode = WIFI_AUTH_WPA2_PSK }
	}};

	ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES
		|| ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ret = nvs_flash_erase();
		if (ret != ESP_OK)
			return ret;

		ret = nvs_flash_init();
		if (ret != ESP_OK)
			return ret;
	}

	ret = esp_netif_init();
	if (ret != ESP_OK)
		return ret;

	ret = esp_event_loop_create_default();
	if (ret != ESP_OK)
		return ret;

	esp_netif_create_default_wifi_sta();

	cfg = WIFI_INIT_CONFIG_DEFAULT();
	ret = esp_wifi_init(&cfg);
	if (ret != ESP_OK)
		return ret;

	ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
		&wifi_event_handler, NULL, &instance_any_id);
	if (ret != ESP_OK)
		return ret;

	ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
		&wifi_event_handler, NULL, &instance_got_ip);
	if (ret != ESP_OK)
		return ret;

	ret = esp_wifi_set_mode(WIFI_MODE_STA);
	if (ret != ESP_OK)
		return ret;

	ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
	if (ret != ESP_OK)
		return ret;

	ret = esp_wifi_start();
	if (ret != ESP_OK)
		return ret;

	while (connected == 0)
		;

	if (connected > 0)
		printf("Successfully connected to %s\n", EXAMPLE_ESP_WIFI_SSID);
	else
		printf("Failed to connect to %s\n", EXAMPLE_ESP_WIFI_SSID);

	return ESP_OK;
}

static esp_err_t http_get_handler(httpd_req_t *req)
{
	esp_err_t ret;
	signal_t signal;
	ei_impulse_result_t result;
	char buf[1024];
	float aqi;

	signal.total_length = featurec;
	signal.get_data = &get_feature_data;
	if (run_classifier(&signal, &result, false) != EI_IMPULSE_OK)
		return ESP_FAIL;
	aqi = result.classification[0].value;

	snprintf(buf, sizeof buf, "AQI in the next hour: %.0f\n", aqi);
	ret = httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}

static httpd_uri_t uri_get = {
	.uri = "/",
	.method = HTTP_GET,
	.handler = http_get_handler,
	.user_ctx = NULL
};

static esp_err_t setup_http_server(httpd_handle_t *server)
{
	esp_err_t ret;
	httpd_config_t config;

	config = HTTPD_DEFAULT_CONFIG();

	ret = httpd_start(server, &config);
	if (ret != ESP_OK)
		return ret;

	ret = httpd_register_uri_handler(*server, &uri_get);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}

static esp_err_t setup_bme280(bme280_handle_t *bme280)
{
	i2c_bus_handle_t i2c_bus;
	i2c_config_t i2c_config = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = GPIO_NUM_21,
		.scl_io_num = GPIO_NUM_22,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master = { .clk_speed = 100000 }
	};

	i2c_bus = i2c_bus_create(I2C_NUM_0, &i2c_config);
	*bme280 = bme280_create(i2c_bus, BME280_I2C_ADDRESS_DEFAULT);
	return bme280_default_init(*bme280);
}

static esp_err_t setup_pms5003(pms5003_t *pms5003)
{
	*pms5003 = (pms5003_t) {
		.rx = GPIO_NUM_16,
		.uart_num = UART_NUM_2
	};

	return pms5003_init(pms5003);
}

static esp_err_t hd44780_lcd_init(hd44780_t *hd44780)
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

static uint16_t aqi_from(const pms5003_data_t *data)
{
	uint16_t pm2_5_index, pm10_index;

	pm2_5_index
		= data->pm2_5 < PM2_5_BP_1
		? AQI_SUB(PM2_5_BP_0, PM2_5_BP_1, data->pm2_5)
		: data->pm2_5 >= PM2_5_BP_1 && data->pm2_5 < PM2_5_BP_2
		? AQI_SUB(PM2_5_BP_1, PM2_5_BP_2, data->pm2_5)
		: data->pm2_5 >= PM2_5_BP_2 && data->pm2_5 < PM2_5_BP_3
		? AQI_SUB(PM2_5_BP_2, PM2_5_BP_3, data->pm2_5)
		: data->pm2_5 >= PM2_5_BP_3 && data->pm2_5 < PM2_5_BP_4
		? AQI_SUB(PM2_5_BP_3, PM2_5_BP_4, data->pm2_5)
		: AQI_SUB(PM2_5_BP_4, PM2_5_BP_5, data->pm2_5);
	pm10_index
		= data->pm10 < PM10_BP_1
		? AQI_SUB(PM10_BP_0, PM10_BP_1, data->pm10)
		: data->pm10 >= PM10_BP_1 && data->pm10 < PM10_BP_2
		? AQI_SUB(PM10_BP_1, PM10_BP_2, data->pm10)
		: data->pm10 >= PM10_BP_2 && data->pm10 < PM10_BP_3
		? AQI_SUB(PM10_BP_2, PM10_BP_3, data->pm10)
		: data->pm10 >= PM10_BP_3 && data->pm10 < PM10_BP_4
		? AQI_SUB(PM10_BP_3, PM10_BP_4, data->pm10)
		: AQI_SUB(PM10_BP_4, PM10_BP_5, data->pm10);

	return MAX(pm2_5_index, pm10_index);
}

extern "C" void app_main(void)
{
	bme280_handle_t bme280;
	pms5003_t pms5003;
	hd44780_t hd44780;
   	httpd_handle_t server;

	ESP_ERROR_CHECK(setup_wifi());
	ESP_ERROR_CHECK(setup_http_server(&server));
	ESP_ERROR_CHECK(setup_pms5003(&pms5003));
	ESP_ERROR_CHECK(setup_bme280(&bme280));
	ESP_ERROR_CHECK(hd44780_lcd_init(&hd44780));

	ESP_ERROR_CHECK(hd44780_puts(&hd44780, "Initializing.."));
	vTaskDelay(pdMS_TO_TICKS(30000));

	for (uint8_t minutes = 0;; minutes++) {
		// esp_err_t ret;
		char buf[21];
		pms5003_data_t data = {0};
		float temperature, humidity, pressure;

		temperature = humidity = pressure = 0;
		bme280_read_temperature(bme280, &temperature);
		bme280_read_humidity(bme280, &humidity);
		bme280_read_pressure(bme280, &pressure);
		pms5003_read_data(&pms5003, &data);

		if (minutes == 60) {
			minutes = 0;

			if (featurec == sizeof features / sizeof *features)
				memmove(features, features + 1, --featurec);

			features[featurec++] = aqi_from(&data);
		}

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
