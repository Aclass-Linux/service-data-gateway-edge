/**
 * @file egw_persist.h
 * @brief 运行时值持久化（脏页位图 + seqlock 读）
 *
 * 主回路更新值时置脏位（零开销），独立持久化线程扫描脏页落盘。
 * 参考 DS-008
 */

#ifndef EGW_PERSIST_H
#define EGW_PERSIST_H

#include "egw_defs.h"

#define EGW_PERSIST_PAGE_SIZE   4096u
#define EGW_PERSIST_SLOT_SIZE   16u

typedef struct egw_persist egw_persist_t;

egw_persist_t *egw_persist_create(const char *file_path, uint32_t slot_count);
void           egw_persist_destroy(egw_persist_t *p);

void egw_persist_set(egw_persist_t *p, uint32_t slot, egw_value_t value);
void egw_persist_flush(egw_persist_t *p);

egw_value_t egw_persist_get(egw_persist_t *p, uint32_t slot);

#endif
