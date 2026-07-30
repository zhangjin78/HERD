# HERDOS HTCondor 批量提交说明

## 文件

```text
scripts/condor/
├── submit_gamma_batch.sh   # 计算子作业数、检查参数并调用 hep_sub
└── run_gamma_subjob.sh     # 每个计算节点实际执行的 HERDOS 模拟
```

## 基本逻辑

```text
子作业数 = ceil(总事件数 / 每子作业事件数)
```

最后一个子作业自动使用剩余事件数。例如总数 1050、每作业 200：

```text
job 0–4：各 200 事件
job 5：50 事件
```

每个子作业使用：

```text
seed = base_seed + ProcId
runID = run_base + ProcId
```

输出文件名含 `ProcId`、事件数和 seed，不会互相覆盖。

## 先预览，不提交

```bash
cd /herdfs/user/zhangjin0101/HERD

scripts/condor/submit_gamma_batch.sh \
  --total-events 1000000 \
  --events-per-job 10000 \
  --tag gamma1GeV_v2025a_trial
```

默认是 dry-run，只显示计划和最终 `hep_sub` 命令。

## 正式提交

确认预览无误后，更换清晰且唯一的 tag：

```bash
scripts/condor/submit_gamma_batch.sh \
  --total-events 1000000 \
  --events-per-job 10000 \
  --tag gamma1GeV_v2025a_prod01 \
  --mode production \
  --submit
```

默认参数：

```text
particle=gamma
energy=1 GeV
geometry=v2025a/v2025a-scdX.xml
g4mac=vertical-5x5
memory=1500 MB
walltime=short（HERD 上限 30 分钟）
OS=AlmaLinux9
group=herd
dedicated worker group=HERD
```

如果每个子作业预计超过 30 分钟，使用：

```bash
--walltime mid
```

HERD 当前限制：

```text
test=5 分钟
short=30 分钟
default=40 小时
mid=200 小时
long=720 小时
```

## 测试与正式数据隔离

测试模式：

```text
results/tests/batch/<tag>/
```

正式模式：

```text
results/production/<tag>/
```

提交记录：

```text
runs/condor/<tag>/MANIFEST.txt
runs/condor/<tag>/submission.txt
runs/condor/<tag>/logs/job_<ProcId>.out
runs/condor/<tag>/logs/job_<ProcId>.err
```

## 查看作业

```bash
export PATH=/afs/ihep.ac.cn/soft/common/sysgroup/hep_job/bin:$PATH
hep_q -u
```

查看特定 cluster：

```bash
hep_q -i CLUSTER_ID
```

删除作业前先确认 ID：

```bash
hep_rm CLUSTER_ID
```

## 其他参数

能量范围：

```bash
--energy-range 0.5 20
```

修改随机种子起点：

```bash
--base-seed 200000
```

提高最大子作业数安全限制：

```bash
--max-jobs 5000
```

脚本默认最多提交 1000 个子作业。超过时必须显式提高限制，防止误操作。

## 注意事项

1. 不带 `--submit` 永远不会提交。
2. 相同 tag 已有 ROOT 文件或 MANIFEST 时会拒绝再次提交。
3. 每个作业先写计算节点临时目录，成功后才移动到最终目录，避免把半成品误当结果。
4. 当前使用个人 v2025a 安装以及“只保存初级 gamma 第一次对转换”的修改。
5. 登录节点只负责提交和查看，不承担大规模模拟。
6. 正式大批量前先提交 2–10 个测试子作业，确认工作节点能访问 CVMFS、scratchfs 和 herdfs。
