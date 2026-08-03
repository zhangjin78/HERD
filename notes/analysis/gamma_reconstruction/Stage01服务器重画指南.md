# Stage-01 图件：服务器可复现运行指南

这份指南对应已经冻结的 prod04/prod05 派生特征包。它**不读取或改写原始 ROOT**；`tar` 中的 `event_features.csv.gz` 只是已提取特征的可靠打包形式，不是 ROOT 合并文件。

## 已验证的绘图环境（2026-08-03）

环境已在登录节点验证可用：

```text
Python:      3.11.13
解释器:      /scratchfs2/herd/zhangjin0101/envs/stage01-plot-py311/bin/python
numpy:       2.4.6
matplotlib:  3.11.1
```

这个环境只用于 Stage-01 的特征统计和静态绘图。每个新终端都必须先激活它；原先位于 HERDFS 的 `envs/stage01-plot` 安装失败，不能使用。

## 我此前实际采用的方法

我先运行统计脚本，把 tar 内的事件特征汇总为小型 CSV/JSON；再在带 `numpy` 和 `matplotlib` 的 Python 环境中绘图。之前最后一步是在本机临时环境完成的，所以服务器默认 `python3` 缺少 `matplotlib` 时会报错。这一版命令改为完全可以在 IHEP 服务器重现。

## 0. 一次性建立绘图环境

Python 虚拟环境包含大量小文件，**不要放在 HERDFS**；其元数据重命名容易失败。将环境放在个人 scratchfs，代码、原始数据和最终图仍留在 HERDFS。不要向系统 Python、CVMFS 或 `code/offline` 安装包。

```bash
cd /herdfs/user/zhangjin0101/HERD
mkdir -p /scratchfs2/herd/zhangjin0101/envs
python3.11 -m venv /scratchfs2/herd/zhangjin0101/envs/stage01-plot-py311
source /scratchfs2/herd/zhangjin0101/envs/stage01-plot-py311/bin/activate
python -m pip install --no-cache-dir -r scripts/environment/requirements-stage01-plot.txt
python -c 'import numpy, matplotlib; print("numpy", numpy.__version__, "matplotlib", matplotlib.__version__)'
```

以后每次新开终端，只需：

```bash
cd /herdfs/user/zhangjin0101/HERD
source /scratchfs2/herd/zhangjin0101/envs/stage01-plot-py311/bin/activate
```

若安装命令因网络/包源失败，停止并保留报错；不要改动系统 Python。

## 1. 重画图 02：二维能量响应与分箱响应

以下命令先在 scratch 构建中间 CSV，再一次性发布图。`work` 可以安全删除；最终图在 `figures/`。

```bash
cd /herdfs/user/zhangjin0101/HERD
source /scratchfs2/herd/zhangjin0101/envs/stage01-plot-py311/bin/activate

work=/scratchfs2/herd/zhangjin0101/HERD/stage01_energy_replot
mkdir -p "$work"

prod04=results/derived/gamma1GeV_v2025a_prod04/stage01/config_v2_counts_pair_leakage.tar
prod05=results/derived/gamma_0p05to20GeV_powerlaw_m1_v2025a_prod05/stage01/config_v3_linear_angle_layout.tar

python scripts/analysis/gamma_calo_features/summarize_energy_response.py \
  "$prod05" "$work/prod05_response.csv" \
  --energy-min 0.05 --energy-max 20 --bins 12

python scripts/analysis/gamma_calo_features/summarize_energy_response.py \
  "$prod04" "$work/prod04_response.csv" \
  --energy-min 0.999 --energy-max 1.001 --bins 1

python scripts/analysis/gamma_calo_features/summarize_energy_response_hist.py \
  "$prod05" "$work/prod05_response_hist.csv" \
  --energy-min 0.05 --energy-max 20 --edep-min 1e-5 --edep-max 25 --bins 54

python scripts/analysis/gamma_calo_features/plot_energy_response_summary.py \
  "$work/prod05_response.csv" "$work/prod04_response.csv" \
  "$work/prod05_response_hist.csv" \
  "$work/02_response_resolution_vs_energy.png"

cp "$work/02_response_resolution_vs_energy.png" \
  figures/_comparison/stage01/energy_response/02_response_resolution_vs_energy.png.partial
mv figures/_comparison/stage01/energy_response/02_response_resolution_vs_energy.png.partial \
  figures/_comparison/stage01/energy_response/02_response_resolution_vs_energy.png
```

图注只在 `plot_energy_response_summary.py` 顶部的 `LABELS = {...}` 修改。尤其是：

- `suptitle`：整张图标题；
- `left_title` / `right_title`：左右面板标题；
- `left_xlabel`、`left_ylabel`、`right_xlabel`、`right_ylabel`：坐标轴；
- `left_note`：左图上方的样本和总事例数文字；
- `right_note`：右图“中位数 / 68% 区间”的说明。

改完后只需重跑本节最后的 `plot_energy_response_summary.py` 命令和两条发布命令；不必重新读取 tar。

## 2. 重画图 03、04：固定 1 GeV 的纵向/横向特征图

```bash
cd /herdfs/user/zhangjin0101/HERD
source /scratchfs2/herd/zhangjin0101/envs/stage01-plot-py311/bin/activate

work=/scratchfs2/herd/zhangjin0101/HERD/stage01_fixed1GeV_replot
mkdir -p "$work"
prod04=results/derived/gamma1GeV_v2025a_prod04/stage01/config_v2_counts_pair_leakage.tar

python scripts/analysis/gamma_calo_features/summarize_fixed_energy_feature_response.py \
  "$prod04" "$work/prod04_fixed_features.json"

python scripts/analysis/gamma_calo_features/plot_fixed_energy_feature_distributions.py \
  "$work/prod04_fixed_features.json" "$work/figures"

cp "$work/figures/03_longitudinal_feature_distributions_1GeV.png" \
  figures/_comparison/stage01/longitudinal_response/03_longitudinal_features_response_residual.png.partial
mv figures/_comparison/stage01/longitudinal_response/03_longitudinal_features_response_residual.png.partial \
  figures/_comparison/stage01/longitudinal_response/03_longitudinal_features_response_residual.png

cp "$work/figures/04_transverse_feature_distributions_1GeV.png" \
  figures/_comparison/stage01/transverse_response/04_transverse_features_response_residual.png.partial
mv figures/_comparison/stage01/transverse_response/04_transverse_features_response_residual.png.partial \
  figures/_comparison/stage01/transverse_response/04_transverse_features_response_residual.png
```

图 03、04 的可编辑文字都集中在 `plot_fixed_energy_feature_distributions.py` 顶部 `LABELS`。分箱方法、曲线归一化与物理含义见 `固定1GeV特征分箱与响应分布说明.md`。

## 常见错误

- `ModuleNotFoundError: matplotlib/numpy`：没有激活 `/scratchfs2/herd/zhangjin0101/envs/stage01-plot-py311`，或环境尚未安装依赖；
- `error: the following arguments are required`：只运行了画图脚本，没有给它所需的 CSV/JSON 输入和输出路径；按本指南整段命令执行；
- HERDFS 写入失败：先输出到 scratch，再以 `.partial` 方式发布；不要覆盖原始 ROOT。
