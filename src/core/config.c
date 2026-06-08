#include "config.h"
#include <cJSON.h>
#include <cJSON_Utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct egw_conf {
    cJSON *root;
    cJSON *cur;
};

/* ── 生命周期 ────────────────────────────────────── */

egw_err_t egw_conf_load(const char *path, egw_conf_t **out) {
    if (!path || !out) {
        return EGW_ERR_HANDLER;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return EGW_ERR_FILE_NOT_FOUND;
    }

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (len <= 0) {
        fclose(fp);
        return EGW_ERR_PARSE;
    }

    char *buf = malloc((size_t)len + 1);
    if (!buf) {
        fclose(fp);
        return EGW_ERR_HANDLER;
    }

    size_t nread = fread(buf, 1, (size_t)len, fp);
    fclose(fp);
    buf[nread] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        return EGW_ERR_PARSE;
    }
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return EGW_ERR_PARSE;
    }

    egw_conf_t *cfg = malloc(sizeof(egw_conf_t));
    if (!cfg) {
        cJSON_Delete(root);
        return EGW_ERR_HANDLER;
    }

    cfg->root = root;
    cfg->cur = root;
    *out = cfg;
    return EGW_OK;
}

void egw_conf_free(egw_conf_t *cfg) {
    if (!cfg) {
        return;
    }
    if (cfg->root) {
        cJSON_Delete(cfg->root);
    }
    free(cfg);
}

/* ── 位置导航 ────────────────────────────────────── */

egw_err_t egw_conf_enter(egw_conf_t *cfg, const char *key_path) {
    if (!cfg || !key_path) {
        return EGW_ERR_HANDLER;
    }

    cJSON *item = cJSONUtils_GetPointer(cfg->root, key_path);
    if (!item) {
        return EGW_ERR_MISSING_KEY;
    }

    cfg->cur = item;
    return EGW_OK;
}

void egw_conf_leave(egw_conf_t *cfg) {
    if (cfg) {
        cfg->cur = cfg->root;
    }
}

/* ── 取值函数 ────────────────────────────────────── */

const char *egw_conf_get_string(egw_conf_t *cfg, const char *key_path, const char *def) {
    if (!cfg) {
        return def;
    }

    cJSON *item = cJSONUtils_GetPointer(cfg->cur, key_path);
    if (!item) {
        return def;
    }
    if (!cJSON_IsString(item)) {
        return def;
    }
    return cJSON_GetStringValue(item);
}

int egw_conf_get_int(egw_conf_t *cfg, const char *key_path, int def) {
    if (!cfg) {
        return def;
    }

    cJSON *item = cJSONUtils_GetPointer(cfg->cur, key_path);
    if (!item) {
        return def;
    }
    if (!cJSON_IsNumber(item)) {
        return def;
    }
    return (int)cJSON_GetNumberValue(item);
}

bool egw_conf_get_bool(egw_conf_t *cfg, const char *key_path, bool def) {
    if (!cfg) {
        return def;
    }

    cJSON *item = cJSONUtils_GetPointer(cfg->cur, key_path);
    if (!item) {
        return def;
    }
    if (!cJSON_IsBool(item)) {
        return def;
    }
    return cJSON_IsTrue(item);
}

int egw_conf_array_length(egw_conf_t *cfg, const char *key_path, int def) {
    if (!cfg) {
        return def;
    }

    cJSON *item = cJSONUtils_GetPointer(cfg->cur, key_path);
    if (!item) {
        return def;
    }
    if (!cJSON_IsArray(item)) {
        return def;
    }
    return cJSON_GetArraySize(item);
}