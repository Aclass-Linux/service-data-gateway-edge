/**
 * @file egw_persist_thread.h
 * @brief 持久化线程上下文 —— 管道 + poll 循环
 *
 * 线程生命周期由调用方管理：
 *   pctx = egw_persist_thread_create(...)
 *   pthread_create(&tid, NULL, egw_persist_thread_fn, pctx)
 *   ...
 *   egw_persist_thread_request_stop(pctx)
 *   pthread_join(tid, NULL)
 *   egw_persist_thread_destroy(pctx)
 */

#ifndef EGW_PERSIST_THREAD_H
#define EGW_PERSIST_THREAD_H

#include <stdint.h>

typedef struct egw_persist_thread egw_persist_thread_t;

egw_persist_thread_t *egw_persist_thread_create(const char *db_path,
                                                  int interval_ms);
void *egw_persist_thread_fn(void *arg);
void  egw_persist_thread_enqueue(egw_persist_thread_t *pt,
                                  const char *table,
                                  uint16_t device_id,
                                  uint16_t reg_addr,
                                  uint16_t value);
void  egw_persist_thread_request_stop(egw_persist_thread_t *pt);
void  egw_persist_thread_destroy(egw_persist_thread_t *pt);

#endif /* EGW_PERSIST_THREAD_H */
