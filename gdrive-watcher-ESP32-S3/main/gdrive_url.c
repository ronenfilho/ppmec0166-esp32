#include <string.h>

#include "gdrive_url.h"
#include "gdrive_api.h"

#define ID_CHARSET "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-"

/* Copia de start até o primeiro '/' ou '?' ou '&' (o que vier primeiro),
 * limitado a id_len-1, e valida o charset/tamanho de um ID de Drive. */
static bool extract_id(const char *start, char *id_out, size_t id_len)
{
    size_t n = strspn(start, ID_CHARSET);
    if (n == 0 || n >= id_len)
    {
        return false;
    }
    if (n < 10 || n > 64)
    {
        return false;
    }
    memcpy(id_out, start, n);
    id_out[n] = '\0';
    return true;
}

bool gdrive_url_match(const char *url, source_kind_t *kind, char *id_out, size_t id_len)
{
    if (url == NULL || kind == NULL || id_out == NULL)
    {
        return false;
    }

    const char *p;

    /* Pasta: drive.google.com/drive/folders/<ID> ou .../u/<n>/folders/<ID> */
    p = strstr(url, "/folders/");
    if (p != NULL)
    {
        *kind = SOURCE_KIND_GDRIVE_FOLDER;
        return extract_id(p + strlen("/folders/"), id_out, id_len);
    }

    /* Arquivo: drive.google.com/file/d/<ID>/... ou docs.google.com/.../d/<ID>/... */
    p = strstr(url, "/d/");
    if (p != NULL)
    {
        *kind = SOURCE_KIND_GDRIVE_FILE;
        return extract_id(p + strlen("/d/"), id_out, id_len);
    }

    /* drive.google.com/open?id=<ID> ou .../uc?id=<ID>&export=download */
    p = strstr(url, "id=");
    if (p != NULL && strstr(url, "google.com") != NULL)
    {
        *kind = SOURCE_KIND_GDRIVE_FILE;
        return extract_id(p + strlen("id="), id_out, id_len);
    }

    /* Nenhum link do Google reconhecido -- tenta como ID solto. */
    if (strstr(url, "google.com") == NULL && strstr(url, "://") == NULL)
    {
        *kind = SOURCE_KIND_GDRIVE_FILE;
        return extract_id(url, id_out, id_len);
    }

    return false;
}

/* Tabela de providers -- hoje so o Google Drive. Para adicionar outro
 * tipo de link no futuro: implementar match_url()/list_items() e somar
 * uma entrada aqui. */
static const source_provider_t g_providers[] = {
    {
        .scheme_name = "gdrive",
        .match_url = gdrive_url_match,
        .list_items = gdrive_list_items,
    },
};

static const size_t g_providers_count = sizeof(g_providers) / sizeof(g_providers[0]);

const source_provider_t *source_provider_resolve(const char *url,
                                                   source_kind_t *kind_out,
                                                   char *id_out, size_t id_len)
{
    for (size_t i = 0; i < g_providers_count; i++)
    {
        if (g_providers[i].match_url(url, kind_out, id_out, id_len))
        {
            return &g_providers[i];
        }
    }
    return NULL;
}
