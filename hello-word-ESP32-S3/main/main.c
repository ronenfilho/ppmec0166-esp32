#include <stdio.h>
#include <unistd.h>
#include "esp_chip_info.h"
#include "esp_idf_version.h"

void app_main(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    printf("Hello world do ESP32-S3!\n");
    printf("Chip: modelo %d, %d nucleo(s), revisao v%d.%d\n",
           chip_info.model, chip_info.cores,
           chip_info.revision / 100, chip_info.revision % 100);
    printf("ESP-IDF: %s\n", esp_get_idf_version());

    while (true)
    {
        printf("Hello from app_main!\n");
        sleep(1);
    }
}
