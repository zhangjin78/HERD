# HERDOS 工作区目录说明

```text
HERD/
├── code/
│   └── offline/                 课题组 HERDOS 源码仓库
├── configs/
│   └── datasets/                数据集定义与 job-level split
├── scripts/
│   ├── analysis/                特征、传统重建和 ML 代码
│   ├── condor/                  模拟作业提交脚本
│   ├── environment/             环境初始化与依赖
│   ├── geometry/                几何检查工具
│   └── Display/                 事例显示工具
├── notes/
│   └── analysis/                教程、字段字典和阶段记录
├── results/
│   ├── production/              原始生产 ROOT；逐子作业保存，永不覆盖
│   ├── derived/                 特征、统计和预测的完整 tar 快照
│   ├── tests/                   小规模测试和验证数据
│   └── _archive/                旧版、失败和不完整结果
├── figures/
│   └── <dataset>/<stage>/<tag>/ 可直接浏览的最终或 partial PNG
└── runs/
    └── condor/                  作业日志、提交记录和运行状态
```

## 规则

- `results/production` 中的 ROOT 是原始数据，不压缩成 tar。
- `results/derived` 的 tar 是一次分析运行的完整快照，用于原子归档和复现。
- PNG 从分析快照中单独发布到 `figures`，不需要解 tar 才能查看。
- partial 图片的目录名必须包含 `partial_NofM`，不能冒充完整结果。
- 旧版与失败目录只移动到 `_archive`，不直接删除。
- `results`、`figures`、`runs`、ROOT、CSV、模型权重均不提交 Git。
- Git 只保存 `configs`、`scripts` 和 `notes`。

## 当前图片

```text
figures/gamma1GeV_v2025a_prod04/stage01/config_v1_complete/
figures/gamma_0p05to20GeV_powerlaw_m1_v2025a_prod05/
  stage01/partial_7of10_config_v1/
```

两处各有 16 张非零 PNG。
