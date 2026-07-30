# HERDOS gamma 转换 MC 真值修改说明

## 目的

HERDOS 原版 `BasicTrackingAction` 会过滤掉 CALO 内产生的大多数次级粒子，因此原始
ROOT 文件中的 `mcparts` 不能确认 gamma 是否发生了
`gamma -> electron + positron` 对产生。

本修改保存 Geant4 创建过程为 `conv` 的电子和正电子，包括低于默认 5 MeV
次级粒子阈值的转换产物。

## 版本与位置

- HERDOS 基线提交：`7cd27b7`
- 个人开发分支：`zhangjin/gamma-conversion-truth`
- 修改提交：`0df164a Save gamma conversion products in MC truth`
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
| gamma 对转换产物 | `(simstat & 2) != 0` |

严格确认一次对转换时，应找到同一顶点的一对 `PDG=11` 和 `PDG=-11`，
二者 `parentID` 相同，并且 `simstat & 2` 非零。

## 已完成验证

使用 v2025a-scdX、1 GeV gamma、vertical-5x5、单事例验证，找到：

```text
gamma: trackID=3, PDG=22
positron: trackID=4, parentID=3, PDG=-11, simstat=2
electron: trackID=5, parentID=3, PDG=11, simstat=2
conversion vertex = (5.434, -4.371, -6.379) cm
|p(e+)| + |p(e-)| = 0.5406 + 0.4594 ~= 1.0000 GeV
```

测试文件：

```text
/herdfs/user/zhangjin0101/HERD/results/v2025a-test/gamma_conversion_truth_test.root
```

注意：该测试文件生成后又修正了电子/正电子的 `charge` 写法，所以此测试文件中的
`charge` 仍为 0；它的 PDG、父子关系、顶点和 `simstat` 验证有效。此后用当前安装
重新生成的文件会写出电子 `charge=-1`、正电子 `charge=+1`。

## 文件大小影响

同一随机种子、同一单事例参数下：

| 文件 | 大小 |
|---|---:|
| 修改前 `my_first_gamma.root` | 64,227 bytes |
| 修改后 `gamma_conversion_truth_test.root` | 66,190 bytes |
| 增量 | 1,963 bytes（3.06%） |

这个 1 GeV 事件保存了约 34 对转换产物。体积增幅与入射能量、簇射复杂度和
对转换次数有关，不能把 3.06% 当成所有作业的固定比例。正式大规模生产前应先用
100–1000 个事例测量平均文件大小和运行时间。

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
```

在与基线兼容的 HERDOS 源码目录中执行：

```bash
git switch -c zhangjin/gamma-conversion-truth
git am /herdfs/user/zhangjin0101/HERD/development/herdos-patches/0001-Save-gamma-conversion-products-in-MC-truth.patch
cmake --build /scratchfs/herd/zhangjin0101/HERDOS/v2025a/build --target SimConfiger -j4
cmake --install /scratchfs/herd/zhangjin0101/HERDOS/v2025a/build
```

应用补丁前应确认工作树干净，并检查当前 HERDOS 版本是否仍与补丁基线兼容。

## 注意事项

1. 这是个人分析扩展，不是课题组官方版本。
2. 不要修改或启用官方 GitLab 的 push 地址。
3. 当前会保存簇射中每一次 `conv` 产生的电子和正电子，而不只是第一次转换。
4. 高能事件可能显著增加 `mcparts` 数量；大规模生产前必须做小样本容量测试。
5. 原有 ROOT schema 没有变化，旧读取程序仍可读取新文件。
6. 分析时必须使用 `simstat & 2` 和父子关系，不要只凭 `PDG=11/-11` 判断首次转换。
