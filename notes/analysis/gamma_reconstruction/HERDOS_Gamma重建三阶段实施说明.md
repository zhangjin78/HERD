# HERDOS Gamma 重建三阶段实施说明

## 研究目标

项目按顺序执行：

1. 数据特征与分布；
2. 传统方法能量重建；
3. 机器学习多任务重建。

任何后续阶段不得绕过前一阶段的验收。现有两批 `vertical-5x5`
数据不用于证明方向重建能力；方向任务需要独立的 0–45° 多角度样本。

## 数据集

| ID | 能量 | 事例 | 用途 |
|---|---|---:|---|
| `gamma1GeV_v2025a_prod04` | 1 GeV mono | 200,000 | 单能响应、几何效应、流程复现 |
| `gamma_0p05to20GeV_powerlaw_m1_v2025a_prod05` | 0.05–20 GeV, E^-1 | 1,000,000 | 能量依赖、线性与分辨率 |

数据状态由 `configs/datasets/*.json` 决定。`partial` 数据只允许调试；
只有文件数、事件数、作业号和校验全部通过后才能标记 `frozen`。

## 目录与版本边界

Git 只管理：

- `configs/`：数据集和实验配置；
- `scripts/`：分析、重建、训练与环境脚本；
- `notes/`：字段说明、教程、阶段报告。

原始数据和生成结果保存在：

```text
results/production/<dataset>/
results/derived/<dataset>/
results/analysis/<dataset>/stage01/
results/analysis/<dataset>/stage02/
results/analysis/<dataset>/stage03/
```

ROOT、CSV、图片、日志、预测、模型权重不进入 Git。`code/offline` 是课题组
仓库，不由外层个人仓库提交其源代码内容。

## 阶段门

### Stage 01

- 完整读取预期事件；
- 作业级 train/validation/test 划分无交叉；
- 转换、未转换、CALO 有效和零沉积分类闭合；
- 输出数据清单、字段表、质量统计和带选择条件的图；
- prod04 必须复现原有统计结果；
- prod05 必须等到 10/10 子作业完成后才能冻结正式报告。

### Stage 02

- 主样本为 `converted && calo_edep>0`；
- 所有 `calo_edep>0` 作为扩展样本另行报告；
- 所有标定只拟合 train，在 validation 选择，在 test 最终评价；
- 依次比较总沉积、单调标定和解析泄漏修正。

### Stage 03

- 先做工程特征 MLP，再做 CALO 3D CNN；
- CALO-only 为主结果，CALO+STK 为增益对照；
- 多任务输出为能量、转换分类、转换顶点和入射方向；
- 各任务损失按真值与有效沉积掩码计算；
- 方向头只在独立多角度数据上训练和评价。

## 推荐工作命令

```bash
source /cvmfs/herd.ihep.ac.cn/HERDOS/el9_amd64_gcc11/Release/v00-10/ExternalLibs/bashrc.sh
source /scratchfs/herd/zhangjin0101/HERDOS/v2025a/install/setup.sh

python3 scripts/analysis/gamma_calo_features/run_dataset_analysis.py \
  configs/datasets/gamma1GeV_v2025a_prod04.json
```

对未完成数据只允许显式使用：

```bash
python3 scripts/analysis/gamma_calo_features/run_dataset_analysis.py \
  configs/datasets/gamma_0p05to20GeV_powerlaw_m1_v2025a_prod05.json \
  --allow-partial
```

输出目录自动带 `complete` 或 `partial_NofM` 标签，且拒绝覆盖已有结果。
正式 HERDFS 发布默认为单一 tar 包并采用 `.partial` 后原子改名，避免多文件
复制导致零字节半成品。需要在节点 `/tmp` 展开检查时显式使用
`--publish-mode directory --results-base /tmp/...`。
