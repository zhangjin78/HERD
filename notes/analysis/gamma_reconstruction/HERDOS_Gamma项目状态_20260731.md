# HERDOS Gamma 项目状态（2026-07-31）

## 已完成

- 建立 prod04/prod05 数据集 JSON 配置和固定 job-level split；
- prod04：4/4 文件、200,000 事例配置验证通过；
- prod04：统一 Stage 01 管线复现通过，核心统计与旧结果一致；
- prod05：当前 7/10 文件、700,000 事例 partial 管线通过；
- prod05 能谱在等宽 log(E) 分箱中近似均匀，计数相对标准差 0.004484；
- prod05 partial 转换比例 81.002%，无零字节分析产物；
- 修正宽能谱“低能尾”为 `Edep/Etrue < 0.8`；
- 传统基线接口在 partial 数据上测试通过；
- 建立 PyTorch 工程特征多任务模型和 3D-CNN 接口，但未安装环境、未训练；
- 代码、配置和文档已备份到个人 GitHub 功能分支。

## Partial 传统基线诊断

这些数值不是正式物理结果：

- train/validation/test：400,000 / 100,000 / 200,000；
- 原始总沉积平均绝对分箱偏差约 3.77%；
- 前向单调标定平均绝对分箱偏差约 1.94%；
- 解析泄漏修正在 validation 上没有进一步改善，因此自动退回单调标定；
- 原始方法平均 68% 分辨率约 4.65%，单调标定约 4.92%。

正式结果必须等待 prod05 10/10 完成后重新生成。

## 大结果归档

由于 HERDFS 多文件写入会随机卡住，标准发布改为原子 tar：

```text
results/derived/gamma1GeV_v2025a_prod04/
  stage01_config_v1_complete.tar

results/derived/gamma_0p05to20GeV_powerlaw_m1_v2025a_prod05/
  stage01_partial_7of10_config_v1.tar
  stage02_partial_7of10_traditional_v2.tar
```

失败的展开目录保存在 `results/analysis/_archive/`，不得作为正式结果使用。

## 当前阻塞和下一触发条件

- prod05 jobs 1、3、6 仍在运行；
- prod05 达到 10/10 后执行完整验证、SHA256、Stage 01 和正式传统基线；
- 当前服务器 Python 没有 PyTorch；Stage 02 冻结前不安装或训练 ML；
- 3D CNN 训练前还需要生成按事件组织的 CALO cell tensor；
- 方向任务等待独立 0–45° 多角度模拟样本。
