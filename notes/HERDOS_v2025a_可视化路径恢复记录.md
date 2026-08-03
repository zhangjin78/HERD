# HERDOS v2025a 可视化路径恢复记录

更新日期：2026-08-02

## 已验证的成功记录

2026-07-30 的 INK 可视化作业
`/workfs2/herd/zhangjin0101/.ink/Jobs/herddisplay-20260730-171636/55675.out`
成功读取了 `my_first_gamma.root` 并构建 v2025a 几何。该作业当时传入的几何为：

```text
/scratchfs/herd/zhangjin0101/HERDOS/v2025a/install/compact/v2025a/v2025a-scdX.xml
```

这个旧 scratchfs 挂载已迁移，目录不再存在；因此不能再在脚本中引用它。

## 当前稳定几何路径

v2025a 几何从个人分支 `zhangjin/gamma-conversion-truth` 恢复到个人 HERD 工作区缓存：

```text
/herdfs/user/zhangjin0101/HERD/resources/geometry-cache/compact/v2025a/v2025a-scdX.xml
```

缓存包含 v2025a 本体及其所需的 `common`、`v2024b`、CALO 几何依赖。它来自课题组 `offline` 的 Git 历史，**仅供运行使用**；外层个人仓库通过 `.gitignore` 忽略 `/resources/geometry-cache/`，不会将其推送到 GitHub。

## 使用方法

在 INK 作业表单中填写：

```text
ROOT 文件：/herdfs/user/zhangjin0101/HERD/results/v2025a-test/my_first_gamma.root
几何文件：/herdfs/user/zhangjin0101/HERD/resources/geometry-cache/compact/v2025a/v2025a-scdX.xml
运行脚本：/herdfs/user/zhangjin0101/HERD/scripts/Display/runink.sh
```

也可将几何字段写成 `v2025a/v2025a-scdX.xml` 或
`compact/v2025a/v2025a-scdX.xml`；脚本会优先从上述缓存解析。

## 恢复来源与注意事项

- 主源码工作树：`/herdfs/user/zhangjin0101/HERD/code/offline`，保持在课题组 `master`，不要在此处做个人修改。
- 保存 v2025a 的个人分支：`zhangjin/gamma-conversion-truth`。
- 旧 worktree：`/scratchfs/herd/zhangjin0101/HERDOS/v2025a/source` 已随存储迁移消失；其失效 Git 登记已清理。
- 若未来需要重新生成缓存，应从该分支导出几何，不要把几何缓存提交到个人 GitHub 仓库。
