## 1. 删除代码

- [x] 1.1 删除 `src/app/simulator.c`
- [x] 1.2 删除 `src/app/data_gen.h`、`src/app/data_gen.c`
- [x] 1.3 删除 `src/app/pipe_util.h`、`src/app/pipe_util.c`
- [x] 1.4 删除 `src/app/fifo_writer.h`、`src/app/fifo_writer.c`

## 2. 清理构建

- [x] 2.1 回退 `src/app/CMakeLists.txt`：移除 simulator 目标和 install

## 3. 清理规格

- [x] 3.1 删除 `openspec/specs/simulator-fifo/` 目录

## 4. 清理学习记录

- [x] 4.1 更新 `src/app/LEARN.md`：删除 simulator 相关章节

## 5. 验证

- [x] 5.1 `build` 编译通过，`gateway` 正常运行
- [x] 5.2 确认 `openspec/specs/simulator-fifo/` 已删除
