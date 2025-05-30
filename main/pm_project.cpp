#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "aqi_html.h"
#include "bme280.h"
#include "driver/ledc.h"
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "hd44780.h"
#include "i2c_bus.h"
#include "nvs_flash.h"
#include "pms5003.h"

#define WIFI_SSID "Smart air quality sensor"

/* Various breakpoints for particulate matter used in the computation of AQI */
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

#define I_LOW_0 0
#define I_LOW_1 50
#define I_LOW_2 100
#define I_LOW_3 150
#define I_LOW_4 200
#define I_LOW_5 300

/* Formula for the AQI subindex of a component given its concentration */
#define AQI_SUB(c, c_bp_low, c_bp_high, i_low) \
	(50 * ((c) - (c_bp_low)) / ((c_bp_high) - (c_bp_low)) + (i_low))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define AQI_QUALITY(aqi) \
	((aqi) < 25 \
	? "Very low" \
	: (aqi) < 50 \
	? "Low" \
	: (aqi) < 75 \
	? "Medium" \
	: (aqi) < 100 \
	? "High" \
	: "Very high")

/* Hourly AQI buffer */
static float aqis[24];
static uint8_t aqic = 0;
static pthread_mutex_t aqis_lock;

/* Features buffer used by the predictive model API */
static float features[24];

/* Callback function for predictive model API */
static int get_feature_data(size_t offset, size_t length, float *out_ptr) {
	memcpy(out_ptr, features + offset, length * sizeof(float));
	return 0;
}

static esp_err_t setup_wifi(void)
{
	esp_err_t ret;
	wifi_init_config_t wifi_init_config;
	wifi_config_t wifi_config = { .ap = {
		.ssid = WIFI_SSID,
		.ssid_len = strlen(WIFI_SSID),
		.channel = 1,
		.authmode = WIFI_AUTH_OPEN,
		.max_connection = 10,
		.pmf_cfg = { .required = true }
	}};

	ret = esp_netif_init();
	if (ret != ESP_OK)
		return ret;

	ret = esp_event_loop_create_default();
	if (ret != ESP_OK)
		return ret;

	esp_netif_create_default_wifi_ap();

	wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
	wifi_init_config.nvs_enable = 0;
	ret = esp_wifi_init(&wifi_init_config);
	if (ret != ESP_OK)
		return ret;

	ret = esp_wifi_set_mode(WIFI_MODE_AP);
	if (ret != ESP_OK)
		return ret;

	ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
	if (ret != ESP_OK)
		return ret;

	ret = esp_wifi_start();
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}

/* HTTP GET request handler for the HTTP server */
static esp_err_t http_get_handler(httpd_req_t *req)
{
	static char buf[1350];
	esp_err_t ret;
	uint8_t featurec;

	pthread_mutex_lock(&aqis_lock);
	memcpy(features, aqis, aqic * sizeof *aqis);
	featurec = aqic;
	pthread_mutex_unlock(&aqis_lock);

	/* Predict AQI of the next 24 hours */
	for (uint8_t i = 0; i < 24; i++) {
		signal_t signal;
		ei_impulse_result_t result;

		signal.total_length = featurec;
		signal.get_data = &get_feature_data;
		if (run_classifier(&signal, &result, false) != EI_IMPULSE_OK)
			return ESP_FAIL;

		if (featurec == sizeof features / sizeof *features)
			memmove(features, features + 1, --featurec);

		features[featurec++] = result.classification[0].value;
	}

	snprintf(buf, sizeof buf, AQI_HTML, UNPACK_24(features));
	ret = httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}

/* Setup for HTTP server which answers GET requests with AQI predictions */
static esp_err_t setup_http_server(httpd_handle_t *server)
{
	esp_err_t ret;
	httpd_config_t httpd_config;
	httpd_uri_t uri_get = {
		.uri = "/",
		.method = HTTP_GET,
		.handler = http_get_handler,
		.user_ctx = NULL
	};

	httpd_config = HTTPD_DEFAULT_CONFIG();

	ret = httpd_start(server, &httpd_config);
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
		.rx = GPIO_NUM_16, /* Only the RX buffer is requried */
		.uart_num = UART_NUM_2
	};

	return pms5003_init(pms5003);
}

static esp_err_t hd44780_lcd_init(hd44780_t *hd44780)
{
	esp_err_t ret;
	/* Initialize necessary structures for contrast control using PWM */
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
		.duty = 650 /* Experimental value for an adequate contrast */
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

/* Compute AQI from the given PM measurements */
static uint16_t aqi_from(const pms5003_data_t *data)
{
	uint16_t pm2_5_index, pm10_index;

	pm2_5_index
		= data->pm2_5 < PM2_5_BP_1
		? AQI_SUB(data->pm2_5, PM2_5_BP_0, PM2_5_BP_1, I_LOW_0)
		: data->pm2_5 < PM2_5_BP_2
		? AQI_SUB(data->pm2_5, PM2_5_BP_1, PM2_5_BP_2, I_LOW_1)
		: data->pm2_5 < PM2_5_BP_3
		? AQI_SUB(data->pm2_5, PM2_5_BP_2, PM2_5_BP_3, I_LOW_2)
		: data->pm2_5 < PM2_5_BP_4
		? AQI_SUB(data->pm2_5, PM2_5_BP_3, PM2_5_BP_4, I_LOW_3)
		: AQI_SUB(data->pm2_5, PM2_5_BP_4, PM2_5_BP_5, I_LOW_4);
	pm10_index
		= data->pm10 < PM10_BP_1
		? AQI_SUB(data->pm10, PM10_BP_0, PM10_BP_1, I_LOW_0)
		: data->pm10 < PM10_BP_2
		? AQI_SUB(data->pm10, PM10_BP_1, PM10_BP_2, I_LOW_1)
		: data->pm10 < PM10_BP_3
		? AQI_SUB(data->pm10, PM10_BP_2, PM10_BP_3, I_LOW_2)
		: data->pm10 < PM10_BP_4
		? AQI_SUB(data->pm10, PM10_BP_3, PM10_BP_4, I_LOW_3)
		: AQI_SUB(data->pm10, PM10_BP_4, PM10_BP_5, I_LOW_4);

	return MAX(pm2_5_index, pm10_index);
}

extern "C" void app_main(void)
{
	bme280_handle_t bme280;
	pms5003_t pms5003;
	hd44780_t hd44780;
	httpd_handle_t server;

	ESP_ERROR_CHECK(hd44780_lcd_init(&hd44780));
	ESP_ERROR_CHECK(hd44780_puts(&hd44780, "Initializing.."));

	pthread_mutex_init(&aqis_lock, NULL);

	ESP_ERROR_CHECK(setup_pms5003(&pms5003));
	ESP_ERROR_CHECK(setup_bme280(&bme280));
	ESP_ERROR_CHECK(setup_wifi());
	ESP_ERROR_CHECK(setup_http_server(&server));

	/* Wait 30 seconds for PMS5003 to warm up */
	vTaskDelay(pdMS_TO_TICKS(30000));

	for (uint8_t minutes = 0;; minutes++) {
		char buf[21];
		pms5003_data_t data = {0};
		float temperature, humidity, pressure;
		uint16_t aqi;

		temperature = humidity = pressure = 0;
		bme280_read_temperature(bme280, &temperature);
		bme280_read_humidity(bme280, &humidity);
		bme280_read_pressure(bme280, &pressure);
		pms5003_read_data(&pms5003, &data);
		aqi = aqi_from(&data);

		/* Every hour, append an AQI to the aqis buffer */
		if (minutes == 60) {
			minutes = 0;

			/* Only keep AQI's of past 24 hours */
			pthread_mutex_lock(&aqis_lock);

			if (aqic == sizeof aqis / sizeof *aqis)
				memmove(aqis, aqis + 1, --aqic);

			aqis[aqic++] = aqi;

			pthread_mutex_unlock(&aqis_lock);
		}

		hd44780_clear(&hd44780);
		snprintf(buf, sizeof buf, "AQI: %u %s", aqi, AQI_QUALITY(aqi));
		hd44780_puts(&hd44780, buf);

		hd44780_gotoxy(&hd44780, 0, 1);
		snprintf(buf, sizeof buf, "  T: %.0f""\xDF""C", temperature);
		hd44780_puts(&hd44780, buf);

		hd44780_gotoxy(&hd44780, 0, 2);
		snprintf(buf, sizeof buf, " RH: %.0f%%", humidity);
		hd44780_puts(&hd44780, buf);

		hd44780_gotoxy(&hd44780, 0, 3);
		snprintf(buf, sizeof buf, "  p: %.1f mb", pressure);
		hd44780_puts(&hd44780, buf);

		/* Measure every 60 seconds (recommended for BME280) */
		vTaskDelay(pdMS_TO_TICKS(60000));
	}
}
