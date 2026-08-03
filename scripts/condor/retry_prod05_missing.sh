#!/usr/bin/env bash
set -euo pipefail

workspace=/herdfs/user/zhangjin0101/HERD
runner="${workspace}/scripts/condor/run_gamma_powerlaw_subjob.sh"
original_tag=gamma_0p05to20GeV_powerlaw_m1_v2025a_prod05
retry_tag="${original_tag}_retry_20260731"
staging_dir="${workspace}/results/staging/${retry_tag}"
run_dir="${workspace}/runs/condor/${retry_tag}"
log_dir="${run_dir}/logs"

total_events=1000000
events_per_job=100000
base_seed=100000
run_base=100000
energy_min=0.05
energy_max=20
particle=gamma
geometry=v2025a/v2025a-scdX.xml
g4mac=vertical-5x5
spectrum=powerlaw

if [[ ! -x "$runner" ]]; then
  echo "ERROR: runner is missing or not executable: $runner" >&2
  exit 1
fi

if [[ -e "${run_dir}/SUBMITTED" ]]; then
  echo "ERROR: this retry set has already been submitted: ${run_dir}/SUBMITTED" >&2
  exit 2
fi

mkdir -p "$staging_dir" "$log_dir"

{
  echo "purpose=premaintenance_parallel_retry"
  echo "original_cluster=80033137"
  echo "original_procids=1 3 6"
  echo "created=$(date --iso-8601=seconds)"
  echo "submit_host=$(hostname -f)"
  echo "staging_dir=${staging_dir}"
  echo "total_events=${total_events}"
  echo "events_per_job=${events_per_job}"
  echo "base_seed=${base_seed}"
  echo "run_base=${run_base}"
  echo "particle=${particle}"
  echo "energy_min_GeV=${energy_min}"
  echo "energy_max_GeV=${energy_max}"
  echo "spectrum=${spectrum}"
  echo "geometry=${geometry}"
  echo "g4macro=${g4mac}"
  echo "herdos_install=/scratchfs/herd/zhangjin0101/HERDOS/v2025a/install"
  echo "herdos_commit=e93260b"
} > "${run_dir}/MANIFEST.txt"

export PATH=/afs/ihep.ac.cn/soft/common/sysgroup/hep_job/bin:$PATH

for proc_id in 1 3 6; do
  expected_output="${staging_dir}/gamma_0p05to20GeV_powerlaw_m1_vertical_job$(printf '%06d' "$proc_id")_nevt100000_seed$((base_seed + proc_id)).root"
  if [[ -e "$expected_output" ]]; then
    echo "ERROR: retry output already exists: $expected_output" >&2
    exit 3
  fi

  retry_name="${retry_tag}_p${proc_id}"
  submit_output=$(
    hep_sub "$runner" \
      -g herd \
      -p physical \
      -u vanilla \
      -os AlmaLinux9 \
      -mem 1500 \
      -wt mid \
      -name "$retry_name" \
      -o "${log_dir}/proc_${proc_id}.out" \
      -e "${log_dir}/proc_${proc_id}.err" \
      -argu "$proc_id" "$total_events" "$events_per_job" "$base_seed" \
            "$staging_dir" "$energy_min" "$energy_max" "$particle" "$geometry" \
            "$g4mac" "$spectrum" "$retry_tag" "$run_base" \
      -n 1
  )
  printf '%s\n' "$submit_output" | tee "${run_dir}/submission_proc_${proc_id}.txt"
done

date --iso-8601=seconds > "${run_dir}/SUBMITTED"

echo
echo "Parallel retries submitted."
echo "Staging outputs: $staging_dir"
echo "Run records:     $run_dir"
