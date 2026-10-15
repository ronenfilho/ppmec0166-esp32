#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_console.h"
#include "app_time.h"
#include "link_store.h"
#include "google_auth.h"
#include "watcher.h"
#include "source_provider.h"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_netif.h"
#include "argtable3/argtable3.h"

static const char *TAG = "CONSOLE";

/* ---------- add ---------- */

static struct
{
    struct arg_str *url;
    struct arg_end *end;
} add_args;

static int cmd_add(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&add_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, add_args.end, argv[0]);
        return 1;
    }

    const char *url = add_args.url->sval[0];
    source_kind_t kind;
    char id[ITEM_ID_LEN];

    const source_provider_t *p = source_provider_resolve(url, &kind, id, sizeof(id));
    if (p == NULL)
    {
        printf("link nao reconhecido\n");
        return 1;
    }

    store_lock();
    int n = store_link_count();
    for (int i = 0; i < n; i++)
    {
        link_rec_t rec;
        if (store_link_get(i, &rec) == ESP_OK && strcmp(rec.id, id) == 0)
        {
            printf("ja registrado como #%d\n", i);
            store_unlock();
            return 0;
        }
    }
    if (n >= STORE_MAX_LINKS)
    {
        printf("limite de %d links atingido\n", STORE_MAX_LINKS);
        store_unlock();
        return 1;
    }

    int idx = -1;
    esp_err_t err = store_link_add(url, kind, id, &idx);
    store_unlock();
    if (err != ESP_OK)
    {
        printf("erro ao registrar: %s\n", esp_err_to_name(err));
        return 1;
    }

    printf("#%d registrado (%s, id=%s)\n", idx,
           kind == SOURCE_KIND_GDRIVE_FOLDER ? "pasta" : "arquivo", id);
    watcher_request(WATCH_REQ_CHECK, NULL);
    return 0;
}

/* ---------- list ---------- */

static struct
{
    struct arg_int *n;
    struct arg_end *end;
} list_args;

static int cmd_list(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&list_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, list_args.end, argv[0]);
        return 1;
    }

    store_lock();

    if (list_args.n->count > 0)
    {
        int idx = list_args.n->ival[0];
        item_t *items = NULL;
        size_t n = 0;
        if (store_snapshot_load(idx, &items, &n) != ESP_OK)
        {
            printf("link #%d nao encontrado\n", idx);
            store_unlock();
            return 1;
        }
        printf("Link #%d -- %zu item(ns) conhecido(s):\n", idx, n);
        for (size_t i = 0; i < n; i++)
        {
            printf("  %-48.48s %-40.40s %s\n", items[i].name, items[i].mime, items[i].modified);
        }
        free(items);
        store_unlock();
        return 0;
    }

    bool auth = google_auth_is_authenticated();
    printf("Modo: %s\n", auth ? "AUTENTICADO" : "PUBLICO (sem login)");

    printf("Links publicos registrados:\n");
    int n = store_link_count();
    if (n == 0)
    {
        printf("  (nenhum -- use \"add <link>\")\n");
    }
    for (int i = 0; i < n; i++)
    {
        link_rec_t rec;
        if (store_link_get(i, &rec) != ESP_OK)
        {
            continue;
        }
        item_t *items = NULL;
        size_t cnt = 0;
        store_snapshot_load(i, &items, &cnt);
        free(items);

        char when[32] = "nunca";
        if (rec.last_check != 0)
        {
            app_time_format((time_t)rec.last_check, when, sizeof(when));
        }
        printf("  #%d [%s] id=%s itens=%zu ultima_checagem=%s\n  %s\n",
               i, rec.kind == SOURCE_KIND_GDRIVE_FOLDER ? "pasta" : "arquivo",
               rec.id, cnt, when, rec.url);
    }

    if (auth)
    {
        item_t *items = NULL;
        size_t n2 = 0;
        store_drive_snapshot_load(&items, &n2);
        printf("Drive autenticado (escopo: %s) -- %zu item(ns) conhecido(s):\n",
               google_auth_scope(), n2);
        for (size_t i = 0; i < n2; i++)
        {
            printf("  %-48.48s %-40.40s %s\n", items[i].name, items[i].mime, items[i].modified);
        }
        free(items);
    }

    store_unlock();
    return 0;
}

/* ---------- rm ---------- */

static struct
{
    struct arg_int *n;
    struct arg_end *end;
} rm_args;

static int cmd_rm(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&rm_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, rm_args.end, argv[0]);
        return 1;
    }

    store_lock();
    esp_err_t err = store_link_remove(rm_args.n->ival[0]);
    store_unlock();
    if (err != ESP_OK)
    {
        printf("erro: %s\n", esp_err_to_name(err));
        return 1;
    }
    printf("removido\n");
    return 0;
}

/* ---------- check / refresh ---------- */

static int cmd_check(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    watcher_request(WATCH_REQ_CHECK, NULL);
    printf("verificacao solicitada -- resultado aparece nos proximos logs\n");
    return 0;
}

/* ---------- auth ---------- */

static int cmd_auth(int argc, char **argv)
{
    if (argc == 1)
    {
        if (google_auth_is_authenticated())
        {
            printf("ja autenticado -- rode \"logout\" primeiro\n");
            return 1;
        }
        watcher_request(WATCH_REQ_AUTH_DEVICE, NULL);
        printf("iniciando fluxo de dispositivo...\n");
        return 0;
    }

    if (strcmp(argv[1], "url") == 0)
    {
        if (google_auth_is_authenticated())
        {
            printf("ja autenticado -- rode \"logout\" primeiro\n");
            return 1;
        }
        watcher_request(WATCH_REQ_AUTH_PKCE_BEGIN, NULL);
        printf("gerando URL de autorizacao...\n");
        return 0;
    }

    if (strcmp(argv[1], "code") == 0 && argc >= 3)
    {
        watcher_request(WATCH_REQ_AUTH_PKCE_FINISH, argv[2]);
        printf("trocando codigo por token...\n");
        return 0;
    }

    printf("uso: auth | auth url | auth code <codigo>\n");
    return 1;
}

/* ---------- status ---------- */

static int cmd_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta != NULL)
    {
        esp_netif_ip_info_t ip;
        if (esp_netif_get_ip_info(sta, &ip) == ESP_OK)
        {
            printf("Wi-Fi IP: " IPSTR "\n", IP2STR(&ip.ip));
        }
    }

    char now[32];
    app_time_format(app_time_now(), now, sizeof(now));
    printf("Hora local: %s\n", now);

    if (google_auth_is_authenticated())
    {
        printf("Modo: AUTENTICADO (escopo: %s), token expira em %ds\n",
               google_auth_scope(), google_auth_expires_in());
    }
    else
    {
        printf("Modo: PUBLICO (sem login)\n");
    }

    watcher_status_t st;
    watcher_status(&st);
    printf("Links registrados: %d/%d\n", st.link_count, STORE_MAX_LINKS);
    printf("Intervalo de polling: %us\n", (unsigned)st.poll_interval_s);
    printf("Heap livre: %u bytes (minimo historico: %u bytes)\n",
           (unsigned)esp_get_free_heap_size(), (unsigned)esp_get_minimum_free_heap_size());
    return 0;
}

/* ---------- logout ---------- */

static int cmd_logout(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    watcher_request(WATCH_REQ_LOGOUT, NULL);
    printf("logout solicitado\n");
    return 0;
}

/* ---------- interval ---------- */

static struct
{
    struct arg_int *seconds;
    struct arg_end *end;
} interval_args;

static int cmd_interval(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&interval_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, interval_args.end, argv[0]);
        return 1;
    }
    watcher_set_interval((uint32_t)interval_args.seconds->ival[0]);
    printf("intervalo ajustado\n");
    return 0;
}

/* ---------- reset ---------- */

static int cmd_reset(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("zerando tudo e reiniciando...\n");
    fflush(stdout);
    store_factory_reset();
    google_auth_logout();
    esp_restart();
    return 0;
}

esp_err_t app_console_init(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "gdrive>";
    repl_config.max_cmdline_length = 512;
    repl_config.task_stack_size = 6144;

    esp_console_register_help_command();

    add_args.url = arg_str1(NULL, NULL, "<link>", "link publico do Google Drive (arquivo ou pasta)");
    add_args.end = arg_end(2);
    const esp_console_cmd_t add_cmd = {
        .command = "add",
        .help = "Registra um link publico do Drive (incremental)",
        .hint = NULL,
        .func = &cmd_add,
        .argtable = &add_args,
    };
    esp_console_cmd_register(&add_cmd);

    list_args.n = arg_int0(NULL, NULL, "<n>", "indice do link (opcional)");
    list_args.end = arg_end(2);
    const esp_console_cmd_t list_cmd = {
        .command = "list",
        .help = "Mostra os links registrados e o que ja se sabe deles",
        .hint = NULL,
        .func = &cmd_list,
        .argtable = &list_args,
    };
    esp_console_cmd_register(&list_cmd);

    rm_args.n = arg_int1(NULL, NULL, "<n>", "indice do link a remover");
    rm_args.end = arg_end(2);
    const esp_console_cmd_t rm_cmd = {
        .command = "rm",
        .help = "Remove um link registrado",
        .hint = NULL,
        .func = &cmd_rm,
        .argtable = &rm_args,
    };
    esp_console_cmd_register(&rm_cmd);

    const esp_console_cmd_t check_cmd = {
        .command = "check",
        .help = "Forca uma verificacao imediata de novidades",
        .hint = NULL,
        .func = &cmd_check,
    };
    esp_console_cmd_register(&check_cmd);

    const esp_console_cmd_t refresh_cmd = {
        .command = "refresh",
        .help = "Alias de \"check\"",
        .hint = NULL,
        .func = &cmd_check,
    };
    esp_console_cmd_register(&refresh_cmd);

    const esp_console_cmd_t auth_cmd = {
        .command = "auth",
        .help = "auth | auth url | auth code <codigo> -- autenticacao Google",
        .hint = NULL,
        .func = &cmd_auth,
    };
    esp_console_cmd_register(&auth_cmd);

    const esp_console_cmd_t status_cmd = {
        .command = "status",
        .help = "Mostra Wi-Fi, hora, modo de autenticacao e heap",
        .hint = NULL,
        .func = &cmd_status,
    };
    esp_console_cmd_register(&status_cmd);

    const esp_console_cmd_t logout_cmd = {
        .command = "logout",
        .help = "Limpa o login (mantem os links publicos registrados)",
        .hint = NULL,
        .func = &cmd_logout,
    };
    esp_console_cmd_register(&logout_cmd);

    interval_args.seconds = arg_int1(NULL, NULL, "<segundos>", "intervalo de polling (min 30)");
    interval_args.end = arg_end(2);
    const esp_console_cmd_t interval_cmd = {
        .command = "interval",
        .help = "Ajusta o intervalo de polling periodico",
        .hint = NULL,
        .func = &cmd_interval,
        .argtable = &interval_args,
    };
    esp_console_cmd_register(&interval_cmd);

    const esp_console_cmd_t reset_cmd = {
        .command = "reset",
        .help = "Zera tudo (NVS) e reinicia",
        .hint = NULL,
        .func = &cmd_reset,
    };
    esp_console_cmd_register(&reset_cmd);

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    esp_err_t err = esp_console_new_repl_uart(&hw_config, &repl_config, &repl);
#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    esp_err_t err = esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl);
#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    esp_err_t err = esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl);
#else
#error "Console nao suportado nesse target"
#endif

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "falha ao criar o REPL: %s", esp_err_to_name(err));
        return err;
    }

    return esp_console_start_repl(repl);
}
