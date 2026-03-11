#include <stdio.h>
#include "freertos/FreeRTOS.h"

void app_main(void)
{
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    printf("Hello world!\n");
    while (1)
    {

    }
}
