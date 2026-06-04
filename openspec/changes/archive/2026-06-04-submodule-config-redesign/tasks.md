## 1. submodule.sh 重写（读写 .project.submodules）

- [x] 1.1 `_dgh_submodule_add`：写 `.project.submodules`（url/path/tag）→ `git submodule add` → 有 tag 则 checkout
- [x] 1.2 `_dgh_submodule_rm`：清 `.project.submodules` 段落 → `git submodule deinit -f` → `git rm -f`
- [x] 1.3 `_dgh_submodule_sync`：从 `.project.submodules` 读取依赖，双向同步逻辑不变

## 2. toolchain.sh 修改

- [x] 2.1 `_dgh_check_submodules`：读 `.project.submodules` 替代 `.gitmodules`

## 3. .project.submodules 初始化

- [x] 3.1 创建空 `.project.submodules` 并提交
- [x] 3.2 `aclass.env.sh` source 时检查 `.project.submodules` 是否存在，不存在则创建空文件

## 4. 删除旧 .gitmodules tag 逻辑

- [x] 4.1 清理 `.gitmodules` 中残留的自定义 tag 字段（如果存在）

## 5. 验证

- [x] 5.1 `submodule-add` 带 tag：`.project.submodules` 含 tag，`.gitmodules` 无 tag
- [x] 5.2 `submodule-rm`：`.project.submodules` 段落被清，`.gitmodules` 也被清
- [x] 5.3 `submodule-sync`：按 `.project.submodules` 补缺失
- [x] 5.4 `build` 正常