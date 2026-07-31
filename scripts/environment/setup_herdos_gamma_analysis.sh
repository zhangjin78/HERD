#!/usr/bin/env bash
set -euo pipefail

source /cvmfs/herd.ihep.ac.cn/HERDOS/el9_amd64_gcc11/Release/v00-10/ExternalLibs/bashrc.sh
source /scratchfs/herd/zhangjin0101/HERDOS/v2025a/install/setup.sh

if [[ "${HERDOS_INSTALL:-}" != "/scratchfs/herd/zhangjin0101/HERDOS/v2025a/install" ]]; then
  echo "ERROR: unexpected HERDOS_INSTALL=${HERDOS_INSTALL:-unset}" >&2
  return 1 2>/dev/null || exit 1
fi

command -v root >/dev/null
python3 -c 'import ROOT; print("PyROOT", ROOT.gROOT.GetVersion())'
