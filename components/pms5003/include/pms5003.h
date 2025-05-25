#ifndef _PMS5003_H_
#define _PMS5003_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/gpio.h"
#include "driver/uart.h"

struct pms5003 {
	gpio_num_t rx;
	uart_port_t uart_num;
};

struct pms5003_data {
	uint16_t pm1_0;
	uint16_t pm2_5;
	uint16_t pm10;
};

typedef struct pms5003 pms5003_t;
typedef struct pms5003_data pms5003_data_t;

esp_err_t pms5003_init(pms5003_t *sens);
esp_err_t pms5003_destroy(pms5003_t *sens);
esp_err_t pms5003_read_data(pms5003_t *sens, pms5003_data_t *data);

#ifdef __cplusplus
}
#endif

#endif
