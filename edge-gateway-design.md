# 工业边缘网关数据中心 — 架构设计方案

> 基于 FFmpeg 管线架构思想，结合工业现场总线场景定制

---

## 目录

1. [核心设计哲学](#1-核心设计哲学)
2. [各阶段扩缩与分区策略](#2-各阶段扩缩与分区策略)
3. [架构概览](#3-架构概览)
4. [数据模型](#4-数据模型)
5. [四阶段流水线设计](#5-四阶段流水线设计)
6. [队列拓扑](#6-队列拓扑)
7. [调度与流控](#7-调度与流控)
8. [线程模型汇总](#8-线程模型汇总)
9. [与 FFmpeg 差异总结](#9-与-ffmpeg-差异总结)
10. [一期实现](#10-一期实现)
11. [二期实现](#11-二期实现)
12. [故障场景行为](#12-故障场景行为)

---

## 1. 核心设计哲学

### 1.1 FFmpeg 管线模型 vs 工业网关管线模型

```
FFmpeg:               demuxer → decoder → filter → encoder → muxer
                        │          │         │         │        │
                    读文件帧    解码帧     滤镜处理    编码帧    写文件
                     I/O密集    CPU密集    CPU密集    CPU密集    I/O密集

工业网关 (你的方案):  采集 → 解析 → 处理 → 转发
                        │       │        │       │
                     epoll读  协议解码  规则引擎  写缓存/发MQTT
                    I/O密集    CPU轻量   CPU轻量  I/O密集
```

关键差异：媒体解码是纯 CPU 密集（适合 FFmpeg 的每阶段独立线程），工业 I/O 是等待密集（适合 epoll + 流水线队列）。

### 1.2 每个阶段独立扩缩，不预设线程数

各阶段按需扩缩，互不耦合：

```
                    采集层             解析层             处理层             转发层
                    (按设备分区)      (无状态池化)       (按 point_id 分区)   (无状态池化)
                    
 设备 1-100 ───▶  采集线程   ─▶ RAW    ┌─ 解析线程1 ─▶ DP    ┌─ 处理线程1  ─▶ ACT    ┌─ 转发线程1  ─▶ StateCache
 设备 101-200 ─▶  采集线程   ─▶ QUEUE ─┼─ 解析线程2 ─▶ QUEUE─┼─ 处理线程2  ─▶ QUEUE─┼─ 转发线程2  ─▶ MQTT
 设备 201-300 ─▶  采集线程   ─▶       └─ 解析线程3 ─▶       └─ 处理线程3  ─▶       └─ 转发线程3  ─▶ Logger
                                                                               
  1~N 线程            N 线程 (epoll)        M 线程               K 线程              L 线程 (M:N 路由可选)
```

工业采集 99% 时间在等串口/网络应答，所以采集线程可以用 epoll 用少量线程管大量连接。
但如果系统有 300 个设备分布在 3 个独立网口，开 3 个采集线程（每个网口一个 epoll）就是合理的选择——**按物理拓扑分区**。

### 1.3 和 FFmpeg 管的线哲学本质一致

FFmpeg 的精髓不是"多线程"，而是**独立阶段 + 队列解耦 + 背压传递**：

```
FFmpeg:    demux ─[pkt queue]─→ dec ─[frame queue]─→ filter ─[frame queue]─→ enc ─[pkt queue]─→ mux
本方案:    采集 ─[raw queue]─→ 解析 ─[dp queue]─→ 处理 ─[action queue]─→ 转发
```

每个阶段有自己的节奏，队列满了上游自然阻塞——这个核心设计哲学不变，变的只是采集阶段从 N 线程轮询改成 epoll 事件驱动。

---

## 2. 各阶段扩缩与分区策略

> 核心原则：每阶段通过**数据分区**实现多线程扩缩，分区方式因阶段而异。

### 2.1 四阶段的分区策略

```
采集层           解析层             处理层              转发层
─────────        ─────────          ─────────           ─────────
分区方式          分区方式            分区方式             分区方式
按设备/连接      无状态竞争          按 point_id hash    按 Action type
┌──────┐         ┌──────┐           ┌──────┐            ┌──────┐
│ 采集1 │──RAW──▶│ 解析  │─DP──────▶│ 处理  │─ACT──────▶│ 转发1 │→ Cache
│(设备1-50)      │(空闲抢)           │(hash=0-99)       │(type=CACHE)
├──────┤         ├──────┤           ├──────┤            ├──────┤
│ 采集2 │──RAW──▶│ 解析  │─DP──────▶│ 处理  │─ACT──────▶│ 转发2 │→ MQTT
│(设备51-100)    │(空闲抢)           │(hash=100-199)    │(type=MQTT)
├──────┤         ├──────┤           ├──────┤            ├──────┤
│ 采集3 │──RAW──▶│ 解析  │─DP──────▶│ 处理  │─ACT──────▶│ 转发3 │→ Logger
│(设备101-150)   │(空闲抢)           │(hash=200-299)    │(type=ALARM)
└──────┘         └──────┘           └──────┘            └──────┘
```

### 2.2 各阶段分区详解

#### 采集层：按设备/连接分区

```
策略 A [单 epoll 线程]                    策略 B [多 epoll 线程]
┌────────────┐                            ┌─────────────────────────┐
│ epoll 线程   │ 监听全部 fd                │ 路由层 (hash/轮询)       │
│            │                            │  ┌──────┐  ┌──────┐    │
│ fd1 fd2... │                            │  │ 采集1 │  │ 采集2 │    │
│            │                            │  │epoll │  │epoll │    │
│ 适用于 ~50 连接                          │  │fd1-50│  │fd51- │    │
└────────────┘                            │  │      │  │ 100  │    │
                                          │  │适用于 100+ 连接      │
                                          └─────────────────────────┘
```

策略 A 适用于几十个连接，策略 B 适用于大规模部署（类似 nginx worker 模型）。具体：

| 分区方式 | 怎么做 | 适用场景 |
|---------|--------|---------|
| **按网口** | `/sys/class/net/eth0` 上的设备分配给线程 A，`eth1` 给线程 B | 多网口物理隔离 |
| **按设备 ID hash** | `device_id % N` 分配到第 N 个采集线程 | 设备均匀分布 |
| **按协议类型** | Modbus 给线程 A，CANopen 给线程 B | 不同协议采集频率差异大 |

每个采集线程拥有独立的 epoll fd，彼此独立，一个线程挂了一个设备的 read 不影响其他设备（其他线程的 epoll 照常跑）。

#### 解析层：无状态竞争消费

解析线程池是所有阶段里最简单的扩缩方式——**无状态竞争**。N 个线程从同一个 RAW_QUEUE 里 `tq_receive()`，谁空闲谁拿。

```c
// 所有 parser 线程从同一个队列竞争取帧
// 天然负载均衡，不挑食
// 加线程 = 加消费者，减线程 = 减消费者，不需要 rebalance
```

唯一注意事项：同一个 fd 的数据帧可能被不同 parser 线程处理。对于 Modbus 这种**有状态的协议**（如正在进行的 transaction）：

- 一期简化：Modbus TCP 每个 request-response 是原子帧，不需要保持连接状态
- 二期：如果需要支持事务状态，可以在解析层之上再加一层**按 fd_id hash 分配**——确保同一个 fd 的帧始终落到同一个 parser 线程

#### 处理层：按 point_id 分区

Processor 是有状态的（聚合窗口、变化率检测需要每个 point_id 维护上下文），所以不能用无状态竞争，必须分区：

```c
// 处理线程 N 只处理 point_id % total_threads == N 的数据点
// 每个 point_id 的数据始终落在同一个 processor 线程
// 聚合窗口、变化率检测不需要跨线程同步

int processor_idx = dp.point_id % nb_processors;
tq_send(processor_queues[processor_idx], 0, &dp);
```

扩容处理线程时需要 rehash（重启或在运行时做一致性哈希），这是分区无法避免的代价。

#### 转发层：按 Action type 分区

每个 Action type 一个独立队列 + 一个或多个转发线程：

```
转发线程数 = 按 type 独立配置

cache_forwarder_threads = 1     // 写 StateCache 是纯内存操作，1 个够
mqtt_forwarder_threads  = 2     // MQTT publish 可能慢，可以并行
logger_forwarder_threads = 1    // 写盘有锁，多线程无益
```

同 type 的多个线程可以通过 point_id hash 分区（保证同一个 point 的消息不乱序），也可以无状态竞争（MQTT 每条消息独立，顺序不重要）。

### 2.3 扩缩配置示例

```json
{
  "collectors": {
    "strategy": "by_network_interface",
    "instances": [
      { "interface": "eth0", "threads": 1 },
      { "interface": "eth1", "threads": 1 },
      { "device_pool": "plc_pool", "threads": 2, "partition": "hash" }
    ]
  },
  "parsers": {
    "threads": 4,
    "partition": "competitive"
  },
  "processors": {
    "threads": 2,
    "partition": "point_id_hash"
  },
  "forwarders": {
    "cache":  { "threads": 1, "partition": "single" },
    "mqtt":   { "threads": 2, "partition": "competitive" },
    "logger": { "threads": 1, "partition": "single" },
    "alarm":  { "threads": 1, "partition": "single" }
  }
}
```

### 2.4 跨阶段队列的连接模式

| 连接模式 | 适用阶段 | 是否有序 |
|---------|---------|---------|
| **单队列 × 单消费者** (1:1) | 采集→解析（简单场景） | 严格有序 |
| **单队列 × 多消费者竞争** (1:N) | 采集→解析（无状态解析） | 无序，但每个 fd 的帧可能乱序 |
| **多队列 × 多消费者分区** (M:N) | 处理→转发（按 point_id hash） | 单点有序 |
| **多队列 × 单消费者** (M:1) | 采集→解析分区再汇聚 | 全局有序 |

## 3. 架构概览

```
┌───────────────────────────────────────────────────────────────────┐
│                     管理面 (主线程)                                │
│  sch_start() → while !sch_wait(100ms) { stats; watchdog; }       │
│                    → sch_stop()                                   │
└──────────────────────┬────────────────────────────────────────────┘
                       │ schedule_update (choke/unchoke)
                       ▼
┌───────────────────────────────────────────────────────────────────┐
│ 数据面流水线 (四阶段流水线，每阶段独立扩缩)                          │
│                                                                    │
│  ┌──────────────┐ raw frame ┌────────────┐ DataPoint ┌──────────┐  │
│  │  采集线程池    │──────────▶│  解析线程池  │──────────▶│ 处理线程池 │  │
│  │  1~N × epoll  │[RAW_QUEUE]│ M (竞争)   │[DP_QUEUE] │ K (hash) │  │
│  └──────┬───────┘           └────────────┘           └─────┬────┘  │
│        │  epoll 监听全部南向 fd                              │       │
│        │  read() → push to RAW_QUEUE                       │       │
│  ┌─────┴─────────────────┐                  ┌─────────────┴──┐    │
│  │ Modbus TCP (N 连接)    │                  │  规则引擎       │    │
│  │ Modbus RTU (M 串口)    │                  │  聚合/报警/变换 │    │
│  │ CANopen (K 总线)       │                  │  Lua 脚本       │    │
│  │ PLC S7 (P 连接)        │                  └────────────────┘    │
│  └───────────────────────┘                                        │
│                                    ActionList                      │
│  ┌────────┐  ┌──────────┐  ┌──────────────────┐  ┌─────────┐     │
│  │ 转发线程 │  │ 转发线程  │  │    转发线程       │  │ 转发线程  │     │
│  │(StateCa │  │(MQTT)   │  │  (Modbus Server) │  │(Logger) │     │
│  │ che)    │  │         │  │                  │  │         │     │
│  └────┬────┘  └────┬────┘  └────────┬─────────┘  └────┬────┘     │
│       │            │                │                  │          │
│       ▼            ▼                ▼                  ▼          │
│  StateCache    MQTT Broker    Modbus Master     InfluxDB/SQLite   │
│  (atomic)       (远程)         (远程)                              │
│       │                                                           │
│       ├── HTTP API (REST + WebSocket)                             │
│       └── 无锁读取                                                  │
└───────────────────────────────────────────────────────────────────┘
```

---

## 4. 数据模型

### 4.1 原始帧 (RawFrame)

采集线程从南向接口读到的原始字节，放入 RAW_QUEUE：

```c
typedef struct RawFrame {
    int             fd_id;          // 来自哪个 socket/串口
    int64_t         timestamp_us;   // 接收时间戳
    uint8_t         data[4096];     // 原始报文
    size_t          size;
    AVBufferRef    *buf;            // 引用计数，零拷贝传给多个解析器(如有需要)
} RawFrame;
```

### 4.2 数据点 (DataPoint)

解析线程产出的结构化数据，放入 DP_QUEUE：

```c
typedef struct DataPoint {
    int64_t         timestamp_us;   // 采集时间戳
    uint32_t        point_id;       // 指向 PointMeta
    DataValue       value;          // union { double; int64_t; bool; raw }
    Quality         quality;        // GOOD / UNCERTAIN / BAD
    int             flags;          // ALARM / CHANGE / PERIODIC
} DataPoint;

typedef union DataValue {
    double   f64;
    int64_t  i64;
    bool     b;
    struct { uint8_t *data; size_t len; } raw;
} DataValue;
```

### 4.3 动作列表 (ActionList)

处理线程的输出，分发给各转发线程：

```c
typedef enum {
    ACT_CACHE_UPDATE,   // 更新 StateCache
    ACT_MQTT_PUBLISH,   // 推送到 MQTT
    ACT_ALARM,          // 报警记录
    ACT_LOG_STORE,      // 时序落盘
    ACT_WS_PUSH,        // WebSocket 推送
} ActionType;

typedef struct Action {
    ActionType  type;
    uint32_t    point_id;
    DataPoint   dp;
} Action;
```

---

## 5. 四阶段流水线设计

### 5.1 第一阶段：采集线程池 (epoll 事件驱动)

将南向接口按分区策略（见 2.2 节）分配到 N 个采集线程，每个线程拥有独立的 epoll fd。

**为什么不用 per-connection 线程？** 工业 I/O 99% 时间在等待应答，per-connection 线程意味着 N 倍栈空间和调度开销。epoll 单线程可管理数百 fd，多线程则是在设备规模更大时按物理网口或设备组**分区**，而不是按连接数。

```c
// 每个采集线程独立运行此循环
// N 个线程 = N 个 epoll 实例，各管一组设备
static int collector_thread(void *arg) {
    Collector *c = arg;
    int epoll_fd = epoll_create1(0);

    // 注册本线程管辖的南向 fd 到 epoll
    for (int i = 0; i < c->nb_devices; i++) {
        int fd = c->devices[i].fd;        // socket / serial fd
        struct epoll_event ev = {
            .events  = EPOLLIN | EPOLLRDHUP,
            .data.ptr = &c->devices[i],
        };
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    }

    struct epoll_event events[64];

    while (1) {
        // 检查是否被 scheduler choke
        if (waiter_wait(&c->waiter))
            break;

        // epoll 等待事件，超时 50ms（便于 choke 响应）
        int nfds = epoll_wait(epoll_fd, events, 64, 50);
        if (nfds < 0) { /* handle EINTR */ continue; }

        for (int i = 0; i < nfds; i++) {
            Device *dev = (Device *)events[i].data.ptr;

            if (events[i].events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
                handle_disconnect(c, dev);
                continue;
            }

            // 只做 read，不做协议解析——保持 epoll 线程轻量
            RawFrame rf = {
                .fd_id       = dev->id,
                .timestamp_us = now(),
            };
            int n = read(dev->fd, rf.data, sizeof(rf.data));
            if (n <= 0) { handle_error(c, dev); continue; }

            rf.size = n;
            rf.buf  = av_buffer_alloc(n);
            memcpy(rf.buf->data, rf.data, n);

            // 推入 RAW_QUEUE（满则阻塞 → 背压传递）
            int ret = tq_send(c->raw_queue, dev->stream_idx, &rf);
            if (ret == AVERROR_EOF) break;
        }
    }

    return 0;
}
```

**为什么不在这里做协议解析？** 和 FFmpeg demuxer 只读容器不解释帧内容同理——保持采集线程非阻塞，解析交给下游池。

**串口怎么办？** 串口 fd 同样可以注册到 epoll（`EPOLLIN | EPOLLET` 边缘触发），和 socket 一样处理。Linux 下一切皆 fd。

### 5.2 第二阶段：解析线程池

从 RAW_QUEUE 取出原始帧，按协议解析成结构化 DataPoint：

```c
static int parser_thread(void *arg) {
    ParserPool *pool = arg;

    while (1) {
        RawFrame rf;
        int stream_idx;

        // 从 RAW_QUEUE 取帧
        int ret = tq_receive(pool->raw_queue, &stream_idx, &rf);
        if (ret == AVERROR_EOF) break;

        // 根据 fd_id 找到对应的协议解析器
        Parser *parser = pool->parsers[rf.fd_id];

        DataPoint dp;
        ret = parser->ops->parse(parser->ctx, &rf, &dp);
        av_buffer_unref(&rf.buf);  // 释放原始缓冲区

        if (ret < 0) {
            log_parse_error(pool, rf.fd_id, ret);
            continue;  // 解析失败就丢弃
        }

        // 解析成功 → 发往 DP_QUEUE
        ret = tq_send(pool->dp_queue, dp.point_id, &dp);
        if (ret == AVERROR_EOF) break;
    }

    return 0;
}
```

**线程池数量**：一期 1 个，二期根据协议复杂度可调。池内每个线程独立从 RAW_QUEUE 竞争取帧（和 slice threading 相反——不是工作窃取，而是队列自然竞争）。

### 5.3 第三阶段：处理线程池（规则引擎）

处理层按 `point_id % nb_processors` 分区（参见 2.2 节），确保同一个数据点始终落在同一个线程，这对有状态的聚合/变化率检测是必要的。

上游（解析层）在发送时完成分区：

```c
// 解析线程根据 point_id 将 DataPoint 发到对应的 processor 队列
int proc_idx = dp.point_id % nb_processors;
tq_send(processor_queues[proc_idx], 0, &dp);
```

每个处理线程从自己的队列消费，执行规则后产出 Action 列表：

```c
static int processor_thread(void *arg) {
    Processor *p = arg;

    while (1) {
        DataPoint dp;
        int ret = tq_receive(p->dp_queue, NULL, &dp);
        if (ret == AVERROR_EOF) break;

        Action actions[MAX_ACTIONS];
        int nb_actions = 0;

        // 一期：C 函数链
        for (int i = 0; i < p->nb_transforms; i++) {
            ret = p->transforms[i](&dp, actions, &nb_actions);
            if (ret < 0) break;  // 条件不满足，丢弃
            if (ret == 0) break; // 提前终止
        }

        // 二期：Lua 规则引擎
        // nb_actions = lua_rule_eval(p->lua_state, &dp, actions);

        // 分发 Action 到对应的转发队列
        for (int i = 0; i < nb_actions; i++) {
            ForwardQueue *fq = p->forward_queues[actions[i].type];
            tq_send(fq->queue, 0, &actions[i]);
        }
    }

    return 0;
}
```

### 5.4 第四阶段：转发线程池（多路输出）

每个转发类型一个独立线程，从自己的 Action 队列读取后执行。

```c
// StateCache 转发
static int forwarder_cache_thread(void *arg) {
    Forwarder *fw = arg;

    while (1) {
        Action act;
        int ret = tq_receive(fw->queue, NULL, &act);
        if (ret == AVERROR_EOF) break;

        cache_write(&fw->state_cache[act.point_id], &act.dp);
    }
    return 0;
}

// MQTT 转发
static int forwarder_mqtt_thread(void *arg) {
    Forwarder *fw = arg;

    while (1) {
        Action act;
        int ret = tq_receive(fw->queue, NULL, &act);
        if (ret == AVERROR_EOF) break;

        char topic[128];
        snprintf(topic, sizeof(topic), "edge/points/%u", act.point_id);
        mqtt_publish(fw->mqtt_ctx, topic, &act.dp);
    }
    return 0;
}

// HTTP API 不占用转发线程——直接从 StateCache 读
static int http_handler(HTTPReq *req, HTTPResp *resp) {
    uint32_t id = parse_point_id(req->url);
    DataPoint dp;
    int ret = cache_read(&state_cache[id], &dp);
    if (ret < 0) return http_404(resp);
    return http_json(resp, &dp);
}
```

---

## 6. 队列拓扑

```
                     RAW_QUEUE              DP_QUEUE
                    (tq_send/tq_receive)   (tq_send/tq_receive)
                    ┌─────────────────┐    ┌──────────────────┐
  fd1 ─── epoll ───▶│  Parser线程 1   │───▶│                  │
  fd2 ─── read ────▶│  Parser线程 2   │───▶│  Processor 线程  │
  fd3 ─────────────▶│  Parser线程...  │───▶│  (规则引擎)      │
  fd4 ─────────────▶│  Parser线程 N   │───▶│                  │
                    └─────────────────┘    └────────┬─────────┘
                                                    │
                     ┌──────────────────────────────┤
                     │  Action 分发 (按 type 路由)   │
                     ▼                              ▼
              ACT_CACHE_UPDATE                 ACT_ALARM
              ┌──────────────┐              ┌──────────────┐
              │ Cache 转发线程 │              │ Alarm 转发线程 │
              │ (1)          │              │ (1)          │
              └──────┬───────┘              └──────┬───────┘
                     ▼                             ▼
               StateCache                      Logger / 通知
              (atomic 无锁)
                     │
         ┌───────┬───┴───┬───────┐
         ▼       ▼       ▼       ▼
      HTTP API  WS    Modbus Srv  MQTT Client
      (被动读)  (推)   (被动读)    (主动推)
```

**关键设计点**：HTTP / Modbus Server 不走 Action 队列，它们直接从 StateCache 读。只有"数据变化时主动推送"的动作（MQTT/WS/Logger）才走转发线程。这和 FFmpeg 的 muxer 不同——muxer 是管道的终点，必须串行写文件；而北向服务可以无阻塞读缓存。

---

## 7. 调度与流控

### 7.1 背压传递链

```
Processor 处理慢
    ↓
DP_QUEUE 满
    ↓
Parser 的 tq_send(DP_QUEUE) 阻塞
    ↓
Parser 不消费 RAW_QUEUE
    ↓
RAW_QUEUE 满
    ↓
采集线程的 tq_send(RAW_QUEUE) 阻塞
    ↓
采集线程 epoll 不再处理新事件 —— 内核 socket buffer 积压
    ↓
TCP 对端（PLC/RTU）收不到 ACK → 自动降速
```

**天然端到端背压**——不需要额外代码。队列满了上游自然停。

### 7.2 Scheduler (choke/unchoke)

主线程同样每 100ms 检查队列水位：

```c
void schedule_update(Gateway *gw) {
    for (int i = 0; i < gw->nb_pipelines; i++) {
        int raw_depth   = tq_get_size(gw->pipelines[i].raw_queue);
        int dp_depth    = tq_get_size(gw->pipelines[i].dp_queue);

        // choke 条件：任一段积压超过水位线
        int overload = (raw_depth > RAW_QUEUE_HWM) || (dp_depth > DP_QUEUE_HWM);
        waiter_set(&gw->collector_waiter, overload);
    }
}
```

### 7.3 故障隔离

某个南向设备掉线不会阻塞其他设备——epoll 线程只是 `read()` 返回 0，标记一下该 fd 断开，继续处理其他 fd。只有**全部队列满了**才会全局阻塞，而这是所有下游全挂的信号，阻塞反而是对的。

---

## 8. 线程模型汇总

所有阶段均支持独立扩缩，以下是一期和二期的典型配置，而非固定值。

### 一期（最小可用：~5 线程）

```
                     ┌──────┐  raw   ┌──────┐  dp    ┌──────┐  act   ┌──────────┐
 设备 1-10 ── epoll ─▶│ 采集1 │───────▶│ 解析1 │───────▶│ 处理1 │───────▶│ Cache转发 │
                     └──────┘        └──────┘        └──────┘        └─────┬────┘
                                                                           │
  主线程 (监控/看门狗) ─────────────────────────────────────────────────────┤
                                                                           ▼
                                                                     HTTP API
```

| 线程 | 数量 | 说明 |
|------|------|------|
| 主线程 | 1 | 启动/停止/sch_wait/schedule_update |
| 采集 | 1 | 单 epoll，管所有南向设备 |
| 解析 | 1 | 从 RAW_QUEUE 竞争消费 |
| 处理 | 1 | C 函数链变换 |
| 转发 | 1 | StateCache 写入 |
| HTTP | 事件驱动 | libmicrohttpd 内部工作池，不计入 |
| **合计** | **5** | |

### 二期（全量：~10-20 线程，按需配置）

| 线程 | 数量 | 分区方式 |
|------|------|---------|
| 主线程 | 1 | — |
| 配置管理 | 1 | — |
| 采集线程池 | 1-4 | 按网口/设备组_id hash |
| 解析线程池 | 2-4 | 无状态竞争 |
| 处理线程池 | 2-4 | 按 point_id hash |
| Cache 转发 | 1 | 单消费者（纯内存，1 线程足够） |
| MQTT 转发 | 1-2 | 无状态竞争 |
| Alarm 转发 | 1 | 单消费者 |
| Logger 转发 | 1 | 单消费者 |
| HTTP API | 事件驱动 | libmicrohttpd 内部管理 |
| **合计** | **~10-18** | 按设备数和协议复杂度调节 |
| Alarm 转发/Logger | 1 | 报警 + 时序落盘 |
| **合计** | **~10** | 远少于每接口一线程的 30+ |

---

## 9. 与 FFmpeg 差异总结

| 维度 | FFmpeg | 本方案（修订后） | 理由 |
|------|--------|-----------------|------|
| **采集方式** | 每容器一个 demuxer 线程 | **单 epoll 线程** | 工业 I/O 是等待密集，不是 CPU 密集 |
| **解析** | decoder 线程（CPU 密集） | **解析线程池**（可扩缩） | 不同协议解析量差异大 |
| **管线阶段数** | 4-5 阶段 | **4 阶段**（采集/解析/处理/转发） | 映射清晰：读/解码/逻辑/写 |
| **北向** | muxer 串行写文件 | **StateCache 无锁读多路服务** | 上游需随时查询最新值 |
| **队列间数据** | AVPacket / AVFrame | RawFrame / DataPoint / Action | 每阶段数据格式不同，类型安全 |
| **帧排序** | PTS/DTS 严格有序 | **无序**（数据点独立） | 温度/压力之间无依赖 |
| **线程池** | 无（每线程固定功能） | **解析线程池** | 按负载动态扩缩 |
| **Fork-join 并行** | slice threading | **不需要** | 单数据点处理量小 |
| **背压** | ThreadQueue 满 + choke | **同一机制** | FFmpeg 最值得保留的设计 |
| **ed管理面** | 主线程 sch_wait 100ms | **同一机制** | 直接移植 |

---

## 10. 一期实现

### 10.1 范围

| 模块 | 包含 | 不包含 |
|------|------|--------|
| **采集线程** | epoll 监听多个 Modbus TCP 连接，read() 后推入 RAW_QUEUE | 断线重连、TLS |
| **解析线程** | Modbus RTU 帧解析（1 个线程） | CANopen、PLC、OPC UA |
| **处理线程** | C 函数链：缩放/偏移/钳位 | Lua 规则引擎、聚合窗口 |
| **转发线程** | StateCache 写入 + HTTP API 读取 | MQTT、Modbus Server、WebSocket |
| **StateCache** | atomic 无锁写入/读取 | TTL 过期、历史序列 |
| **主线程** | 启动/停止/sch_wait 循环 + 队列水位流控 | 热加载、看门狗 |
| **配置** | 静态 JSON 文件加载 | 热加载、REST 管理 API |
| **日志** | stdout | 结构化日志、远程 syslog |

### 10.2 目录结构

```
src/
├── main.c                  # main() → gw_init → gw_start → loop → gw_stop
├── gateway.h               # 顶层结构体 Gateway, Scheduler
├── gateway.c
│
├── collector.h             # epoll 采集线程
├── collector.c
│
├── parser.h                # 解析线程 + ops 接口
├── parser_modbus.c         # Modbus 协议解析
├── parser.c                # 解析线程入口
│
├── processor.h             # 处理线程 + transform 链
├── processor.c
│
├── forwarder.h             # 转发线程抽象
├── forwarder_cache.c       # StateCache 写入
│
├── state_cache.h           # 无锁共享缓存
├── state_cache.c
│
├── http_api.h              # HTTP Server (libmicrohttpd)
├── http_api.c
│
├── datapoint.h             # RawFrame, DataPoint, Action 定义
│
├── queue.h                 # ThreadQueue (移植 fftools)
├── queue.c
│
├── scheduler.h             # SchWaiter + schedule_update
├── scheduler.c
│
└── config.json
```

### 10.3 启动流程

```c
int main(int argc, char **argv) {
    Gateway *gw = gw_new(config_path);

    // 顺序启动：下游先就绪
    task_start(&gw->forwarder.task);     // 1. 转发线程先跑
    task_start(&gw->processor.task);     // 2. 处理线程
    task_start(&gw->parser.task);        // 3. 解析线程
    task_start(&gw->collector.task);     // 4. 采集线程最后

    // 主循环
    timer_start = av_gettime_relative();
    while (!sch_wait(gw, 100ms)) {
        print_stats(gw, timer_start);
        if (check_keyboard()) break;
    }

    // 有序关闭
    task_stop(&gw->collector.task);  // 先停采集（不再读入）
    task_stop(&gw->parser.task);     // 等解析排空
    task_stop(&gw->processor.task);  // 等处理排空
    task_stop(&gw->forwarder.task);  // 最后停转发
}
```

### 10.4 一期配置文件

```json
{
  "collector": {
    "epoll_timeout_ms": 50,
    "devices": [
      { "id": 1, "name": "boiler_1",  "protocol": "modbus_tcp",  "url": "192.168.1.100:502", "poll_ms": 100 },
      { "id": 2, "name": "boiler_2",  "protocol": "modbus_tcp",  "url": "192.168.1.101:502", "poll_ms": 200 },
      { "id": 3, "name": "temp_rack", "protocol": "modbus_rtu",  "url": "/dev/ttyUSB0",       "baud": 9600,  "poll_ms": 500 }
    ]
  },
  "parsers": {
    "count": 1
  },
  "transforms": [
    { "type": "scale",  "point_id": 1, "factor": 0.1,  "offset": -40 },
    { "type": "clamp",  "point_id": 1, "min": -40,    "max": 200 },
    { "type": "scale",  "point_id": 2, "factor": 0.01 }
  ],
  "http": {
    "port": 8080,
    "endpoints": ["/api/v1/points/:id"]
  },
  "queues": {
    "raw_queue_size": 1024,
    "dp_queue_size":  2048,
    "act_queue_size": 1024
  }
}
```

### 10.5 一期里程碑

| 阶段 | 内容 | 验证方式 |
|------|------|----------|
| M1 | 线程基座 + ThreadQueue + sch_wait 循环 | 启动 5 线程，互相传递空消息 |
| M2 | epoll 采集线程 + Modbus TCP 模拟设备 | tcpdump 见正确请求，RawFrame 入队 |
| M3 | 解析线程解析 Modbus 帧 → DataPoint | 打印正确温湿度值 |
| M4 | Processor 函数链处理数据 | 缩放/钳位生效 |
| M5 | StateCache + HTTP API | curl 读到最新值 |
| M6 | Scheduler 流控 + 队列水位 choke | 注入慢处理，verify 采集被 choke |

---

## 11. 二期实现

### 11.1 新增功能

| 模块 | 说明 |
|------|------|
| **多协议解析** | CANopen (SDO/PDO)、PLC S7、OPC UA、IEC 104 |
| **Lua 规则引擎** | 嵌入 Lua，热加载规则脚本，支持 emit 动作 |
| **聚合窗口** | 滑动窗口均值/最大值/计数 |
| **报警引擎** | 触发条件 → 动作（日志/推送/HTTP） |
| **Modbus TCP Server** | 将 StateCache 映射为从设备寄存器 |
| **MQTT Client** | 变化推送 + 下行命令订阅 |
| **WebSocket** | 实时推送数据变化 |
| **时序日志** | SQLite / InfluxDB 落盘 |
| **管理 REST API** | CRUD 配置、启停采集、查看状态 |
| **配置热加载** | inotify watch + SIGHUP + REST |
| **TLS** | mbedTLS / OpenSSL |
| **看门狗** | 线程 hang 检测、自动重启 |

### 11.2 全量线程布局

```
                      ┌──────────────┐
                      │  主线程 + 配置  │  (2 threads)
                      └──────┬───────┘
                             │ schedule_update
                             ▼
┌──────────┐  RAW_QUEUE  ┌──────────┐  DP_QUEUE  ┌──────────┐
│ epoll采集 │────────────▶│ 解析池 ×N │───────────▶│ Processor│
│ (1)      │  (有界队列)  │ (2-4)    │ (有界队列)  │ (Lua引擎)│
└──────────┘             └──────────┘             └────┬─────┘
                                                       │ Actions
                                                       │
              ┌────────────────────────────────────────┤
              │           │             │              │
              ▼           ▼             ▼              ▼
       ┌──────────┐ ┌──────────┐ ┌──────────┐  ┌──────────┐
       │ Cache    │ │ MQTT    │ │ Alarm    │  │ Logger   │
       │ Forward  │ │ Forward │ │ Forward  │  │ Forward  │
       └────┬─────┘ └────┬────┘ └────┬─────┘  └────┬─────┘
            │            │           │              │
            ▼            ▼           ▼              ▼
       StateCache    MQTT      告警通知          SQLite
        (atomic)     Broker
            │
       ┌────┼────┐
       ▼    ▼    ▼
     HTTP   WS  Modbus Srv
```

### 11.3 二期关键特性细节

**Lua 规则引擎热加载：**

```lua
-- /etc/edge-gateway/rules/boiler.lua
-- 修改后自动 reload（inotify 监听文件目录）

local temp_high = redis_get("alarm.boiler.temp_high") or 85.0
local prev_temp = 0

function on_data(dp)
    -- 变化率检测：超过 10°C/s 立即报警
    local dt = (dp.timestamp_us - prev_ts) / 1000000
    if dt > 0 and math.abs(dp.value.f64 - prev_temp) / dt > 10 then
        emit("alarm", { severity = "critical", message = "Temperature spike" })
    end
    prev_temp = dp.value.f64
    prev_ts   = dp.timestamp_us

    -- 正常更新
    if dp.value.f64 < temp_high then
        emit("cache_update", dp)
        emit("mqtt_publish", dp)
    else
        -- 超温只报警，不更新缓存（保留最后一组正常值）
        emit("alarm", { severity = "warning", message = "Over temperature" })
    end
end
```

**Modbus Server 寄存器映射：**

```c
typedef struct {
    uint16_t        reg_addr;       // Modbus 寄存器地址
    uint32_t        point_id;       // StateCache 中的数据点
    DataType        type;           // U16 / F32 / BOOL
} ModbusRegMap;

// Modbus 读请求处理
static int modbus_read_holding_regs(ModbusCtx *ctx, int addr, int count, uint16_t *out) {
    for (int i = 0; i < count; i++) {
        ModbusRegMap *map = find_map(ctx->reg_map, addr + i);
        if (!map) { out[i] = 0; continue; }

        DataPoint dp;
        cache_read(&ctx->state_cache[map->point_id], &dp);
        out[i] = value_to_u16(dp.value, map->type);
    }
    return 0;
}
```

### 11.4 二期里程碑

| 阶段 | 内容 |
|------|------|
| M7 | 解析线程池扩缩（动态增减 parser 线程） |
| M8 | Lua 规则引擎 + 热加载 |
| M9 | Modbus TCP Server + 寄存器映射表 |
| M10 | MQTT Client + WebSocket 推送 |
| M11 | 管理 REST API（CRUD + 启停） |
| M12 | 看门狗 + 健康检查 + 时序落盘 |

---

## 12. 故障场景行为

| 场景 | 行为 | 恢复 |
|------|------|------|
| **单个 Modbus 设备掉线** | epoll 返回 EPOLLHUP，标记 fd 断开，其他设备不受影响 | 重连逻辑自动恢复后重新注册 epoll |
| **所有设备掉线** | 采集线程无事件 → epoll_wait 超时 → 继续循环（空转） | 设备上线后 epoll 触发 |
| **解析线程崩溃** | RAW_QUEUE 积压 → 背压阻塞采集线程 → 设备 buffer 满 | 看门狗重启解析线程 |
| **处理线程 Hang** | 看门狗超时 → 标记 task_failed → 主线程 sch_stop 清理 | 日志记录后重启 |
| **磁盘满（Logger）** | Logger 转发线程 write() 失败，tq_send 不阻塞，动作丢弃 | 磁盘恢复后自动正常 |
| **HTTP API 慢** | 不影响——HTTP 是独立线程直接读 StateCache，不走队列 | — |
| **MQTT Broker 断连** | MQTT 转发线程 tq_receive 仍在消费队列但 publish 失败 | 重连成功后续发 |

---

## 附录

### A. 移植的 FFmpeg 组件

| 组件 | 来源 | 用途 |
|------|------|------|
| `AVBufferRef` / `av_buffer_ref/unref` | libavutil/buffer.h | 引用计数字节缓冲区（RawFrame 中的原始报文） |
| `ThreadQueue` (tq_send/tq_receive) | fftools/thread_queue.h | 线程安全有界队列（RAW_QUEUE/DP_QUEUE/ACT_QUEUE） |
| `SchWaiter` (waiter_wait/waiter_set) | fftools/ffmpeg_sched.c | 采集线程的 choke/unchoke 流控 |
| `task_start/task_stop` | fftools/ffmpeg_sched.c | 线程生命周期管理 |
| `sch_wait` (cond_timedwait) | fftools/ffmpeg_sched.c | 主线程监控循环 |

### B. epoll 为什么适用于采集阶段

```
操作             耗时           每线程每秒可处理        适用场景
─────────────────────────────────────────────────────────
blocking read    1-100ms      10-1000 次             一线程一 fd（N 线程）
non-blocking     0.001ms      1,000,000+ 次          epoll 单线程
read + epoll

工业采集通常每设备 50-1000ms 才产生一次数据。
epoll 单线程 50ms 超时轮询，空转时不消耗 CPU。
```

### C. 一期性能估算

```
场景：32 路 Modbus TCP 设备，每设备 100 个寄存器，轮询周期 500ms
数据量：32 设备 × 200 字节/帧 ÷ 0.5s = 12,800 字节/秒
帧率：  32 ÷ 0.5s = 64 帧/秒

采集线程：epoll_wait 每次返回 1-8 个事件, read() 后推入队列, 64 ops/s, CPU << 1%
解析线程：解析 Modbus 帧 → DataPoint, ~5000 points/s, 完全够用
处理线程：简单变换, 远超 64 points/s
转发线程：写入 StateCache, 内存操作

系统总开销：5 线程, CPU 使用率 < 5%
对比"每接口一线程"方案：32 线程, 调度开销大, 32×8MB=256MB 栈空间
```
