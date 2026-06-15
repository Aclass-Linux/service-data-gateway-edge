/**
 * @file egw_ptable_loader.h
 * @brief 点表 mmap 加载器
 *
 * 打开 .bin 文件，mmap 映射，验证文件头（magic/version/endian/checksum）。
 * 调用方通过 entries() 获取条目数组直接访问。
 */

#ifndef EGW_PTABLE_LOADER_H
#define EGW_PTABLE_LOADER_H

#include "egw_defs.h"
#include "egw_ptable.h"

typedef struct egw_ptable egw_ptable_t;

egw_err_t egw_ptable_open(const char *path, uint32_t expected_magic,
                           egw_ptable_t **out);

void egw_ptable_close(egw_ptable_t *pt);

const void *egw_ptable_entries(egw_ptable_t *pt);
uint32_t    egw_ptable_entry_count(egw_ptable_t *pt);
uint32_t    egw_ptable_build_id(egw_ptable_t *pt);

#endif
