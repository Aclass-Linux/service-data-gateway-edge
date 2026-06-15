#include "config.h"
#include <cJSON.h>
#include <cJSON_Utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>



struct egw_conf {
    cJSON *root;
    cJSON *cur;
};

/* ── 生命周期 ────────────────────────────────────── */

egw_err_t egw_conf_load(const char *path, egw_conf_t **out) {
    if (!path || !out) {
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return EGW_RET_CODE(ERR_NOTFOUND);
    }

    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size == 0) {
        close(fd);
        return EGW_RET_CODE(ERR_PARSE);
    }

    char *buf = malloc((size_t)st.st_size + 1);
    if (!buf) {
        close(fd);
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }

    ssize_t nread = read(fd, buf, (size_t)st.st_size);
    close(fd);

    if (nread < 0) {
        free(buf);
        return EGW_RET_CODE(ERR_PARSE);
    }

    buf[nread] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        return EGW_RET_CODE(ERR_PARSE);
    }
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return EGW_RET_CODE(ERR_PARSE);
    }

    egw_conf_t *cfg = malloc(sizeof(egw_conf_t));
    if (!cfg) {
        cJSON_Delete(root);
        return EGW_RET_CODE(ERR_INVALID_ARG);
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
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }

    cJSON *item = cJSONUtils_GetPointer(cfg->root, key_path);
    if (!item) {
        return EGW_RET_CODE(ERR_NOTFOUND);
    }

    cfg->cur = item;
    return EGW_OK;
}

/* ── 取值函数 ────────────────────────────────────── */

egw_err_t egw_conf_get_string(egw_conf_t *cfg, const char *key_path,
                               char **out, const char *def) {
    if (!cfg || !out) {
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }

    cJSON *item = cJSONUtils_GetPointer(cfg->cur, key_path);
    if (!item || !cJSON_IsString(item)) {
        *out = def ? strdup(def) : NULL;
        return EGW_RET_CODE(ERR_NOTFOUND);
    }

    *out = strdup(cJSON_GetStringValue(item));
    if (!*out) {
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }
    return EGW_OK;
}

egw_err_t egw_conf_get_int(egw_conf_t *cfg, const char *key_path,
                            int32_t *out, int32_t def) {
    if (!cfg || !out) {
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }

    cJSON *item = cJSONUtils_GetPointer(cfg->cur, key_path);
    if (!item || !cJSON_IsNumber(item)) {
        *out = def;
        return EGW_RET_CODE(ERR_NOTFOUND);
    }

    *out = (int32_t)cJSON_GetNumberValue(item);
    return EGW_OK;
}

egw_err_t egw_conf_get_bool(egw_conf_t *cfg, const char *key_path,
                             bool *out, bool def) {
    if (!cfg || !out) {
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }

    cJSON *item = cJSONUtils_GetPointer(cfg->cur, key_path);
    if (!item || !cJSON_IsBool(item)) {
        *out = def;
        return EGW_RET_CODE(ERR_NOTFOUND);
    }

    *out = cJSON_IsTrue(item);
    return EGW_OK;
}

egw_err_t egw_conf_array_length(egw_conf_t *cfg, const char *key_path,
                                 int32_t *out, int32_t def) {
    if (!cfg || !out) {
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }

    cJSON *item = cJSONUtils_GetPointer(cfg->cur, key_path);
    if (!item || !cJSON_IsArray(item)) {
        *out = def;
        return EGW_RET_CODE(ERR_NOTFOUND);
    }

    *out = (int32_t)cJSON_GetArraySize(item);
    return EGW_OK;
}