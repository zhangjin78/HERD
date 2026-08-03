# HERDOS v2025a：INK 网页可视化启动教程

更新日期：2026-08-03

本教程记录已经实际启动成功的 v2025a 事件显示环境。适用于由本个人分支生成的 v2025a 模拟 ROOT 文件。

## 一、需要填写的三个路径

在 INK 的 HERD Event Display 提交表单中，按以下方式填写。

```text
ROOT 文件：
/herdfs/user/zhangjin0101/HERD/results/production/gamma1GeV_v2025a_prod04/gamma_1GeV_vertical_job000000_nevt50000_seed100000.root

几何文件：
/herdfs/user/zhangjin0101/HERD/resources/geometry-cache/compact/v2025a/v2025a-scdX.xml

运行脚本：
/scratchfs2/herd/zhangjin0101/HERDOS/v2025a/runink_v2025a.sh
```

ROOT 文件可替换为其他 v2025a 生产或测试文件；其余两个路径保持不变。

## 二、为什么要使用新脚本

旧脚本 `/herdfs/user/zhangjin0101/HERD/scripts/Display/runink.sh` 中仍引用已经迁移消失的旧环境：

```text
/scratchfs/herd/zhangjin0101/HERDOS/v2025a/install/setup.sh
```

v2025a 几何中的 `TopSCD_v3` 需要个人编译的 `libFullGeometry.so` 插件。公共 v00-10 环境不包含该插件，因而会报：

```text
No factory with name Create(TopSCD_v3)
```

当前可用环境已经在 scratchfs2 重建：

```text
源码：/scratchfs2/herd/zhangjin0101/HERDOS/v2025a/source
构建：/scratchfs2/herd/zhangjin0101/HERDOS/v2025a/build
安装：/scratchfs2/herd/zhangjin0101/HERDOS/v2025a/install
```

`runink_v2025a.sh` 是旧显示脚本的稳定副本，只将 `setup.sh` 改为新的 scratchfs2 安装位置。

## 三、正常启动时的现象

日志出现下列信息是正常的，表示 Flask 网页服务已启动：

```text
running on  http://lhwsXXX:端口号
python display_server.py --flask_port 端口号 ...
```

随后从 INK 页面打开对应作业的访问链接。看到探测器几何、事件导航和能量投影后，说明几何与显示后端均已加载成功。

## 四、使用限制与维护

- 几何缓存位于 `resources/geometry-cache/`，由课题组 `offline` 的已保存 Git 分支恢复，仅用于运行；不要把它推送到个人 GitHub。
- 不要修改 `code/offline` 的 `master`。v2025a 源码 worktree 是 detached HEAD，提交号为 `e93260b`。
- scratchfs2 是运行与构建空间。若未来该存储再次迁移，应重新在可用路径检出该提交、执行 `build.sh`，再更新 `runink_v2025a.sh` 中唯一的 `setup.sh` 路径。
- 若更换到不同版本的几何，必须同时使用与该几何兼容的编译安装环境，不能只替换 XML 文件。

## 五、重新编译命令（仅在需要时）

```bash
source /cvmfs/herd.ihep.ac.cn/HERDOS/el9_amd64_gcc11/Release/v00-10/ExternalLibs/bashrc.sh
cd /scratchfs2/herd/zhangjin0101/HERDOS/v2025a/source
./build.sh \
  --build-dir=/scratchfs2/herd/zhangjin0101/HERDOS/v2025a/build \
  --install-dir=/scratchfs2/herd/zhangjin0101/HERDOS/v2025a/install \
  --type=Release
```
