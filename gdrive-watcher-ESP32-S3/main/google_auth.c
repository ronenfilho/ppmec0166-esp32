#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "google_auth.h"
#include "http_util.h"
#include "link_store.h"
#include "secrets.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "mbedtls/sha256.h"
#include "mbedtls/base64.h"

static const char *TAG = "AUTH";

#define ACCESS_TOKEN_LEN  2048
#define REFRESH_TOKEN_LEN 512
#define SCOPE_LEN         160
#define VERIFIER_LEN      65
#define DEVICE_CODE_LEN   160

#define SCOPE_DEVICE  "https://www.googleapis.com/auth/drive.file"
#define SCOPE_DESKTOP "https://www.googleapis.com/auth/drive.readonly"
#define REDIRECT_URI  "http://127.0.0.1:8080"

static auth_state_t s_state = AUTH_NONE;
static char s_access_token[ACCESS_TOKEN_LEN];
static char s_refresh_token[REFRESH_TOKEN_LEN];
static char s_scope[SCOPE_LEN];
static int64_t s_access_expires_us = 0;

/* estado do fluxo device */
static char s_device_code[DEVICE_CODE_LEN];
static int64_t s_device_deadline_us = 0;
static int64_t s_device_next_poll_us = 0;
static int s_device_interval_s = 5;

/* estado do fluxo PKCE */
static char s_code_verifier[VERIFIER_LEN];

static bool is_desktop_scope(const char *scope)
{
    return strstr(scope, "drive.readonly") != NULL;
}

static void gen_random_verifier(char *out, size_t len)
{
    static const char charset[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    for (size_t i = 0; i < len - 1; i++)
    {
        out[i] = charset[esp_random() % (sizeof(charset) - 1)];
    }
    out[len - 1] = '\0';
}

static void base64url_from_bytes(const unsigned char *bytes, size_t n, char *out, size_t out_len)
{
    unsigned char tmp[128];
    size_t olen = 0;
    mbedtls_base64_encode(tmp, sizeof(tmp), &olen, bytes, n);
    size_t j = 0;
    for (size_t i = 0; i < olen && j + 1 < out_len; i++)
    {
        char c = (char)tmp[i];
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
        else if (c == '=') continue; /* strip padding */
        out[j++] = c;
    }
    out[j] = '\0';
}

esp_err_t google_auth_init(void)
{
    store_get_refresh_token(s_refresh_token, sizeof(s_refresh_token));
    store_get_scope(s_scope, sizeof(s_scope));

    if (s_refresh_token[0] != '\0')
    {
        s_state = AUTH_OK;
        s_access_expires_us = 0; /* forca refresh no primeiro uso */
        ESP_LOGI(TAG, "Sessao restaurada da NVS (escopo: %s)", s_scope);
    }
    else
    {
        s_state = AUTH_NONE;
    }
    return ESP_OK;
}

auth_state_t google_auth_state(void)
{
    return s_state;
}

bool google_auth_is_authenticated(void)
{
    return s_state == AUTH_OK;
}

const char *google_auth_scope(void)
{
    return s_scope;
}

/* ---------- device flow ---------- */

esp_err_t google_auth_device_begin(char *user_code, size_t user_code_len,
                                    char *verification_url, size_t url_len)
{
    if (s_state == AUTH_OK)
    {
        return ESP_ERR_INVALID_STATE;
    }

    char scope_enc[128];
    http_url_encode(SCOPE_DEVICE, scope_enc, sizeof(scope_enc));

    char body[256];
    snprintf(body, sizeof(body), "client_id=%s&scope=%s", GOOGLE_CLIENT_ID, scope_enc);

    char *resp = NULL;
    size_t len = 0;
    int status = 0;
    esp_err_t err = http_post_form("https://oauth2.googleapis.com/device/code",
                                    body, &resp, &len, &status);
    if (err != ESP_OK)
    {
        return err;
    }
    if (status < 200 || status >= 300)
    {
        ESP_LOGE(TAG, "device/code HTTP %d: %s", status, resp);
        free(resp);
        return ESP_FAIL;
    }

    cJSON *json = cJSON_Parse(resp);
    free(resp);
    if (json == NULL)
    {
        return ESP_FAIL;
    }

    cJSON *dc = cJSON_GetObjectItem(json, "device_code");
    cJSON *uc = cJSON_GetObjectItem(json, "user_code");
    cJSON *vurl = cJSON_GetObjectItem(json, "verification_url");
    if (vurl == NULL)
    {
        vurl = cJSON_GetObjectItem(json, "verification_uri"); /* nome RFC 8628 padrao */
    }
    cJSON *exp = cJSON_GetObjectItem(json, "expires_in");
    cJSON *iv = cJSON_GetObjectItem(json, "interval");

    if (!cJSON_IsString(dc) || !cJSON_IsString(uc) || !cJSON_IsString(vurl))
    {
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    strncpy(s_device_code, dc->valuestring, sizeof(s_device_code) - 1);
    strncpy(user_code, uc->valuestring, user_code_len - 1);
    strncpy(verification_url, vurl->valuestring, url_len - 1);

    int expires_in = cJSON_IsNumber(exp) ? exp->valueint : 1800;
    s_device_interval_s = cJSON_IsNumber(iv) ? iv->valueint : 5;
    s_device_deadline_us = esp_timer_get_time() + (int64_t)expires_in * 1000000LL;
    s_device_next_poll_us = esp_timer_get_time(); /* pode tentar ja na primeira chamada */

    cJSON_Delete(json);
    s_state = AUTH_DEVICE_PENDING;
    return ESP_OK;
}

static esp_err_t apply_token_response(cJSON *json, const char *fallback_scope)
{
    cJSON *at = cJSON_GetObjectItem(json, "access_token");
    cJSON *rt = cJSON_GetObjectItem(json, "refresh_token");
    cJSON *exp = cJSON_GetObjectItem(json, "expires_in");
    cJSON *scope = cJSON_GetObjectItem(json, "scope");

    if (!cJSON_IsString(at))
    {
        return ESP_FAIL;
    }

    strncpy(s_access_token, at->valuestring, sizeof(s_access_token) - 1);
    int expires_in = cJSON_IsNumber(exp) ? exp->valueint : 3600;
    s_access_expires_us = esp_timer_get_time() + (int64_t)expires_in * 1000000LL;

    if (cJSON_IsString(rt))
    {
        strncpy(s_refresh_token, rt->valuestring, sizeof(s_refresh_token) - 1);
        store_set_refresh_token(s_refresh_token);
    }

    if (cJSON_IsString(scope))
    {
        strncpy(s_scope, scope->valuestring, sizeof(s_scope) - 1);
    }
    else
    {
        strncpy(s_scope, fallback_scope, sizeof(s_scope) - 1);
    }
    store_set_scope(s_scope);

    s_state = AUTH_OK;
    return ESP_OK;
}

esp_err_t google_auth_device_poll_once(void)
{
    if (s_state != AUTH_DEVICE_PENDING)
    {
        return ESP_ERR_INVALID_STATE;
    }

    int64_t now = esp_timer_get_time();
    if (now < s_device_next_poll_us)
    {
        return ESP_ERR_NOT_FINISHED; /* ainda nao e hora de bater na API de novo */
    }
    if (now >= s_device_deadline_us)
    {
        s_state = AUTH_NONE;
        return ESP_ERR_TIMEOUT;
    }

    char body[384];
    snprintf(body, sizeof(body),
             "client_id=%s&client_secret=%s&device_code=%s"
             "&grant_type=urn%%3Aietf%%3Aparams%%3Aoauth%%3Agrant-type%%3Adevice_code",
             GOOGLE_CLIENT_ID, GOOGLE_CLIENT_SECRET, s_device_code);

    char *resp = NULL;
    size_t len = 0;
    int status = 0;
    esp_err_t err = http_post_form("https://oauth2.googleapis.com/token", body, &resp, &len, &status);
    s_device_next_poll_us = esp_timer_get_time() + (int64_t)s_device_interval_s * 1000000LL;

    if (err != ESP_OK)
    {
        return ESP_ERR_NOT_FINISHED; /* falha de rede transitoria -- tenta de novo depois */
    }

    cJSON *json = cJSON_Parse(resp);
    free(resp);
    if (json == NULL)
    {
        return ESP_ERR_NOT_FINISHED;
    }

    cJSON *error = cJSON_GetObjectItem(json, "error");
    if (cJSON_IsString(error))
    {
        const char *e = error->valuestring;
        if (strcmp(e, "authorization_pending") == 0)
        {
            cJSON_Delete(json);
            return ESP_ERR_NOT_FINISHED;
        }
        if (strcmp(e, "slow_down") == 0)
        {
            s_device_interval_s += 5;
            cJSON_Delete(json);
            return ESP_ERR_NOT_FINISHED;
        }
        ESP_LOGE(TAG, "device flow negado: %s", e);
        cJSON_Delete(json);
        s_state = AUTH_NONE;
        return ESP_FAIL;
    }

    esp_err_t ok = apply_token_response(json, SCOPE_DEVICE);
    cJSON_Delete(json);
    return ok;
}

/* ---------- PKCE flow ---------- */

esp_err_t google_auth_pkce_begin(char *auth_url_out, size_t len)
{
    if (s_state == AUTH_OK)
    {
        return ESP_ERR_INVALID_STATE;
    }

    gen_random_verifier(s_code_verifier, sizeof(s_code_verifier));

    unsigned char digest[32];
    mbedtls_sha256((const unsigned char *)s_code_verifier, strlen(s_code_verifier), digest, 0);

    char challenge[64];
    base64url_from_bytes(digest, sizeof(digest), challenge, sizeof(challenge));

    char scope_enc[160];
    http_url_encode(SCOPE_DESKTOP, scope_enc, sizeof(scope_enc));
    char redirect_enc[64];
    http_url_encode(REDIRECT_URI, redirect_enc, sizeof(redirect_enc));

    snprintf(auth_url_out, len,
             "https://accounts.google.com/o/oauth2/v2/auth"
             "?client_id=%s&redirect_uri=%s&response_type=code&scope=%s"
             "&code_challenge=%s&code_challenge_method=S256"
             "&access_type=offline&prompt=consent",
             GOOGLE_DESKTOP_CLIENT_ID, redirect_enc, scope_enc, challenge);

    s_state = AUTH_PKCE_PENDING;
    return ESP_OK;
}

esp_err_t google_auth_pkce_finish(const char *code)
{
    if (s_state != AUTH_PKCE_PENDING)
    {
        return ESP_ERR_INVALID_STATE;
    }

    char code_enc[256];
    http_url_encode(code, code_enc, sizeof(code_enc));
    char redirect_enc[64];
    http_url_encode(REDIRECT_URI, redirect_enc, sizeof(redirect_enc));

    char body[700];
    snprintf(body, sizeof(body),
             "client_id=%s&client_secret=%s&code=%s&code_verifier=%s"
             "&redirect_uri=%s&grant_type=authorization_code",
             GOOGLE_DESKTOP_CLIENT_ID, GOOGLE_DESKTOP_CLIENT_SECRET,
             code_enc, s_code_verifier, redirect_enc);

    char *resp = NULL;
    size_t len = 0;
    int status = 0;
    esp_err_t err = http_post_form("https://oauth2.googleapis.com/token", body, &resp, &len, &status);
    if (err != ESP_OK)
    {
        return err;
    }
    if (status < 200 || status >= 300)
    {
        ESP_LOGE(TAG, "PKCE token exchange HTTP %d: %s", status, resp);
        free(resp);
        s_state = AUTH_NONE;
        return ESP_FAIL;
    }

    cJSON *json = cJSON_Parse(resp);
    free(resp);
    if (json == NULL)
    {
        s_state = AUTH_NONE;
        return ESP_FAIL;
    }

    esp_err_t ok = apply_token_response(json, SCOPE_DESKTOP);
    cJSON_Delete(json);
    if (ok != ESP_OK)
    {
        s_state = AUTH_NONE;
    }
    return ok;
}

/* ---------- comuns ---------- */

esp_err_t google_auth_refresh(void)
{
    if (s_refresh_token[0] == '\0')
    {
        return ESP_ERR_INVALID_STATE;
    }

    bool desktop = is_desktop_scope(s_scope);
    const char *cid = desktop ? GOOGLE_DESKTOP_CLIENT_ID : GOOGLE_CLIENT_ID;
    const char *csecret = desktop ? GOOGLE_DESKTOP_CLIENT_SECRET : GOOGLE_CLIENT_SECRET;

    char body[700];
    snprintf(body, sizeof(body),
             "client_id=%s&client_secret=%s&refresh_token=%s&grant_type=refresh_token",
             cid, csecret, s_refresh_token);

    char *resp = NULL;
    size_t len = 0;
    int status = 0;
    esp_err_t err = http_post_form("https://oauth2.googleapis.com/token", body, &resp, &len, &status);
    if (err != ESP_OK)
    {
        return err;
    }
    if (status < 200 || status >= 300)
    {
        ESP_LOGE(TAG, "refresh HTTP %d: %s", status, resp);
        free(resp);
        return ESP_FAIL;
    }

    cJSON *json = cJSON_Parse(resp);
    free(resp);
    if (json == NULL)
    {
        return ESP_FAIL;
    }

    /* refresh nao devolve novo refresh_token -- mantem o que ja temos */
    cJSON *at = cJSON_GetObjectItem(json, "access_token");
    cJSON *exp = cJSON_GetObjectItem(json, "expires_in");
    if (!cJSON_IsString(at))
    {
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    strncpy(s_access_token, at->valuestring, sizeof(s_access_token) - 1);
    int expires_in = cJSON_IsNumber(exp) ? exp->valueint : 3600;
    s_access_expires_us = esp_timer_get_time() + (int64_t)expires_in * 1000000LL;
    s_state = AUTH_OK;

    cJSON_Delete(json);
    return ESP_OK;
}

const char *google_auth_get_bearer(void)
{
    if (s_state != AUTH_OK)
    {
        return NULL;
    }

    int64_t margin_us = 60LL * 1000000LL;
    if (esp_timer_get_time() >= s_access_expires_us - margin_us)
    {
        if (google_auth_refresh() != ESP_OK)
        {
            return NULL;
        }
    }
    return s_access_token;
}

int google_auth_expires_in(void)
{
    if (s_state != AUTH_OK)
    {
        return 0;
    }
    int64_t remaining = (s_access_expires_us - esp_timer_get_time()) / 1000000LL;
    return remaining > 0 ? (int)remaining : 0;
}

void google_auth_logout(void)
{
    s_state = AUTH_NONE;
    memset(s_access_token, 0, sizeof(s_access_token));
    memset(s_refresh_token, 0, sizeof(s_refresh_token));
    memset(s_scope, 0, sizeof(s_scope));
    s_access_expires_us = 0;
    store_clear_auth();
}
