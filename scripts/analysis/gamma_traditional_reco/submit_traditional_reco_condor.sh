#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 DATASET_CONFIG TAG" >&2
  exit 64
fi

config=$1
tag=$2
workspace=/herdfs/user/zhangjin0101/HERD
runner="$workspace/scripts/analysis/gamma_traditional_reco/run_traditional_reco_condor.sh"
run_dir="$workspace/runs/condor/$tag"
log_dir="$run_dir/logs"

[[ -f "$workspace/$config" ]] || { echo "missing config: $config" >&2; exit 65; }
[[ -x "$runner" ]] || { echo "missing runner: $runner" >&2; exit 66; }
[[ "$tag" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ ]] || { echo "invalid tag" >&2; exit 67; }
[[ ! -e "$run_dir/submission.txt" ]] || { echo "submission record exists: $run_dir/submission.txt" >&2; exit 68; }

export PATH=/afs/ihep.ac.cn/soft/common/sysgroup/hep_job/bin:$PATH
mkdir -p "$log_dir"
command -v hep_sub >/dev/null

command=(hep_sub "$runner" -g herd -p physical -u vanilla -os AlmaLinux9
  -mem 3000 -wt mid -name "$tag"
  -o "$log_dir/job_%{ProcId}.out" -e "$log_dir/job_%{ProcId}.err"
  -argu "$config" "$tag" -n 1)

printf 'command=' > "$run_dir/submission.txt"
printf ' %q' "${command[@]}" >> "$run_dir/submission.txt"
printf '\nsubmitted_at=%s\n' "$(date --iso-8601=seconds)" >> "$run_dir/submission.txt"
"${command[@]}" | tee -a "$run_dir/submission.txt"
