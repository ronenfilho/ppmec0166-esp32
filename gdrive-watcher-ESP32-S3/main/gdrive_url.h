#ifndef GDRIVE_URL_H
#define GDRIVE_URL_H

#include <stdbool.h>
#include <stddef.h>
#include "source_provider.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Reconhece as formas usuais de link do Google Drive e extrai o ID:
 *   - drive.google.com/file/d/<ID>/view?...           -> GDRIVE_FILE
 *   - drive.google.com/open?id=<ID>                    -> GDRIVE_FILE
 *   - drive.google.com/uc?id=<ID>&export=download      -> GDRIVE_FILE
 *   - drive.google.com/drive/folders/<ID>?...          -> GDRIVE_FOLDER
 *   - drive.google.com/drive/u/<n>/folders/<ID>        -> GDRIVE_FOLDER
 *   - docs.google.com/{document,spreadsheets,presentation}/d/<ID>/edit
 *                                                       -> GDRIVE_FILE
 *   - um ID solto (10-64 caracteres [A-Za-z0-9_-])      -> GDRIVE_FILE
 * Devolve false se nao reconhecer nenhuma forma.
 */
bool gdrive_url_match(const char *url, source_kind_t *kind, char *id_out, size_t id_len);

#ifdef __cplusplus
}
#endif

#endif /* GDRIVE_URL_H */
