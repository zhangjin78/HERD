# HERDOS v2025a 本地测试教程

## 1. 适用范围

本教程用于在高能所登录节点上运行 HERDOS `v2025a` 开发几何的简单测试。

当前测试环境：

```text
源码分支：origin/66-updated-detector-geometries
源码提交：7cd27b7 (CALO PD update)
几何入口：v2025a/v2025a-scdX.xml
外部依赖：HERDOS v00-10 ExternalLibs
```

注意：`v2025a-scdX.xml` 标记为开发状态，文件中明确说明 STK 和 TRD 参数仍需更新。因此它适合软件和几何测试，不应直接用于正式物理生产。

## 2. 目录结构

```text
/scratchfs/herd/zhangjin0101/HERDOS/v2025a/
├── source/     # Git 源码 worktree
├── build/      # CMake 编译中间文件
├── install/    # 可运行的 HERDOS 安装
└── output/     # scratch 上的临时测试输出
```

持久保存的结果建议放在：

```text
/herdfs/user/zhangjin0101/HERD/results/v2025a-test/
```

`scratchfs` 适合源码展开、编译和临时计算，但不应视为长期归档空间。重要的 ROOT 文件、日志和笔记应保存到 `herdfs`。

## 3. 每次登录后的环境初始化

先加载 v00-10 外部依赖：

```bash
source /cvmfs/herd.ihep.ac.cn/HERDOS/el9_amd64_gcc11/Release/v00-10/ExternalLibs/bashrc.sh
```

再加载本地编译的 v2025a 开发版本：

```bash
source /scratchfs/herd/zhangjin0101/HERDOS/v2025a/install/setup.sh
```

顺序不能颠倒，也不能省略第一步，否则可能出现：

```text
ModuleNotFoundError: No module named 'Sniper'
```

检查当前环境：

```bash
echo "$HERDOS_INSTALL"
```

正确输出应为：

```text
/scratchfs/herd/zhangjin0101/HERDOS/v2025a/install
```

检查几何和入射宏：

```bash
ls "$HERDOS_INSTALL/compact/v2025a/v2025a-scdX.xml"
ls "$HERDOS_INSTALL/g4macro/comp_vertical-5x5.g4mac"
```

## 4. 运行第一个事例

建立持久输出目录：

```bash
mkdir -p /herdfs/user/zhangjin0101/HERD/results/v2025a-test
```

运行 1 个 10 GeV、垂直入射的 gamma：

```bash
python3 "$HERDOS_INSTALL/scripts/SimConfiger/devrun.py" \
  --particle gamma \
  --energy 10 \
  --geometry v2025a/v2025a-scdX.xml \
  --g4mac vertical-5x5 \
  -N 1 \
  -o /herdfs/user/zhangjin0101/HERD/results/v2025a-test/my_first_gamma.root
```

参数含义：

| 参数 | 含义 |
|---|---|
| `--particle gamma` | Geant4 粒子为 gamma |
| `--energy 10` | 固定能量 10 GeV |
| `--geometry ...` | 使用 v2025a-scdX 几何 |
| `--g4mac vertical-5x5` | 从顶部方形平面沿 `(0,0,-1)` 垂直入射 |
| `-N 1` | 只运行 1 个事例 |
| `-o ...root` | 指定 ROOT 输出文件 |

`devrun.py` 会自动把：

```text
vertical-5x5
```

解析成：

```text
$HERDOS_INSTALL/g4macro/comp_vertical-5x5.g4mac
```

因此不需要手动添加 `comp_` 和 `.g4mac`。

## 5. 同时保存运行日志

推荐使用：

```bash
python3 "$HERDOS_INSTALL/scripts/SimConfiger/devrun.py" \
  --particle gamma \
  --energy 10 \
  --geometry v2025a/v2025a-scdX.xml \
  --g4mac vertical-5x5 \
  -N 1 \
  -o /herdfs/user/zhangjin0101/HERD/results/v2025a-test/my_first_gamma.root \
  2>&1 | tee /herdfs/user/zhangjin0101/HERD/results/v2025a-test/my_first_gamma.log
```

## 6. 判断是否成功

日志末尾应出现：

```text
Sucessfully initialized GeometrySvc.
events processed 1
OutputFile
```

检查文件：

```bash
ls -lh /herdfs/user/zhangjin0101/HERD/results/v2025a-test/my_first_gamma.root
```

查看 ROOT 文件内容：

```bash
rootls -t /herdfs/user/zhangjin0101/HERD/results/v2025a-test/my_first_gamma.root
```

正常输出中应能找到：

```text
events
mcparts
calohits
stkhits
psdhits
scdhits
trdhits
```

注意：单个 gamma 事例具有随机性，不保证每个探测器都有非零 hit。判断程序是否成功，首先看几何初始化、处理事例数和 ROOT 数据结构。

## 7. 改变测试参数

运行 10 个事例：

```bash
-N 10
```

改为 1 GeV：

```bash
--energy 1
```

改为 20 GeV：

```bash
--energy 20
```

改为半径 1.8 m 球面入射：

```bash
--g4mac iso-R1.8m
```

每次修改参数时应使用不同输出文件名，例如：

```text
gamma_E1GeV_vertical_10evt.root
gamma_E10GeV_vertical_10evt.root
gamma_E20GeV_vertical_10evt.root
gamma_E10GeV_isoR1p8m_100evt.root
```

不要覆盖之前的 ROOT 文件和日志。

## 8. 查看源码状态

```bash
cd /scratchfs/herd/zhangjin0101/HERDOS/v2025a/source
git status --short --branch
git log -1 --oneline
```

当前 worktree 采用 detached HEAD，目的是防止误把个人测试提交到课题组开发分支。

不要在这个 worktree 中直接执行：

```bash
git push
```

## 9. 重新编译

只有修改源码或更新开发分支后才需要重新编译。普通运行不需要重复编译。

```bash
source /cvmfs/herd.ihep.ac.cn/HERDOS/el9_amd64_gcc11/Release/v00-10/ExternalLibs/bashrc.sh

cd /scratchfs/herd/zhangjin0101/HERDOS/v2025a/source

./build.sh \
  --build-dir=/scratchfs/herd/zhangjin0101/HERDOS/v2025a/build \
  --install-dir=/scratchfs/herd/zhangjin0101/HERDOS/v2025a/install \
  --type=Release
```

编译完成后重新执行：

```bash
source /scratchfs/herd/zhangjin0101/HERDOS/v2025a/install/setup.sh
```

## 10. 常见问题

### 找不到 Sniper

原因：没有先加载 ExternalLibs。

解决：

```bash
source /cvmfs/herd.ihep.ac.cn/HERDOS/el9_amd64_gcc11/Release/v00-10/ExternalLibs/bashrc.sh
source /scratchfs/herd/zhangjin0101/HERDOS/v2025a/install/setup.sh
```

### 找不到几何 XML

检查：

```bash
echo "$DDXMLPATH"
ls "$HERDOS_INSTALL/compact/v2025a/v2025a-scdX.xml"
```

### 找不到 Geant4 宏

检查：

```bash
echo "$G4MACROPATH"
ls "$HERDOS_INSTALL/g4macro/comp_vertical-5x5.g4mac"
```

### 输出文件不存在

检查输出目录是否存在并且可写：

```bash
mkdir -p /herdfs/user/zhangjin0101/HERD/results/v2025a-test
touch /herdfs/user/zhangjin0101/HERD/results/v2025a-test/write_test
rm /herdfs/user/zhangjin0101/HERD/results/v2025a-test/write_test
```

## 11. 已验证的基准

2026-07-30 已使用以下参数成功运行：

```text
particle  = gamma
energy    = 10 GeV
geometry  = v2025a/v2025a-scdX.xml
g4mac     = vertical-5x5
events    = 1
```

验证结果：

```text
GeometrySvc 初始化成功
几何节点数：48229
VolumeManager 节点数：16419
events processed：1
ROOT 输出：约 64 KB
```
