# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS
        "main.c"
        "connect.c"
        "toogleLed.c"
        "mqtt_main.c"

    INCLUDE_DIRS
        "."

    PRIV_REQUIRES
        mqtt
        driver
        nvs_flash
        esp_wifi
        esp_netif
        esp_event
)
