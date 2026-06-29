/**
 * @file egw_persist.h
 * @brief 运行时数据持久化 —— 事务式 SQLite 写入
 */

#ifndef EGW_PERSIST_H
#define EGW_PERSIST_H

#include <stdint.h>

typedef struct egw_persist egw_persist_t;

egw_persist_t *egw_persist_open(const char *db_path);
void           egw_persist_begin(egw_persist_t *p);
void           egw_persist_put(egw_persist_t *p, const char *table,
                                uint16_t device_id, uint16_t reg_addr,
                                uint16_t value);
void           egw_persist_commit(egw_persist_t *p);
void           egw_persist_full_dump(egw_persist_t *p, const char *table);
void           egw_persist_close(egw_persist_t *p);

#endif /* EGW_PERSIST_H */
