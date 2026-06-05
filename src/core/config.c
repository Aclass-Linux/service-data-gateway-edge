#include "config.h"
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct egw_conf {
    cJSON *root;
};

/* ── key path 解析 ────────────────────────────────── */

static cJSON *key_path_resolve(cJSON *root, const char *key_path) {
    if (!root || !key_path || *key_path == '\0')
        return NULL;

    cJSON *current = root;
    const char *p = key_path;
    char token[256];
    int ti;

    while (*p) {
        while (*p == '.') p++;
        if (*p == '\0') break;

        /* 判断是否数组下标 */
        if (*p == '[') {
            p++; /* skip '[' */
            int idx = 0;
            while (*p >= '0' && *p <= '9') {
                idx = idx * 10 + (*p - '0');
                p++;
            }
            if (*p == ']') p++; /* skip ']' */
            if (!cJSON_IsArray(current)) return NULL;
            current = cJSON_GetArrayItem(current, idx);
            if (!current) return NULL;
            /* 跳过后面的 '.' */
            while (*p == '.') p++;
            continue;
        }

        /* 读取 key 名直到遇到 . 或 [ 或 \0 */
        ti = 0;
        while (*p && *p != '.' && *p != '[' && ti < (int)sizeof(token) - 1)
            token[ti++] = *p++;
        token[ti] = '\0';

        current = cJSON_GetObjectItem(current, token);
        if (!current) return NULL;
    }

    return current;
}

/* ── 生命周期 ────────────────────────────────────── */

egw_conf_t *egw_conf_load(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) { fclose(fp); return NULL; }

    fread(buf, 1, (size_t)len, fp);
    buf[len] = '\0';
    fclose(fp);

    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) return NULL;
    if (!cJSON_IsObject(root)) { cJSON_Delete(root); return NULL; }

    egw_conf_t *cfg = (egw_conf_t *)malloc(sizeof(egw_conf_t));
    if (!cfg) { cJSON_Delete(root); return NULL; }

    cfg->root = root;
    return cfg;
}

void egw_conf_free(egw_conf_t *cfg) {
    if (!cfg) return;
    if (cfg->root) cJSON_Delete(cfg->root);
    free(cfg);
}

/* ── 取值函数 ────────────────────────────────────── */

const char *egw_conf_get_string(egw_conf_t *cfg, const char *key_path, const char *def) {
    if (!cfg) return def;
    cJSON *item = key_path_resolve(cfg->root, key_path);
    if (!item) return def;
    if (!cJSON_IsString(item)) return def;
    return cJSON_GetStringValue(item);
}

int egw_conf_get_int(egw_conf_t *cfg, const char *key_path, int def) {
    if (!cfg) return def;
    cJSON *item = key_path_resolve(cfg->root, key_path);
    if (!item) return def;
    if (!cJSON_IsNumber(item)) return def;
    return (int)cJSON_GetNumberValue(item);
}

bool egw_conf_get_bool(egw_conf_t *cfg, const char *key_path, bool def) {
    if (!cfg) return def;
    cJSON *item = key_path_resolve(cfg->root, key_path);
    if (!item) return def;
    if (!cJSON_IsBool(item)) return def;
    return cJSON_IsTrue(item);
}

int egw_conf_array_length(egw_conf_t *cfg, const char *key_path) {
    if (!cfg) return 0;
    cJSON *item = key_path_resolve(cfg->root, key_path);
    if (!item) return 0;
    if (!cJSON_IsArray(item)) return 0;
    return cJSON_GetArraySize(item);
}
