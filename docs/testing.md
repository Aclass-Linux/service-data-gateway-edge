# 测试流程

## 1. 加载环境

```bash
source aclass.env.sh
```

## 2. 编译

```bash
build
```

## 3. 单元测试

```bash
ctest --test-dir build --output-on-failure
```

## 4. 启动虚拟串口

```bash
./tools/virtual_serial.sh start
```

## 5. 初始化数据库（首次或重置时）

```bash
rm -f config.db && python3 tools/init_db.py config.db
```

## 6. 运行网关（含 Modbus 本地回环）

```bash
run
```

## 7. 预期输出

- 三张表加载成功（southbound / northbound / route 各 3 行）
- `[server] read_cb: addr=0 qty=2`
- `[client] done_cb: unit=1 sig=100 value.u32=0x000A000B`
- exit code: 0

## 8. 收尾

```bash
./tools/virtual_serial.sh stop
```

---

改代码后只需循环步骤 **2 → 6**，虚拟串口不用重启。
