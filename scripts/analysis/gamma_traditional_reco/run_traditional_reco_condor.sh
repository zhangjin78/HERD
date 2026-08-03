#!/usr/bin/env bash
# HERDOS setup.sh itself references optional environment variables before it
# initializes them; enable nounset only after sourcing the release setup.
set -eo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 DATASET_CONFIG TAG" >&2
  exit 64
fi

config=$1
tag=$2
workspace=/herdfs/user/zhangjin0101/HERD

source /cvmfs/herd.ihep.ac.cn/HERDOS/el9_amd64_gcc11/Release/v00-10/ExternalLibs/bashrc.sh
source /cvmfs/herd.ihep.ac.cn/HERDOS/el9_amd64_gcc11/Release/v00-10/setup.sh
set -u
cd "$workspace"
exec scripts/analysis/gamma_traditional_reco/run_traditional_reco.py \
  "$config" --tag "$tag"
