#!/usr/bin/env bash

workspace=/herdfs/user/zhangjin0101/HERD
original_tag=gamma_0p05to20GeV_powerlaw_m1_v2025a_prod05
retry_tag="${original_tag}_retry_20260731"
production_dir="${workspace}/results/production/${original_tag}"
staging_dir="${workspace}/results/staging/${retry_tag}"
run_dir="${workspace}/runs/condor/${retry_tag}"

source /cvmfs/herd.ihep.ac.cn/HERDOS/el9_amd64_gcc11/Release/v00-10/ExternalLibs/bashrc.sh >/dev/null 2>&1
set -euo pipefail
export PATH=/afs/ihep.ac.cn/soft/common/sysgroup/hep_job/bin:$PATH

# Keep the publication destination present while compute-node jobs are running.
# An empty staging directory is intentional and must never be removed early.
mkdir -p "$staging_dir"

declare -A old_job=(
  [1]=80033137.1
  [3]=80033137.3
  [6]=80033137.6
)

declare -A retry_job=(
  [1]=80060397.0
  [3]=80060443.0
  [6]=80060444.0
)

declare -A retry_log_name=(
  [1]=proc_1.out
  [3]=proc_3_retry2.out
  [6]=proc_6_retry2.out
)

promoted=0
waiting=0

for proc_id in 1 3 6; do
  seed=$((100000 + proc_id))
  filename="gamma_0p05to20GeV_powerlaw_m1_vertical_job$(printf '%06d' "$proc_id")_nevt100000_seed${seed}.root"
  staged="${staging_dir}/${filename}"
  final="${production_dir}/${filename}"
  retry_log="${run_dir}/logs/${retry_log_name[$proc_id]}"

  echo "=== ProcId ${proc_id} ==="

  if [[ -s "$final" ]]; then
    if rootls "$final" >/dev/null 2>&1; then
      echo "Production result already exists and is readable: $final"
      if hep_q -i "${retry_job[$proc_id]}" 2>/dev/null | grep -q "${retry_job[$proc_id]}"; then
        echo "Removing redundant retry ${retry_job[$proc_id]}"
        hep_rm "${retry_job[$proc_id]}"
      fi
      if [[ -s "$staged" ]] && rootls "$staged" >/dev/null 2>&1; then
        echo "Removing redundant validated staged result: $staged"
        rm -- "$staged"
      fi
      waiting=$((waiting + 1))
      continue
    fi
    echo "ERROR: production result exists but ROOT validation failed: $final" >&2
    exit 10
  fi

  if [[ ! -s "$staged" ]]; then
    echo "Retry result is not complete yet."
    waiting=$((waiting + 1))
    continue
  fi

  if [[ ! -f "$retry_log" ]] || ! grep -q '^SUBJOB_SUCCESS$' "$retry_log"; then
    echo "Retry file exists but success marker is missing; leaving everything untouched."
    waiting=$((waiting + 1))
    continue
  fi

  if ! rootls "$staged" >/dev/null 2>&1; then
    echo "ERROR: staged ROOT validation failed: $staged" >&2
    exit 11
  fi

  echo "Retry result passed success-marker and ROOT checks."
  if hep_q -i "${old_job[$proc_id]}" 2>/dev/null | grep -q "${old_job[$proc_id]}"; then
    echo "Removing superseded original job ${old_job[$proc_id]}"
    hep_rm "${old_job[$proc_id]}"
  else
    echo "Original job ${old_job[$proc_id]} is no longer in the queue."
  fi

  mv "$staged" "$final"
  sha256sum "$final" > "${run_dir}/promoted_proc_${proc_id}.sha256"
  echo "Promoted retry result to: $final"
  promoted=$((promoted + 1))
done

echo
echo "promoted=${promoted}"
echo "waiting_or_already_complete=${waiting}"
echo "production_root_count=$(find "$production_dir" -maxdepth 1 -type f -name '*.root' | wc -l)"
