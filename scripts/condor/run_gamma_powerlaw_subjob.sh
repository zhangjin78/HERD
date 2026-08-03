#!/usr/bin/env bash
set -eo pipefail

if [[ $# -ne 13 ]]; then
  echo "Usage: $0 PROC_ID TOTAL_EVENTS EVENTS_PER_JOB BASE_SEED OUTPUT_DIR ENERGY_MIN ENERGY_MAX PARTICLE GEOMETRY G4MAC SPECTRUM TAG RUN_BASE" >&2
  exit 64
fi

proc_id=$1
total_events=$2
events_per_job=$3
base_seed=$4
output_dir=$5
energy_min=$6
energy_max=$7
particle=$8
geometry=$9
g4mac=${10}
spectrum=${11}
tag=${12}
run_base=${13}

for value_name in proc_id total_events events_per_job base_seed run_base; do
  value=${!value_name}
  if [[ ! "$value" =~ ^[0-9]+$ ]]; then
    echo "ERROR: ${value_name} must be a non-negative integer: ${value}" >&2
    exit 65
  fi
done

start_event=$((proc_id * events_per_job))
if (( start_event >= total_events )); then
  echo "ERROR: ProcId ${proc_id} starts beyond total event count ${total_events}." >&2
  exit 66
fi

remaining=$((total_events - start_event))
nevents=$events_per_job
if (( remaining < events_per_job )); then
  nevents=$remaining
fi

seed=$((base_seed + proc_id))
run_id=$((run_base + proc_id))
if (( seed > 2147483647 || run_id > 2147483647 )); then
  echo "ERROR: seed or run ID exceeds signed 32-bit range." >&2
  exit 67
fi

source /cvmfs/herd.ihep.ac.cn/HERDOS/el9_amd64_gcc11/Release/v00-10/ExternalLibs/bashrc.sh
source /scratchfs/herd/zhangjin0101/HERDOS/v2025a/install/setup.sh
set -u

case "$spectrum" in
  mono|powerlaw) ;;
  *) echo "ERROR: unsupported spectrum: ${spectrum}" >&2; exit 68 ;;
esac
if [[ "$spectrum" == mono && "$energy_min" != "$energy_max" ]]; then
  echo "ERROR: mono spectrum requires equal energy bounds." >&2
  exit 68
fi
if [[ "$spectrum" == powerlaw && "$energy_min" == "$energy_max" ]]; then
  echo "ERROR: powerlaw spectrum requires an energy range." >&2
  exit 68
fi

if [[ "$HERDOS_INSTALL" != "/scratchfs/herd/zhangjin0101/HERDOS/v2025a/install" ]]; then
  echo "ERROR: unexpected HERDOS_INSTALL=${HERDOS_INSTALL}" >&2
  exit 68
fi

mkdir -p "$output_dir"
printf -v job_index "%06d" "$proc_id"

energy_label=${energy_min//./p}GeV
if [[ "$energy_min" != "$energy_max" ]]; then
  energy_label=${energy_min//./p}to${energy_max//./p}GeV
fi
spectrum_label=$spectrum
if [[ "$spectrum" == powerlaw ]]; then
  spectrum_label=powerlaw_m1
fi

output_name="${particle}_${energy_label}_${spectrum_label}_vertical_job${job_index}_nevt${nevents}_seed${seed}.root"
final_output="${output_dir}/${output_name}"

if [[ -e "$final_output" ]]; then
  echo "ERROR: output already exists; refusing to overwrite: ${final_output}" >&2
  exit 69
fi

job_tmp_base=${TMPDIR:-/tmp}
job_tmp="${job_tmp_base}/${USER}/herdos_${tag}_${job_index}_$$"
mkdir -p "$job_tmp"
trap 'rm -rf -- "$job_tmp"' EXIT
temporary_output="${job_tmp}/${output_name}.partial"

echo "HERDOS gamma simulation subjob"
echo "  host=$(hostname -f)"
echo "  condor_job_id=${_CONDOR_IHEP_JOB_ID:-local}"
echo "  proc_id=${proc_id}"
echo "  event_range=[${start_event},$((start_event + nevents)))"
echo "  nevents=${nevents}"
echo "  seed=${seed}"
echo "  run_id=${run_id}"
echo "  geometry=${geometry}"
echo "  particle=${particle}"
echo "  energy_GeV=${energy_min},${energy_max}"
echo "  spectrum=${spectrum}"
echo "  final_output=${final_output}"
echo "  start_time=$(date --iso-8601=seconds)"

energy_args=(--energy "$energy_min")
if [[ "$energy_min" != "$energy_max" ]]; then
  energy_args+=( "$energy_max" )
fi
g4mac_args=(--g4mac "$g4mac")
if [[ "$spectrum" == powerlaw ]]; then
  g4mac_args+=(powerlaw)
fi

/usr/bin/time -v python3 "$HERDOS_INSTALL/scripts/SimConfiger/devrun.py" \
  --particle "$particle" \
  "${energy_args[@]}" \
  --geometry "$geometry" \
  "${g4mac_args[@]}" \
  --seed "$seed" \
  --runid "$run_id" \
  --info "batch=${tag};proc=${proc_id};first_gamma_conversion_or_unconverted_final" \
  -N "$nevents" \
  -o "$temporary_output"

if [[ ! -s "$temporary_output" ]]; then
  echo "ERROR: simulation did not produce a non-empty ROOT file." >&2
  exit 70
fi

mv "$temporary_output" "$final_output"
echo "  output_bytes=$(stat -c %s "$final_output")"
echo "  end_time=$(date --iso-8601=seconds)"
echo "SUBJOB_SUCCESS"
