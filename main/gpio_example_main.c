#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#include <ds1307.h>

#define GPIO_OUTPUT_IO_0    CONFIG_GPIO_OUTPUT_0
#define GPIO_INPUT_IO_0     CONFIG_GPIO_INPUT_0
#define ESP_INTR_FLAG_DEFAULT 0

static QueueHandle_t gpio_evt_queue = NULL;
static bool led_enable = 0;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task_example(void* arg)
{
    i2c_dev_t dev;
    memset(&dev, 0, sizeof(i2c_dev_t));
    ESP_ERROR_CHECK(ds1307_init_desc(&dev, 0, CONFIG_EXAMPLE_I2C_MASTER_SDA, CONFIG_EXAMPLE_I2C_MASTER_SCL));

    struct tm time = {
        .tm_year = 118, //since 1900 (2018 - 1900)
        .tm_mon  = 3,  // 0-based
        .tm_mday = 11,
        .tm_hour = 0,
        .tm_min  = 52,
        .tm_sec  = 10
    };

    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
        	if (gpio_get_level(io_num) == 0) {
        		led_enable = 1;
        		gpio_set_level(GPIO_OUTPUT_IO_0, 1);
        		ds1307_get_time(&dev, &time);
        		printf("%04d-%02d-%02d %02d:%02d:%02d SENSOR TRIGGERED ", time.tm_year + 1900 /*Add 1900 for better readability*/, time.tm_mon + 1,
        		        			      time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);
        		printf("GPIO[%"PRIu32"] intr, LED ENABLED\n", io_num);
        	}
        	else {
        		if (led_enable == 1) {
        			led_enable = 0;
        			gpio_set_level(GPIO_OUTPUT_IO_0, 0);
        			ds1307_get_time(&dev, &time);
        			printf("%04d-%02d-%02d %02d:%02d:%02d SENSOR TRIGGERED ", time.tm_year + 1900 /*Add 1900 for better readability*/, time.tm_mon + 1,
        			        			      time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);
        			printf("GPIO[%"PRIu32"] intr, LED DISABLED\n", io_num);
        		}
        	}
        }
    }
}

void app_main(void)
{
	/* INIT I2C and DS1307 */
    ESP_ERROR_CHECK(i2cdev_init());

	/* INIT GPIO */
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL<<GPIO_OUTPUT_IO_0);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = (1ULL<<GPIO_INPUT_IO_0);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);

}
