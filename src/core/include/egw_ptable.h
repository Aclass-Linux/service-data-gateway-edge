/**
 * @file egw_ptable.h
 * @brief 点表二进制格式定义
 *
 * 三个点表文件（南向、北向、路由）共享相同文件头，条目结构不同。
 * 固定小端字节序、固定宽度类型、__attribute__((packed))。
 *
 * 参考：ADR-0003（mmap 二进制）、ADR-0005（路由表对齐）、ADR-0009（值表示）
 */

#ifndef EGW_PTABLE_H
#define EGW_PTABLE_H

#include "egw_defs.h"

/* ── 文件魔数 ───────────────────────────────────────── */

#define EGW_BIN_MAGIC_SOUTH  0x534F5554u  /* "SOUT" */
#define EGW_BIN_MAGIC_NORTH  0x4E4F5254u  /* "NORT" */
#define EGW_BIN_MAGIC_ROUTE  0x524F5554u  /* "ROUT" */

/* ── 字节序标记 ────────────────────────────────────── */

#define EGW_BIN_ENDIAN_LITTLE  0x01u
#define EGW_BIN_ENDIAN_BIG     0x02u

/* ── 通用文件头（所有 .bin 文件共享）────────────────── */

typedef struct {
    uint32_t magic;          /* EGW_BIN_MAGIC_* */
    uint32_t version;        /* 格式版本号 */
    uint32_t build_id;       /* 离线构建唯一 ID */
    uint32_t checksum;       /* CRC32 */
    uint8_t  endianness;     /* EGW_BIN_ENDIAN_LITTLE / _BIG */
    uint8_t  reserved0;
    uint16_t reserved1;
    uint32_t entry_count;    /* 条目数量 */
    uint32_t strtab_size;    /* 字符串表大小（0 表示无） */
} __attribute__((packed)) egw_bin_header_t;

/* ── 南向点表条目（每设备独立文件）─────────────────── */

/* flags 位定义 */
#define EGW_SB_FLAG_ENABLED           (1u << 0)
#define EGW_SB_FLAG_HAS_SCALE_OFFSET  (1u << 1)
#define EGW_SB_FLAG_HAS_DEADBAND      (1u << 2)

typedef struct {
    uint32_t sig_id;             /* 设备内唯一测点标识 */
    uint8_t  func_code;          /* Modbus 功能码 */
    uint16_t reg_addr;           /* 寄存器起始地址 */
    uint16_t reg_count;          /* 寄存器数量 */
    uint8_t  ctype;              /* 南向交互类型（egw_ctype_t） */
    uint32_t poll_interval_ms;   /* 采集周期（毫秒），0 = 按需 */
    uint8_t  flags;              /* EGW_SB_FLAG_* 组合 */
    float    scale;              /* HAS_SCALE_OFFSET 时有效 */
    float    offset;
    float    deadband;           /* HAS_DEADBAND 时有效 */
} __attribute__((packed)) egw_sb_point_t;

/* ── 北向点表条目（全局单文件）──────────────────────── */

/* flags 位定义（位定义复用南向位名，值相同） */
#define EGW_NB_FLAG_ENABLED           (1u << 0)
#define EGW_NB_FLAG_HAS_SCALE_OFFSET  (1u << 1)
#define EGW_NB_FLAG_HAS_DEADBAND      (1u << 2)

/* 通道掩码位定义 */
#define EGW_CHANNEL_MQTT   (1u << 0)
#define EGW_CHANNEL_SQLITE (1u << 1)
#define EGW_CHANNEL_LUA    (1u << 2)

typedef struct {
    uint32_t sig_id;             /* 北向测点标识 */
    uint8_t  ctype;              /* 北向目标类型（egw_ctype_t） */
    uint32_t channel_mask;       /* EGW_CHANNEL_* 组合 */
    uint16_t reg_addr;           /* 北向 Modbus 寄存器地址 */
    uint8_t  flags;              /* EGW_NB_FLAG_* 组合 */
    float    scale;
    float    offset;
    float    deadband;
    uint32_t strtab_offset;      /* 字符串表偏移（MQTT topic 等） */
    uint16_t strtab_len;         /* 字符串长度 */
    uint8_t  reserved[2];
} __attribute__((packed)) egw_nb_point_t;

/* ── 路由表条目（全局单文件）────────────────────────── */

typedef struct {
    uint16_t device_id;          /* 南向设备 ID */
    uint32_t sig_id;             /* 测点标识 */
    uint32_t sb_index;           /* 南向点表条目索引 */
    uint32_t nb_index;           /* 北向点表条目索引 */
    uint8_t  ctype;              /* 总线核心类型（egw_ctype_t） */
    uint32_t sb_addr_raw;        /* (func_code << 16 | reg_addr) 南向反向查找 */
    uint32_t nb_addr;            /* (func_code << 24 | reg_addr) 北向反向查找 */
    uint8_t  reserved[3];
} __attribute__((packed)) egw_route_entry_t;

/* ── 编译期大小验证 ─────────────────────────────────── */

_Static_assert(sizeof(egw_bin_header_t)  == 28, "egw_bin_header_t: expected 28");
_Static_assert(sizeof(egw_sb_point_t)    == 27, "egw_sb_point_t: expected 27");
_Static_assert(sizeof(egw_nb_point_t)    == 32, "egw_nb_point_t: expected 32");
_Static_assert(sizeof(egw_route_entry_t) == 26, "egw_route_entry_t: expected 26");

#endif /* EGW_PTABLE_H */
