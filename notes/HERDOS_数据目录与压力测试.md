# HERDOS 数据目录与压力测试

## 数据目录规则

```text
results/
├── tests/                         # 所有测试、调试和性能测量
│   ├── examples/                  # 少量事例学习
│   └── benchmarks/                # 压力测试
│       └── <geometry>/
│           └── <date>_<configuration>/
└── production/                    # 正式生产数据，禁止放测试文件
```

命名建议：

```text
<particle>_<energy>_<incidence>_<nevents>_<truth-tag>.root
```

例如：

```text
gamma_1GeV_vertical_100_firstconv.root
```

每个 benchmark 目录同时保存：

- ROOT 输出；
- 完整运行日志；
- `MANIFEST.txt`，记录软件版本、参数、节点、时间和结论。

`results/` 已被个人仓库 `.gitignore` 忽略，因此大数据不会推送到 GitHub；Git 只保存
目录规范、分析脚本和结论。

## 2026-07-30：v2025a 1 GeV gamma 压力测试

测试路径：

```text
/herdfs/user/zhangjin0101/HERD/results/tests/benchmarks/v2025a/20260730_gamma_1GeV_vertical_firstconv/
```

共同配置：

```text
HERDOS_INSTALL=/scratchfs/herd/zhangjin0101/HERDOS/v2025a/install
geometry=v2025a/v2025a-scdX.xml
particle=gamma
energy=1 GeV
g4mac=vertical-5x5
MC truth=仅初级 gamma 的第一次对转换
host=lxlogin008.ihep.ac.cn
```

节点当时负载很高，load average 约为 94–97，以下结果是初步容量估算，不应替代
批处理工作节点上的正式 benchmark。

### 实测

| 事件数 | 墙钟时间 | User CPU | System CPU | 峰值内存 | ROOT 大小 |
|---:|---:|---:|---:|---:|---:|
| 1 | 23.29 s | 20.52 s | 0.85 s | 610,388 kB | 64,621 bytes |
| 100 | 26.07 s | 22.20 s | 0.91 s | 610,548 kB | 229,860 bytes |

两次任务均显示：

```text
SNiPER::Context Terminated Successfully
```

100 事件任务实际处理：

```text
events processed 100
```

### 简单时间模型

由 1 和 100 事件测量拟合：

```text
T(N) ≈ 23.262 s + 0.02808 s × N
```

- `23.262 s`：几何、Geant4、服务和输出初始化；
- `0.02808 s/event`：当前节点负载下的边际墙钟时间。

初步外推：

| 事件数 | 预计时间 |
|---:|---:|
| 1,000 | 51 s |
| 10,000 | 5.1 min |
| 100,000 | 47 min |
| 1,000,000 | 7.8 h |

这些数字没有包括排队时间、远程存储拥塞和不同能量导致的簇射复杂度变化。

### 简单文件大小模型

由两个文件估算：

```text
S(N) ≈ 62,952 bytes + 1,669 bytes × N
```

初步外推：

| 事件数 | 预计 ROOT 大小 |
|---:|---:|
| 1,000 | 1.7 MB |
| 10,000 | 16.8 MB |
| 100,000 | 167 MB |
| 1,000,000 | 1.67 GB |

ROOT 压缩率会随事件内容变化，因此大规模提交前应使用目标能谱和真实入射方式再测。

## 作业规模建议

1. 登录节点只用于 1–100 个事件的功能检查。
2. 正式模拟通过批处理系统提交，不在登录节点直接长时间运行。
3. 首轮批处理建议每作业 10,000 事件，验证稳定性和输出大小。
4. 稳定后可尝试每作业 100,000 事件，使初始化开销低于总时间的约 1%。
5. 不建议直接提交单个百万事件作业；失败后重跑成本和单文件管理成本都较高。
6. 不同能量、能谱和入射配置分别建 benchmark 目录，不能只使用 1 GeV 结果外推全部样本。
