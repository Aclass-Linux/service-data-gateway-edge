## 1. submodule.sh 重写

- [x] 1.1 `_dgh_submodule_add`：`git submodule add --name <name>` + 有 tag 则 `git config -f .gitmodules` 写入 tag + checkout
- [x] 1.2 `_dgh_submodule_rm`：`git submodule deinit -f` + `git rm -f` + 清 `.git/modules/`
- [x] 1.3 `_dgh_submodule_sync`：双向同步（按 `.gitmodules` 补缺失 + 遍历 third-party 询问删除多余）

## 2. build.sh 修改

- [x] 2.1 cmake 配置前检查 `.gitmodules` 中定义的 submodule 是否缺失
- [x] 2.2 cmake 失败时若 submodule 缺失则打印提示

## 3. toolchain.sh 修改

- [x] 3.1 `_dgh_check_submodules` 改为读 `.gitmodules` 判断 submodule 状态，替换 glob 方式

## 4. 验证

- [x] 4.1 `submodule-add https://... third-party/xxx v1.0.0`：`.gitmodules` 含 tag 字段
- [x] 4.2 `submodule-add https://... third-party/xxx`：`.gitmodules` 无 tag 字段
- [x] 4.3 `submodule-rm third-party/xxx`：`.gitmodules` 中整段被清理
- [x] 4.4 `submodule-sync`：缺失的自动补，多余的逐个询问
- [x] 4.5 `build` cmake 失败时正确提示 submodule 缺失