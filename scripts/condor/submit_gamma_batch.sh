#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
runner="${script_dir}/run_gamma_subjob.sh"

total_events=
events_per_job=
tag=
mode=test
energy_min=1
energy_max=1
particle=gamma
geometry=v2025a/v2025a-scdX.xml
g4mac=vertical-5x5
base_seed=100000
run_base=100000
memory_mb=1500
walltime=short
max_jobs=1000
do_submit=0

usage() {
  cat <<'EOF'
Usage:
  submit_gamma_batch.sh --total-events N --events-per-job N --tag TAG [options]

Required:
  --total-events N        Total number of events across all subjobs.
  --events-per-job N      Maximum events in one subjob.
  --tag TAG               Stable batch name using letters, digits, dot, dash or underscore.

Options:
  --mode test|production  Output under results/tests/batch or results/production.
                          Default: test
  --energy E              Mono energy in GeV. Default: 1
  --energy-range A B      Energy range in GeV.
  --particle NAME         Geant4 particle. Default: gamma
  --geometry PATH         Geometry relative to compact/. Default: v2025a/v2025a-scdX.xml
  --g4mac NAME            G4 macro short name. Default: vertical-5x5
  --base-seed N           Seed for ProcId 0; later jobs use N+ProcId. Default: 100000
  --run-base N            Run ID for ProcId 0. Default: 100000
  --memory-mb N           Requested memory. Default: 1500
  --walltime CLASS        test|short|mid|long|special. Default: short
  --max-jobs N            Safety ceiling. Default: 1000
  --submit                Actually call hep_sub. Without this flag: dry-run only.
  -h, --help              Show this help.

Examples:
  # Preview 1,000,000 events split into 100 jobs of 10,000:
  submit_gamma_batch.sh --total-events 1000000 --events-per-job 10000 \
    --tag gamma1GeV_v2025a_trial

  # Submit after checking the preview:
  submit_gamma_batch.sh --total-events 1000000 --events-per-job 10000 \
    --tag gamma1GeV_v2025a_prod01 --mode production --submit
EOF
}

while (($#)); do
  case "$1" in
    --total-events) total_events=$2; shift 2 ;;
    --events-per-job) events_per_job=$2; shift 2 ;;
    --tag) tag=$2; shift 2 ;;
    --mode) mode=$2; shift 2 ;;
    --energy) energy_min=$2; energy_max=$2; shift 2 ;;
    --energy-range) energy_min=$2; energy_max=$3; shift 3 ;;
    --particle) particle=$2; shift 2 ;;
    --geometry) geometry=$2; shift 2 ;;
    --g4mac) g4mac=$2; shift 2 ;;
    --base-seed) base_seed=$2; shift 2 ;;
    --run-base) run_base=$2; shift 2 ;;
    --memory-mb) memory_mb=$2; shift 2 ;;
    --walltime) walltime=$2; shift 2 ;;
    --max-jobs) max_jobs=$2; shift 2 ;;
    --submit) do_submit=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "ERROR: unknown argument: $1" >&2; usage >&2; exit 64 ;;
  esac
done

for required_name in total_events events_per_job tag; do
  if [[ -z "${!required_name}" ]]; then
    echo "ERROR: --${required_name//_/-} is required." >&2
    exit 65
  fi
done

for value_name in total_events events_per_job base_seed run_base memory_mb max_jobs; do
  value=${!value_name}
  if [[ ! "$value" =~ ^[0-9]+$ ]] || (( value == 0 )); then
    echo "ERROR: ${value_name} must be a positive integer: ${value}" >&2
    exit 66
  fi
done

if [[ ! "$tag" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ ]]; then
  echo "ERROR: invalid tag: ${tag}" >&2
  exit 67
fi

if [[ "$mode" != test && "$mode" != production ]]; then
  echo "ERROR: --mode must be test or production." >&2
  exit 68
fi

number_pattern='^[0-9]+([.][0-9]+)?$'
if [[ ! "$energy_min" =~ $number_pattern || ! "$energy_max" =~ $number_pattern ]]; then
  echo "ERROR: energy values must be non-negative decimal numbers." >&2
  exit 69
fi
if ! awk -v lo="$energy_min" -v hi="$energy_max" 'BEGIN { exit !(lo <= hi) }'; then
  echo "ERROR: energy range lower bound exceeds upper bound." >&2
  exit 69
fi

case "$walltime" in
  test|short|mid|long|special) ;;
  *) echo "ERROR: invalid walltime class: ${walltime}" >&2; exit 70 ;;
esac

if [[ ! -x "$runner" ]]; then
  echo "ERROR: runner is missing or not executable: ${runner}" >&2
  exit 71
fi

num_jobs=$(((total_events + events_per_job - 1) / events_per_job))
last_events=$((total_events - (num_jobs - 1) * events_per_job))
if (( num_jobs > max_jobs )); then
  echo "ERROR: ${num_jobs} jobs exceeds safety limit ${max_jobs}." >&2
  echo "Use a larger --events-per-job or explicitly raise --max-jobs." >&2
  exit 72
fi

workspace=/herdfs/user/zhangjin0101/HERD
if [[ "$mode" == test ]]; then
  output_dir="${workspace}/results/tests/batch/${tag}"
else
  output_dir="${workspace}/results/production/${tag}"
fi
run_dir="${workspace}/runs/condor/${tag}"
log_dir="${run_dir}/logs"
manifest="${run_dir}/MANIFEST.txt"

if [[ -d "$output_dir" ]] && find "$output_dir" -maxdepth 1 -name '*.root' -print -quit | grep -q .; then
  echo "ERROR: ROOT files already exist in ${output_dir}; choose a new tag." >&2
  exit 73
fi
if [[ -e "$manifest" ]]; then
  echo "ERROR: batch manifest already exists: ${manifest}; choose a new tag." >&2
  exit 74
fi

export PATH=/afs/ihep.ac.cn/soft/common/sysgroup/hep_job/bin:$PATH

submit_cmd=(
  hep_sub "$runner"
  -g herd
  -p physical
  -u vanilla
  -os AlmaLinux9
  -mem "$memory_mb"
  -wt "$walltime"
  -name "$tag"
  -o "${log_dir}/job_%{ProcId}.out"
  -e "${log_dir}/job_%{ProcId}.err"
  -argu "%{ProcId}" "$total_events" "$events_per_job" "$base_seed"
        "$output_dir" "$energy_min" "$energy_max" "$particle" "$geometry"
        "$g4mac" "$tag" "$run_base"
  -n "$num_jobs"
)

echo "Batch plan"
echo "  mode=${mode}"
echo "  tag=${tag}"
echo "  total_events=${total_events}"
echo "  events_per_job=${events_per_job}"
echo "  num_jobs=${num_jobs}"
echo "  last_job_events=${last_events}"
echo "  energy_GeV=${energy_min},${energy_max}"
echo "  geometry=${geometry}"
echo "  output_dir=${output_dir}"
echo "  log_dir=${log_dir}"
echo "  memory_mb=${memory_mb}"
echo "  walltime=${walltime}"
printf '  command='; printf ' %q' "${submit_cmd[@]}"; printf '\n'

if (( ! do_submit )); then
  echo "DRY_RUN: nothing submitted. Add --submit after checking this plan."
  exit 0
fi

if ! command -v hep_sub >/dev/null 2>&1; then
  echo "ERROR: hep_sub is not available." >&2
  exit 75
fi

mkdir -p "$output_dir" "$log_dir"
{
  printf 'purpose=%s\n' "$mode"
  printf 'tag=%s\n' "$tag"
  printf 'created=%s\n' "$(date --iso-8601=seconds)"
  printf 'submit_host=%s\n' "$(hostname -f)"
  printf 'total_events=%s\n' "$total_events"
  printf 'events_per_job=%s\n' "$events_per_job"
  printf 'num_jobs=%s\n' "$num_jobs"
  printf 'last_job_events=%s\n' "$last_events"
  printf 'base_seed=%s\n' "$base_seed"
  printf 'run_base=%s\n' "$run_base"
  printf 'particle=%s\n' "$particle"
  printf 'energy_min_GeV=%s\n' "$energy_min"
  printf 'energy_max_GeV=%s\n' "$energy_max"
  printf 'geometry=%s\n' "$geometry"
  printf 'g4macro=%s\n' "$g4mac"
  printf 'memory_mb=%s\n' "$memory_mb"
  printf 'walltime=%s\n' "$walltime"
  printf '%s\n' 'herdos_install=/scratchfs/herd/zhangjin0101/HERDOS/v2025a/install'
  printf '%s\n' 'herdos_branch=zhangjin/gamma-conversion-truth'
  printf '%s\n' 'herdos_commit=e93260b'
  printf '%s\n' 'truth_scope=primary_gamma_first_conversion_or_unconverted_final_state'
} > "$manifest"

echo "Submitting ${num_jobs} jobs to HERD HTCondor..."
"${submit_cmd[@]}" | tee "${run_dir}/submission.txt"
echo "Submission record: ${run_dir}/submission.txt"
echo "Check jobs with: hep_q -u"
