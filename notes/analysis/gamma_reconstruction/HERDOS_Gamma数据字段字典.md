# HERDOS Gamma 阶段一字段字典

## 标识与划分

| 字段 | 单位 | 说明 |
|---|---|---|
| `event` | — | 当前组合数据中的全局事件序号 |
| `job_id` | — | 生产子作业编号，从文件名 `jobXXXXXX` 解析 |
| `split` | — | 0=train、1=validation、2=test、−1=未配置 |

划分以生产子作业为单位，禁止事件级随机混切。

## MC 真值

| 字段 | 单位 | 说明 |
|---|---|---|
| `true_energy_GeV` | GeV | 初级 gamma 真能量 |
| `converted` | bool | 是否记录到初级 gamma 的首次 pair conversion |
| `unconverted_final` | bool | 是否保存未转换初级 gamma 的最终状态 |
| `conversion_[xyz]_cm` | cm | 首次 pair conversion 顶点 |
| `pair_energy_share` | — | `min(E-,E+)/(E-+E+)` |
| `pair_opening_deg` | degree | 首次正负电子张角 |

## CALO 事件特征

| 字段 | 单位 | 说明 |
|---|---|---|
| `calo_edep_GeV` | GeV | 所有 CALO hit 沉积能量和 |
| `n_cells` | cell | 非零沉积晶体数 |
| `n_cells_gt_1MeV` | cell | 沉积超过 1 MeV 的晶体数 |
| `n_cells_gt_20MeV` | cell | 沉积超过 20 MeV 的晶体数 |
| `centroid_[ix,iy,iz]` | cell/layer index | 能量加权簇射重心 |
| `transverse_rms_cells` | cell index | 横向 RMS |
| `longitudinal_rms_layers` | layer index | 纵向 RMS |
| `max_cell_fraction` | — | 最高能晶体占总沉积的比例 |
| `boundary_distance_cells` | cell index | 重心到 x/y 最近边界的索引距离 |
| `last_layer_fraction` | — | 最后一层能量占总沉积的比例 |
| `layer_edep_0_GeV` … `layer_edep_20_GeV` | GeV | 各 `iz` 层沉积能量 |

`ix/iy/iz` 是晶体索引，不是厘米。当前配置按 0–20 定义 CALO 索引边界；
正式使用边界变量前必须与 v2025a 几何映射复核。

## STK 预留特征

| 字段 | 单位 | 说明 |
|---|---|---|
| `n_stk_hits` | hit | STK hit 数 |
| `stk_edep_GeV` | GeV | STK hit 沉积能量和 |

STK 字段只用于后续 CALO+STK 对照。若该 HERDOS 输出版本中 `stkhits.edep`
恒为零，应使用 hit 位置和轨迹信息重新定义 STK 输入，不能把零值当成有效能量。
