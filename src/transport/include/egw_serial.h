/**
 * @file egw_serial.h
 * @brief 串口 I/O 操作
 *
 * 纯读写接口，不涉及 event loop、poll、回调、状态机。
 * 调用者通过 egw_fd 或 epoll 等方式获得 fd 就绪通知后调用 read/flush。
 */

#ifndef EGW_SERIAL_H
#define EGW_SERIAL_H

#include "egw_defs.h"
#include "egw_serial_params.h"
#include <stddef.h>
#include <stdint.h>

typedef struct egw_serial egw_serial_t;

/**
 * @brief 打开串口
 *
 * open + termios 配置同步完成。失败时 *tp 为 NULL。
 *
 * @param[in]  params 串口参数（path、baud 等）
 * @param[out] tp     成功时写入串口句柄
 * @return EGW_OK       成功
 * @return EGW_ERR_OPEN   open 或 termios 配置失败
 * @return EGW_ERR_INVAL  参数为 NULL
 * @return EGW_ERR_NOMEM  内存分配失败
 */
egw_err_t egw_serial_open(const egw_serial_params_t *params,
                           egw_serial_t **tp);

/**
 * @brief 关闭串口
 *
 * 关闭 fd、释放内部资源。tp 指向的句柄不可再使用。
 *
 * @param[in] tp 串口句柄，可为 NULL
 */
void egw_serial_close(egw_serial_t *tp);

/**
 * @brief 获取 fd
 *
 * 用于注册到 event loop 或 epoll。
 *
 * @param[in] tp 串口句柄
 * @return 文件描述符，错误返回 -1
 */
int egw_serial_get_fd(const egw_serial_t *tp);

/**
 * @brief 非阻塞读
 *
 * 内部调用 read(fd, buf, cap)。*len 返回实际读取的字节数。
 * 如果 fd 无数据（EAGAIN），*len = 0，返回 EGW_OK。
 *
 * @param[in]  tp  串口句柄
 * @param[out] buf 读缓冲区
 * @param[out] len 写入实际读取字节数
 * @param[in]  cap 缓冲区容量
 * @return EGW_OK         成功（*len 可能为 0）
 * @return EGW_ERR_READ    读取失败或 EOF，调用者应 close
 * @return EGW_ERR_INVAL   参数为 NULL
 */
egw_err_t egw_serial_read(egw_serial_t *tp, void *buf,
                           size_t *len, size_t cap);

/**
 * @brief 非阻塞写（入队）
 *
 * 将数据追加到内部写缓冲区，不阻塞。flush 时才实际 write。
 *
 * @param[in] tp  串口句柄
 * @param[in] buf 要写入的数据
 * @param[in] len 数据长度
 * @return EGW_OK       成功
 * @return EGW_ERR_CLOSE  端口已关闭
 * @return EGW_ERR_BUSY  缓冲区满，调用者应先 flush
 * @return EGW_ERR_INVAL 参数为 NULL
 */
egw_err_t egw_serial_write(egw_serial_t *tp, const void *buf, size_t len);

/**
 * @brief 刷写写缓冲区（非阻塞）
 *
 * 将写缓冲区中的数据通过 write(fd) 发出。
 * 一次 flush 不一定全部写出（EAGAIN 时部分写出）。
 * 调用者应重复调用直到 egw_serial_has_pending() 返回 false。
 *
 * @param[in] tp 串口句柄
 * @return EGW_OK         成功（可能部分写出）
 * @return EGW_ERR_WRITE  写入错误
 * @return EGW_ERR_CLOSE  端口已关闭
 * @return EGW_ERR_INVAL  参数为 NULL
 */
egw_err_t egw_serial_flush(egw_serial_t *tp);

/**
 * @brief 检查写缓冲区是否有数据
 *
 * @param[in] tp 串口句柄
 * @return true  有数据待 flush
 * @return false 缓冲区空
 */
bool egw_serial_has_pending(const egw_serial_t *tp);

#endif /* EGW_SERIAL_H */
