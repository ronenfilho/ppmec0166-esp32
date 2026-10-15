# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS
        "main.c"
        "connect.c"
        "app_time.c"
        "http_util.c"
        "gdrive_url.c"
        "gdrive_api.c"
        "google_auth.c"
        "link_store.c"
        "watcher.c"
        "app_console.c"

    INCLUDE_DIRS
        "."

    PRIV_REQUIRES
        esp_http_client
        esp-tls
        mbedtls
        console
        nvs_flash
        esp_timer
        esp_wifi
        esp_netif
        esp_event
        lwip
        json
)
