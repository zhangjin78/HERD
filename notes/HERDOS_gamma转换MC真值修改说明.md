# HERDOS gamma 转换 MC 真值修改说明

## 目的

HERDOS 原版 `BasicTrackingAction` 会过滤掉 CALO 内产生的大多数次级粒子，因此原始
ROOT 文件中的 `mcparts` 不能确认 gamma 是否发生了
`gamma -> electron + positron` 对产生。

本修改只保存由初级入射 gamma 通过 Geant4 `conv` 过程直接产生的第一对电子和
正电子，包括低于默认 5 MeV 次级粒子阈值的转换产物。后续簇射 gamma 的转换
产物不保存。

## 版本与位置

- HERDOS 基线提交：`7cd27b7`
- 个人开发分支：`zhangjin/gamma-conversion-truth`
- 初始提交：`0df164a Save gamma conversion products in MC truth`
- 范围收紧提交：`da985c9 Keep only primary gamma first conversion truth`
- 未转换终态提交：`e93260b Save final state of unconverted primary gamma`
- 源码工作树：`/scratchfs/herd/zhangjin0101/HERDOS/v2025a/source`
- 安装目录：`/scratchfs/herd/zhangjin0101/HERDOS/v2025a/install`
- 官方仓库 push 地址已禁用，不要把个人修改推送到课题组仓库。

## 新增信息

对转换产生的电子、正电子写入现有 `mcparts` collection，保存：

- `pdgID`：电子为 `11`，正电子为 `-11`
- `trackID`
- `parentID`
- `momentum`，单位 GeV
- `vertex`，单位 cm
- `time`，单位 ns
- `charge`
- `simstat`

`simstat` 标志位定义：

| 条件 | 判断方法 |
|---|---|
| 初级粒子 | `(simstat & 1) != 0` |
| 初级 gamma 的第一对转换产物 | `(simstat & 2) != 0` |
| 未转换初级 gamma 的终态快照 | `(simstat & 4) != 0` |

严格确认第一次对转换时，应找到同一顶点的一对 `PDG=11` 和 `PDG=-11`，
二者 `parentID` 都等于初级 gamma 的 `trackID`，并且 `simstat & 2` 非零。

如果初级 gamma 到跟踪结束仍未通过 Geant4 `conv` 过程发生对转换，则额外保存
一条 `PDG=22`、`simstat & 4` 非零的终态快照：

- `vertex`：最终位置，单位 cm；
- `momentum`：最终三动量，单位 GeV；
- 最终能量：光子满足 `E = |p|`，由三动量模计算，单位 GeV。

该终态快照不设置 `kPrimary` 位，因此不会与原始入射 gamma 记录混淆；发生首次
对转换的事例仍只保存原有初级 gamma 和直接 e-/e+ 转换产物。

## 未转换 gamma 终态验证

使用 v2025a-scdX、1 GeV gamma、vertical-5x5、固定随机种子运行 100 个事例：

| 分类 | 事例数 |
|---|---:|
| 保存首次 e-/e+ 对 | 78 |
| 保存未转换 gamma 终态 | 22 |
| 同时属于两类 | 0 |
| 两类均未记录 | 0 |

抽查的未转换 gamma 均到达世界边界 `z=-500 cm`，最终动量仍为
`(0,0,-1) GeV`，最终能量仍为 1 GeV。这说明这些事例中的 gamma 完整穿过了
当前几何而未发生相互作用。

验证文件：

```text
/herdfs/user/zhangjin0101/HERD/results/tests/v2025a/gamma_1GeV_100_with_unconverted_final.root
```

## 已完成验证

使用 v2025a-scdX、1 GeV gamma、vertical-5x5、单事例验证，找到：

```text
gamma: trackID=3, PDG=22
positron: trackID=4, parentID=3, PDG=-11, simstat=2
electron: trackID=5, parentID=3, PDG=11, simstat=2
conversion vertex = (5.434, -4.371, -6.379) cm
|p(e+)| + |p(e-)| = 0.5406 + 0.4594 ~= 1.0000 GeV
```

最终范围收紧验证文件：

```text
/herdfs/user/zhangjin0101/HERD/results/v2025a-test/my_gamma_first_conversion_only.root
```

该文件只有 5 条 `mcparts`：3 条原有初级/辅助轨迹，加第一对电子和正电子。
电子 `charge=-1`，正电子 `charge=+1`。

## 文件大小影响

同一随机种子、同一单事例参数下：

| 文件 | 大小 |
|---|---:|
| 修改前 `my_first_gamma.root` | 64,227 bytes |
| 只保存第一次转换 `my_gamma_first_conversion_only.root` | 64,414 bytes |
| 增量 | 187 bytes（0.29%） |

由于每个发生首次对转换的事件最多新增一对粒子，体积增量受到控制。不过 ROOT
压缩和事件内容会影响比例，仍应在正式生产前用小样本测量平均文件大小。

## 运行方法

运行流程与原来相同：

```bash
source /cvmfs/herd.ihep.ac.cn/HERDOS/el9_amd64_gcc11/Release/v00-10/ExternalLibs/bashrc.sh
source /scratchfs/herd/zhangjin0101/HERDOS/v2025a/install/setup.sh

python3 "$HERDOS_INSTALL/scripts/SimConfiger/devrun.py" \
  --particle gamma \
  --energy 1 \
  --geometry v2025a/v2025a-scdX.xml \
  --g4mac vertical-5x5 \
  -N 1 \
  -o /herdfs/user/zhangjin0101/HERD/results/v2025a-test/my_gamma_with_truth.root
```

不要覆盖旧文件；使用新文件名，以便比较修改前后的结果。

只有加载上述个人 `install/setup.sh` 时才会使用修改后的 `libSimConfiger.so`。
如果加载 CVMFS 官方 Release 的 setup，得到的仍是官方行为。

## 从补丁恢复修改

补丁保存在个人仓库：

```text
development/herdos-patches/0001-Save-gamma-conversion-products-in-MC-truth.patch
development/herdos-patches/0002-Keep-only-primary-gamma-first-conversion-truth.patch
```

在与基线兼容的 HERDOS 源码目录中执行：

```bash
git switch -c zhangjin/gamma-conversion-truth
git am /herdfs/user/zhangjin0101/HERD/development/herdos-patches/0001-Save-gamma-conversion-products-in-MC-truth.patch
git am /herdfs/user/zhangjin0101/HERD/development/herdos-patches/0002-Keep-only-primary-gamma-first-conversion-truth.patch
git am /herdfs/user/zhangjin0101/HERD/development/herdos-patches/0003-Save-final-state-of-unconverted-primary-gamma.patch
cmake --build /scratchfs/herd/zhangjin0101/HERDOS/v2025a/build --target SimConfiger -j4
cmake --install /scratchfs/herd/zhangjin0101/HERDOS/v2025a/build
```

应用补丁前应确认工作树干净，并检查当前 HERDOS 版本是否仍与补丁基线兼容。

## 注意事项

1. 这是个人分析扩展，不是课题组官方版本。
2. 不要修改或启用官方 GitLab 的 push 地址。
3. 当前只保存初级入射 gamma 直接转换产生的第一对电子和正电子。
   未转换的初级 gamma 额外保存一条终态快照。
4. 如果以后研究完整电磁簇射真值，需要重新设计独立开关，不能直接用当前精简文件。
5. 原有 ROOT schema 没有变化，旧读取程序仍可读取新文件。
6. 分析时必须同时检查 `simstat & 2`、PDG 和 `parentID`，不要只凭电子/正电子判断首次转换。

## 读取第一次转换

分析宏：

```text
scripts/analysis/read_first_gamma_conversion.C
```

读取默认文件：

```bash
root -l -b -q \
  /herdfs/user/zhangjin0101/HERD/scripts/analysis/read_first_gamma_conversion.C
```

读取指定文件：

```bash
root -l -b -q \
  '/herdfs/user/zhangjin0101/HERD/scripts/analysis/read_first_gamma_conversion.C("/path/to/input.root")'
```

输出包括转换顶点、e⁻/e⁺ 三动量、单位方向向量、相对初级 gamma 的偏转角和方位角。
对于未转换事例，还会输出 `UNCONVERTED_GAMMA_FINAL`，包括最终位置、最终三动量
和由三动量模计算的最终能量。
