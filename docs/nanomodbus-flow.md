# nanoMODBUS 初始化与收帧处理流程（Server 侧）

## 背景

本文还原 nanoMODBUS 从创建实例到处理请求的完整内部路径。

核心假设：
- `nmbs_t` 实例已分配（栈或静态）
- `platform` 层已就绪（串口已 open / socket 已 listen）
- `nmbs_poll()` 被周期调用

---

## 零、初始化流程

### Server 侧创建

```
app 调用:
  nmbs_t t;  (或 static / heap)

  │
  ├── 1. 设置 platform 回调:
  │       t->platform.receive = my_receive   // 从物理端口读字节
  │       t->platform.send    = my_send      // 写字节到物理端口
  │
  ├── 2. 设置 app 回调:
  │       t->callbacks.read_registers  = my_read_regs
  │       t->callbacks.write_registers = my_write_regs
  │
  ├── 3. nmbs_server_create(&t, transport, unit_id, user_arg)
  │       │
  │       ├─ t->transport = RTU / TCP
  │       ├─ t->unit_id   = instance_unit_id
  │       ├─ t->user_arg  = user_arg
  │       │
  │       ├─ 内部分配 parser 缓冲区:
  │       │     t->recv_buf[EGW_MODBUS_MAX_FRAME]     ← 收字节环形缓冲
  │       │     t->resp_buf[EGW_MODBUS_MAX_FRAME]     ← 响应帧缓冲区
  │       │     t->parse_buf[EGW_MODBUS_MAX_FRAME]    ← 帧定界工作区
  │       │
  │       ├─ 状态重置:
  │       │     t->has_response = false
  │       │     t->state = NMBS_STATE_IDLE
  │       │     parser → reset
  │       │
  │       └─ return NMBS_OK / NMBS_ERROR
  │
  └── 此时实例就绪，等待 nmbs_poll() 被调用
```

### 初始化后的内存布局

```
nmbs_t {
  ┌─────────────────────────────┐
  │  transport: RTU             │← 影响 wrap_pdu / unwrap_frame 行为
  │  unit_id:   1               │← 只响应地址匹配的帧（0=广播）
  │  state:     NMBS_STATE_IDLE │
  │  ───────────────────────────│
  │  platform: { receive, send }│← 绑定到具体 fd / 串口
  │  callbacks: { read, write } │← app 提供的寄存器读写
  │  user_arg                   │← 透传给回调
  │  ───────────────────────────│
  │  recv_buf[260]              │← platform.receive 写入
  │  parse_buf[260]             │← 帧定界工作区（指针/偏移）
  │  resp_buf[260]              │← 组好的响应帧
  │  has_response: false        │
  │  ───────────────────────────│
  │  // Client 侧专用 (Server 不用)
  │  req_buf, deadline, ...     │
  └─────────────────────────────┘
```

### 与我们的差异

| 步骤 | nanoMODBUS | 我们 |
|------|-----------|------|
| 实例分配 | app 提供栈/静态内存 | `egw_modbus_server_create(&params)` 内部 calloc |
| platform 设置 | app 手动赋值 `t->platform = {...}` | `egw_transport_open` 返回 handle，app 持有 |
| 回调注册 | create 时传 callbacks struct | create 时传 `read_cb, write_cb, arg` |
| 输运层分派 | 全局函数内 switch transport | transport 枚举 + `egw_modbus_encode/decode` 统一分发 |
| 多 unit 支持 | 单 unit_id，多实例 | 单个实例支持 `unit_mask[32]` 位图（1~247） |
| 环形缓冲区 | `recv_buf` + 内部 offset 追踪 | `buf[rd/wr]` 真环形缓冲区（可回绕） |
| 响应发送 | `has_response` + platform.send | `sending` 标志 + app 调 `get_response`/`response_sent` |

---

## 一、公共路径：从收到字节到帧就绪

```
nmbs_poll(t, now_ms)
  │
  ├─ recv_len = platform.t->receive(t->recv_buf, sizeof(t->recv_buf))
  │     │
  │     └─ recv_len > 0 ?
  │           │
  │           ├─ 塞入内部 ring buffer / parser
  │           └─ 尝试帧定界：
  │                 RTU:  从 ring buf 找完整 ADU（unit + fc + data + crc16）
  │                 TCP:  读 MBAP header → 取 length → 收齐 payload
  │
  └─ frame 就绪 ?
        │
        ├─ 校验：
        │     RTU: crc16(frame, len-2) == frame[len-2..len-1]
        │     TCP:  无 CRC（TCP 本身校验）但检查 mbap_len 一致性
        │
        └─ 校验通过 → goto dispatch
```

---

## 二、分发：解析 ADU → PDU

```
dispatch:                                ← 仍在 nmbs_poll() 内部，引擎层
  │
  ├─ unwrap_frame(frame, len) → unit_id + pdu
  │     RTU:  frame[0] = unit_id, frame[1..len-3] = pdu
  │     TCP:  frame[6] = unit_id, frame[7..7+mbap_len-2] = pdu
  │
  └─ 匹配 unit_id:
        │
        ├─ unit_id == t->unit_id → 命中
        └─ unit_id != t->unit_id → 丢弃（广播 0 经 write 回调处理）
```

校验通过后还在引擎内部，没有返回到 app。直到匹配到 unit_id 后，**读请求/写请求分支里才会调 callbacks**——那才是回到 app 层。

所以完整调用链是：

```
app 层:    nmbs_poll(&t, now_ms)
              │
引擎内部:    recv → 定界 → 校验 → dispatch → unwrap → 匹配 unit_id
              │
              ├─ 读请求 → callbacks.read_registers(...)  ─→ 回到 app 层
              └─ 写请求 → callbacks.write_registers(...) ─→ 回到 app 层
                            │
引擎内部:    回调返回后 → 组响应 → has_response = true
下个 tick:  platform.send(resp_buf)
```

---

## 三、读请求动作（FC01-04）

```
pdu[0] = fc ∈ {0x01, 0x02, 0x03, 0x04}

  │
  ├─ 解析 PDU:
  │     addr  = (pdu[1] << 8) | pdu[2]
  │     count = (pdu[3] << 8) | pdu[4]
  │
  ├─ 参数校验:
  │      addr + count ≤ 65536
  │      count 在合理范围内（1~2000 for coils, 1~125 for regs）
  │      ↓ 不通过 → 构建异常响应（ILLEGAL_DATA_ADDR）
  │
  ├─ 读数据:
  │     ┌─────────────────────────────────────────┐
  │     │ t->callbacks.read_registers(            │
  │     │     addr, count, unit_id,               │
  │     │     t->reg_buf, t->user_arg);           │
  │     │                                         │
  │     │  回调由 app 实现：                       │
  │     │  • 从 app 的寄存器模型读 addr ~ addr+count│
  │     │  • 写入 reg_buf（CPU 字节序）            │
  │     │  • 返回 EGW_OK / 错误                    │
  │     └─────────────────────────────────────────┘
  │
  ├─ 回调结果:
  │     │
  │     ├─ EGW_OK → 构建正常响应:
  │     │     resp_pdu = fc | byte_count | data(data_len)
  │     │     data = cpu_to_be(regs)  /  coils → 位图
  │     │
  │     └─ error  → 构建异常响应:
  │           resp_pdu = (fc | 0x80) | SLAVE_DEVICE_FAILURE
  │
  └─ 封装响应:
        resp_frame = encode_pdu(resp_pdu, unit_id)
          RTU:  unit_id + pdu + crc16
          TCP:  tid(0) + proto(0) + len + unit_id + pdu
        t->has_response = true
        t->resp_frame  = resp_frame
        t->resp_len    = frame_len
```

### 关键点

| 步骤 | 谁做 | 备注 |
|------|------|------|
| PDU 解析 | 引擎 | `parse_request()` |
| 参数校验 | 引擎 | addr/count 范围、功能码合法性 |
| 实际读寄存器 | **app 回调** | 引擎完全不碰寄存器 |
| 字节序转换 | 引擎 | `regs[]` CPU 序 → 大端 byte stream |
| 响应封装 | 引擎 | encode_pdu（加 CRC/MBAP，通过函数指针分发 RTU/TCP） |

---

## 四、写请求动作（FC05/06/0F/10）

```
pdu[0] = fc ∈ {0x05, 0x06, 0x0F, 0x10}

  │
  ├─ 解析 PDU:
  │     addr     = (pdu[1] << 8) | pdu[2]
  │     count    = (pdu[3] << 8) | pdu[4]   (FC05/06 = 1)
  │     byte_cnt = pdu[5]                   (FC0F/10)
  │     data     = pdu + 6                  (FC0F/10)
  │              = pdu + 3                  (FC05/06)
  │
  ├─ 参数校验:
  │     addr + count ≤ 65536
  │     byte_cnt 与 count 一致性校验
  │     写多线圈时 data 够不够放 count 个 bit
  │     写多寄存器时 data 够不够放 count*2 字节
  │     ↓ 不通过 → 异常（ILLEGAL_DATA_ADDR / ILLEGAL_DATA_VAL）
  │
  ├─ 写数据:
  │     ┌───────────────────────────────────────────┐
  │     │ t->callbacks.write_registers(             │
  │     │     addr, count, unit_id,                 │
  │     │     t->write_buf, t->user_arg);           │
  │     │                                           │
  │     │  引擎先做字节序转换:                        │
  │     │  • FC05（写单线圈）: value = data[0..1]    │
  │     │  • FC06（写单寄存器）: value = data[0..1]   │
  │     │  • FC0F（写多线圈）:  位图 → uint16_t[]    │
  │     │  • FC10（写多寄存器）: 大端 → uint16_t[]   │
  │     │                                           │
  │     │  回调由 app 实现：                         │
  │     │  • 写入 app 的寄存器模型                    │
  │     │  • 返回 EGW_OK / 错误                     │
  │     └───────────────────────────────────────────┘
  │
  ├─ 回调结果:
  │     │
  │     ├─ EGW_OK → 构建正常响应:
  │     │     FC05/06: 回显原 PDU（echo）
  │     │     FC0F/10: 回显 addr + count（不包含数据体）
  │     │
  │     └─ error → 构建异常响应:
  │           resp_pdu = (fc | 0x80) | 异常码
  │
  └─ 封装响应（同读请求）:
        resp_frame = encode_pdu(resp_pdu, unit_id)
        t->has_response = true
```

### 写请求的特殊之处

| 功能码 | 正常响应格式 | 说明 |
|--------|-------------|------|
| FC05 (Write Single Coil) | 回显完整请求帧（8 字节） | 确认线圈值 |
| FC06 (Write Single Reg) | 回显完整请求帧（8 字节） | 确认寄存器值 |
| FC0F (Write Multiple Coils) | addr + count（8 字节） | 不含写入的数据体 |
| FC10 (Write Multiple Regs) | addr + count（8 字节） | 不含写入的数据体 |

异常响应统一格式：`(fc | 0x80) + exc_code`

---

## 五、异常码映射

| 条件 | 异常码 |
|------|--------|
| addr/count 超出寄存器范围 | `ILLEGAL_DATA_ADDR` (2) |
| count=0 或 count 超上限 | `ILLEGAL_DATA_VAL` (3) |
| 功能码不支持 | `ILLEGAL_FUNCTION` (1) |
| 回调返回错误 | `SLAVE_DEVICE_FAILURE` (4) |

---

## 六、发送响应

```
nmbs_poll(t, now_ms) 的后续 tick:
  │
  ├─ t->has_response == true ?
  │     │
  │     ├─ sent = platform.t->send(t->resp_frame, t->resp_len)
  │     │
  │     ├─ sent == t->resp_len ?
  │     │     │
  │     │     ├─ has_response = false
  │     │     └─ 重置 parser，等待下一帧
  │     │
  │     └─ sent < resp_len ?
  │           丢包 / 断开，下次重试或丢弃
```

---

## 八、对照：我们当前的实现

| nanoMODBUS | 我们 |
|-----------|------|
| app 声明 `nmbs_t t`（栈） | `calloc(sizeof(egw_modbus_server_t))` |
| `t->platform = {receive, send}` | `egw_transport_handle_t *h`（app 层持有） |
| `t->callbacks = {read, write}` | `egw_modbus_srv_read_cb` + `write_cb` 参数 |
| `t->unit_id = 1` | `params.unit_ids = {1, ...}`（链表+位图，支持多 unit） |
| 内部分配三个 buf | heap 分配 `buf` + `resp_buf`，容量可配置 |
| `nmbs_poll()` 内嵌 recv | App 层 `uv_poll_cb → egw_transport_read` |
| 内部 ring buf + parser | `buf[rd/wr]` 环形缓冲区 + `try_process` 内联定界 |
| `parse_request` + unwrap | `egw_modbus_decode_pdu` + `egw_modbus_parse_request` |
| `callbacks.read_registers` 回调 | `egw_modbus_srv_read_cb` 回调 |
| 内部 reg_buf 做字节序转换 | `regs[256]` 栈缓冲区 |
| `wrap_pdu` 封装响应 | `egw_modbus_encode(transport, &enc, &len)` |
| `has_response + platform.send` | `egw_modbus_server_get_response` + `egw_transport_write` |
| Client 侧：`nmbs_make_request` | `egw_modbus_client_register` → `egw_modbus_encode` → slot |
| | `egw_modbus_client_request(cli, slot, &len)` 显式指定 slot |

**核心差异**：nanoMODBUS 把数据读写（回调）和字节序转换都放在同一 tick 里，我们也一样——`server_handle_frame` 里调回调 → 取 regs → 构建 resp_pdu → `egw_modbus_encode()`（transport 枚举分派 RTU/TCP）。
