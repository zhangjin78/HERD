# PyTorch on IHEP/HERDOS servers

## Architecture decision

PyTorch is a training and research choice, not a HERDOS file-format requirement.
The recommended boundary is:

```text
ROOT → validated features/tensors → PyTorch training → ONNX export
     → ONNX Runtime validation → HERDOS C++ inference
```

TensorFlow, JAX, scikit-learn, XGBoost, or ROOT TMVA can replace PyTorch when they
fit the model. The exported ONNX model and its preprocessing contract are the
portable deliverables.

## Environment rules

- Do not install into CVMFS, system Python, or the HERDOS release.
- Do not store a large virtual environment in the small AFS home directory.
- Use a versioned environment under scratch or another approved user area.
- Keep `requirements*.txt`, environment notes, and export scripts in Git.
- Keep datasets, checkpoints, TensorBoard logs, and ONNX files outside Git.
- Confirm the compute-node OS, Python ABI, NVIDIA driver, and CUDA support before
  choosing a GPU wheel.
- Do not run real training on a login node.

## CPU-first smoke environment

Adapt the Python executable to the supported server version:

```bash
python3 -m venv /scratchfs/herd/zhangjin0101/envs/herd-ml-cpu
source /scratchfs/herd/zhangjin0101/envs/herd-ml-cpu/bin/activate
python -m pip install --upgrade pip
python -m pip install -r scripts/environment/requirements-ml.txt
```

The requirements file should pin a tested set of packages. Use the official
PyTorch install selector to choose the CPU or CUDA wheel; do not guess a CUDA
index URL from the login node.

## Verification

```bash
python - <<'PY'
import torch
print("torch:", torch.__version__)
print("cuda build:", torch.version.cuda)
print("cuda available:", torch.cuda.is_available())
if torch.cuda.is_available():
    print("device:", torch.cuda.get_device_name(0))
PY
```

Run a tiny model and a few events as a smoke test. For GPU training, repeat the
CUDA check inside the submitted compute job because the login node may have no
GPU even when the cluster does.

## Reproducibility record

For each trained model record:

- dataset ID and frozen split;
- Git revision;
- Python and package versions;
- random seeds;
- feature normalization computed from training only;
- model configuration and loss masks;
- checkpoint checksum;
- ONNX opset and validation tolerance;
- traditional-baseline and held-out-test metrics.
