# 已保存 ROOT 文件中可用于 CALO γ 重建的信息

更新日期：2026-08-03。本文只盘点当前个人 HERD 工作区
`/herdfs/user/zhangjin0101/HERD/results/` 中已经保存的文件；不包含其他课题组或 DMS 数据。

## 结论

用于正式 CALO γ 重建、能量响应研究或训练的首选原始输入是以下两批冻结生产数据：

1. `gamma1GeV_v2025a_prod04`：固定 1 GeV、垂直 5×5 入射，共 200,000 事例；适合建立单能量基准、调试重建算法。
2. `gamma_0p05to20GeV_powerlaw_m1_v2025a_prod05`：50 MeV–20 GeV、幂律指数 −1、垂直 5×5 入射，共 1,000,000 事例；适合能量依赖、响应函数和训练/验证/测试研究。

两批原始文件均保存了事件级 CALO 晶体能量沉积、Geant 命中和 MC 粒子真值；但**未保存标准 CALO 重建 cluster／shower collection**。从原始文件开始做重建时，应以 `calohits` 为主要输入；`gnhits_calo` 与 `mcparts` 仅用于 MC 真值验证、监督学习标签或诊断，不应混入数据型重建输入。

## 原始 CALO 信息（两批正式生产数据及下列测试文件均已验证存在）

事件树为 `events`。下列 collection/分支可直接读取：

| Collection | 可用字段 | 对 CALO γ 重建的用途 |
|---|---|---|
| `calohits` | `ix, iy, iz, edep, fracEM, fracBS, pos.{x,y,z}, localpos.{x,y,z}, tfirst, tmean, tsigma` | 逐晶体能量、三维 shower 图像、层向/横向 profile、主轴和能量估计的主要输入。|
| `gnhits_calo` | `cellCode, edep, pos, localpos, localend, pathlen, time, momentum, trackID, pdgID` | Geant 级命中。可用于分析能量沉积来源、次级粒子贡献和逐步过程；不适合作为将来真实数据直接可得的输入。|
| `mcparts` | `pdgID, trackID, parentID, momentum, vertex, charge, mass, time, simstat` | 初级 γ 真值、首次 e+/e− 对转换与未转换初级 γ 的标记/末态；可提供能量、入射方向、转换点和监督标签。|

当前生产配置中的真值约定：`simstat` bit0=初级粒子，bit1=初级 γ 的首次对转换 e+/e−，bit2=未发生首次对转换的初级 γ 最终状态。

## A. 正式原始生产数据（优先使用）

### A1. 固定 1 GeV：`gamma1GeV_v2025a_prod04`

状态：`frozen`；几何：`v2025a/v2025a-scdX.xml`；共 4 个文件、每个 50,000 事例、合计 200,000 事例。

```text
results/production/gamma1GeV_v2025a_prod04/
├── gamma_1GeV_vertical_job000000_nevt50000_seed100000.root  # train
├── gamma_1GeV_vertical_job000001_nevt50000_seed100001.root  # train
├── gamma_1GeV_vertical_job000002_nevt50000_seed100002.root  # validation
└── gamma_1GeV_vertical_job000003_nevt50000_seed100003.root  # test
```

适用：固定能量响应、纵横向 shower 特征开发、单能量方向重建基准、算法单元测试。

### A2. 宽能谱：`gamma_0p05to20GeV_powerlaw_m1_v2025a_prod05`

状态：`frozen`；几何：`v2025a/v2025a-scdX.xml`；50 MeV–20 GeV，幂律指数 −1；共 10 个文件、每个 100,000 事例、合计 1,000,000 事例。

```text
results/production/gamma_0p05to20GeV_powerlaw_m1_v2025a_prod05/
├── gamma_0p05to20GeV_powerlaw_m1_vertical_job000000_nevt100000_seed100000.root  # train
├── gamma_0p05to20GeV_powerlaw_m1_vertical_job000001_nevt100000_seed100001.root  # train
├── gamma_0p05to20GeV_powerlaw_m1_vertical_job000002_nevt100000_seed100002.root  # train
├── gamma_0p05to20GeV_powerlaw_m1_vertical_job000003_nevt100000_seed100003.root  # train
├── gamma_0p05to20GeV_powerlaw_m1_vertical_job000004_nevt100000_seed100004.root  # train
├── gamma_0p05to20GeV_powerlaw_m1_vertical_job000005_nevt100000_seed100005.root  # train
├── gamma_0p05to20GeV_powerlaw_m1_vertical_job000006_nevt100000_seed100006.root  # validation
├── gamma_0p05to20GeV_powerlaw_m1_vertical_job000007_nevt100000_seed100007.root  # validation
├── gamma_0p05to20GeV_powerlaw_m1_vertical_job000008_nevt100000_seed100008.root  # test
└── gamma_0p05to20GeV_powerlaw_m1_vertical_job000009_nevt100000_seed100009.root  # test
```

适用：全能区能量响应/分辨、泄漏校正、能量依赖方向性能、ML 训练。必须按 job 保持 train/validation/test 划分，不能在一个 job 内拆分事件。

## B. 已生成的 CALO 重建结果（可直接做性能研究）

这两份不是原始命中，而是基于原始 `calohits` 生成的传统重建摘要，树名为 `traditional_reco`。

| 文件 | 事例数 | 主要分支 |
|---|---:|---|
| `results/derived/gamma1GeV_v2025a_prod04/stage02/traditional_v1_truth_seeded_direction/traditional_reco.root` | 200,000 | `calo_active, has_mc_pair, has_calo_axis, true_energy_GeV, calo_edep_GeV, ereco_log_cal_GeV, ereco_leakage_GeV, mc_conversion_{x,y,z}_cm, mc_pair_direction_delta_deg, calo_axis_direction_delta_deg` |
| `results/derived/gamma_0p05to20GeV_powerlaw_m1_v2025a_prod05/stage02/traditional_v1_truth_seeded_direction_rerun01/traditional_reco.root` | 1,000,000 | 同上 |

适用：无需重新读取逐晶体命中的能量响应、泄漏校正与方向残差研究。名称中的 `truth_seeded_direction` 表明它含有 MC 辅助信息；它适合算法开发与性能基准，不能直接代表独立于真值的最终物理重建。

## C. 小规模测试 ROOT（调试/可视化可用，不用于正式统计）

下列 12 个文件均已检查到 `calohits`、`gnhits_calo`、`mcparts` 和 `mcparts.simstat`：

```text
results/tests/manual/v2025a-initial/
  gamma_10GeV_vertical_v2025a-scdX_1evt.root                         (1)
  gamma_conversion_truth_test.root                                    (1)
  my_gamma_first_conversion_only.root                                 (1)
  my_first_gamma.root                                                 (1)
  my_gamma_with_truth.root                                            (1)

results/tests/benchmarks/v2025a/20260730_gamma_1GeV_vertical_firstconv/
  gamma_1GeV_vertical_1_firstconv.root                                (1)
  gamma_1GeV_vertical_100_firstconv.root                              (100)

results/tests/v2025a/
  gamma_1GeV_100_with_unconverted_final.root                          (100)

results/tests/spectrum-validation/20260730_gamma_0p05to20GeV_powerlaw_m1/
  gamma_0p05to20GeV_powerlaw_m1_vertical_job000000_nevt100_seed510000.root (100)

results/tests/condor-validation/prod02/
  gamma_1GeV_vertical_job000000_nevt1000_seed100000.root              (1000)
results/tests/condor-validation/prod03/
  gamma_1GeV_vertical_job000000_nevt1000_seed100000.root              (1000)
results/tests/batch/script_validation_20260730/
  gamma_1GeV_vertical_job000002_nevt1_seed310002.root                 (1)
```

括号为事例数。它们适合事件显示、ROOT 读取代码、truth 筛选和特征提取 smoke test；不可用于性能结论或模型训练的正式样本。

## D. 不作为事件级 CALO 重建输入的 ROOT

`results/_archive/` 中的 `analysis_histograms.root` 是已归档的直方图产物，不含原始 `events` 逐晶体命中，不可用于重新训练、重建或事件显示。它们只能用于历史图形/结果追溯。

## 推荐使用顺序

1. 用 `results/tests/manual/.../my_gamma_with_truth.root` 或 100 事例测试文件调试读取代码和事件显示。
2. 用 A1（1 GeV）建立无能谱混合的 CALO 基线。
3. 用 A2（宽能谱）开展能量依赖和训练研究；保持配置定义的 job-level 数据划分。
4. 用 B 中的 `traditional_reco.root` 快速比较传统基线与后续算法；需要新的三维特征时再回到 A 中的 `calohits`。

## E. `calohits` 详细说明：当前应如何使用

### E1. 它是什么

`calohits` 是模拟阶段按 **CALO 晶体** 汇总的能量沉积 collection；一条 hit 对应一个有能量沉积的晶体，而不是 Geant4 的每一个 step。每个事件的 hit 数是可变的，ROOT 中表现为 `calohits_` 和长度为 `calohits_` 的数组分支。因此它天然是稀疏三维图像。

它是当前文件中最接近“理想化 CALO 读出”的对象：含真实能量沉积，但尚未经过光产额、光电探测器、电子学噪声、阈值和标定等 digitization/reconstruction。它适合首先开发 shower 形状、能量与方向算法；若研究真实探测器响应，需要后续加入 digitization。

### E2. 当前生产文件中实际可用的字段

| 字段 | 含义 | 当前推荐用法 |
|---|---|---|
| `ix`, `iy` | 晶体横向整数索引，范围 0–22 | 构造横向能量图、重心和横向展宽。|
| `iz` | 晶体层索引，范围 0–20 | 构造纵向 profile。当前配置记录 `incoming_direction=-z`、`last_layer=0`：入射面一侧为较大的 `iz`，粒子沿 −z 传播时层号趋向 0。|
| `edep` | 该晶体的总能量沉积，单位 GeV | 唯一应作为能量权重的输入。对 1 GeV 生产样本的第 0 事例，`Σ edep = 0.966409 GeV`，与派生文件的 `calo_edep_GeV=0.966409` 完全一致。|

可将事件转为张量 `E[iz][ix][iy]`，尺寸为 **21 × 23 × 23**。初始化为零后，对每条 hit 做 `E[iz][ix][iy] += edep`；即使当前通常是一晶体一条 hit，也应使用累加而非赋值，保证读取逻辑稳健。

### E3. 当前不可用/不应直接使用的字段

对固定 1 GeV 正式生产文件的第 0 事例（99 个 CALO hit）已实际检查：

```text
pos.{x,y,z} = 0
localpos.{x,y,z} = 0
tfirst = tmean = tsigma = 0
fracEM = fracBS = 0
```

因此本批生产 ROOT 中这些字段只是 schema 保留位，**不能**用于物理坐标、hit 时间、EM 成分或反散射成分。几何坐标应由 `(ix, iy, iz)` 和 v2025a 几何 XML 映射得到；时间/粒子成分研究应转向 `gnhits_calo`，并明确其仅为 Geant 真值级信息。

### E4. 从 `calohits` 可先做的重建量

对每一个事件，优先构造：

1. 总沉积能量：`Ecal = Σ edep`；它是最基础的能量估计量。
2. 纵向 profile：`E_k = Σ_{ix,iy} E[k][ix][iy]`，用于 shower 起始、最大层、泄漏特征和能量校正。
3. 横向图像：每层 `E[ix][iy]` 或对若干层求和，用于重心、横向宽度与 shower 方向。
4. 能量加权重心与协方差/PCA 主轴：用晶体索引，或映射后的几何中心坐标，拟合 CALO shower 轴。
5. ML 输入：使用 21×23×23 的零填充能量体素；训练时只使用 `calohits`，标签从 `mcparts` 派生，并保持既定 job-level train/validation/test 划分。

`gnhits_calo` 可以用于回答“哪些粒子、在哪些 step 造成了沉积”，`mcparts` 用于回答“初级 γ 的真值是什么”；二者不应成为数据型 CALO 重建的特征输入。
