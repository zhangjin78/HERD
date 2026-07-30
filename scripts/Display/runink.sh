#!/bin/bash 


# The public installation supplies zdisplay and the web application.
source /herdfs/user/quzy/public/herdinstall/setup.sh

# Load the local v2025a environment last so HERDOS_INSTALL and DDXMLPATH
# point to the same geometry installation used for the simulation.
source /cvmfs/herd.ihep.ac.cn/HERDOS/el9_amd64_gcc11/Release/v00-10/ExternalLibs/bashrc.sh
source /scratchfs/herd/zhangjin0101/HERDOS/v2025a/install/setup.sh

zport="5010"
xmlfile='/scratchfs/herd/zhangjin0101/HERDOS/v2025a/install/compact/v2025a/v2025a-scdX.xml'
rootfile='/herdfs/user/zhangjin0101/HERD/results/v2025a-test/my_first_gamma.root'
shorthost="`/bin/hostname | /bin/awk -F '.' '{print $1}'`"
if [ $# -eq 4 ]; then
  zport=$1
  xmlfile=$2
  rootfile=$3
  shorthost=$4
fi

# Accept an absolute path, v2025a/v2025a-scdX.xml, or
# compact/v2025a/v2025a-scdX.xml from the web form.
if [[ "$xmlfile" != /* ]]; then
  if [[ -f "${HERDOS_INSTALL}/${xmlfile}" ]]; then
    xmlfile="${HERDOS_INSTALL}/${xmlfile}"
  elif [[ -f "${HERDOS_INSTALL}/compact/${xmlfile}" ]]; then
    xmlfile="${HERDOS_INSTALL}/compact/${xmlfile}"
  fi
fi

if [[ ! -f "$xmlfile" ]]; then
  echo "ERROR: geometry XML does not exist: $xmlfile" >&2
  exit 2
fi

if [[ ! -f "$rootfile" ]]; then
  echo "ERROR: ROOT input does not exist: $rootfile" >&2
  exit 3
fi

zdisplay_cmd="/herdfs/user/zhangjin0101/HERD/scripts/Display/zdisplay_v2025a.py"
if [[ ! -x "$zdisplay_cmd" ]]; then
  echo "ERROR: v2025a-compatible zdisplay is not executable: $zdisplay_cmd" >&2
  exit 4
fi


export PYTHONPATH=/herdfs/user/quzy/public/pythonlib:$PYTHONPATH

zhost="`/bin/hostname | /bin/awk -F '.' '{print $1}'`"
echo "running on  http://${zhost}:${zport}"
#start web service
cd /tmp
rm -rf ./app_${USER}
cp -r /herdfs/user/quzy/public/app ./app_${USER}
cd app_${USER}
echo python display_server.py --flask_port ${zport} --short_domain $shorthost --user_port ${zport} --username $USER
python display_server.py --flask_port ${zport} --short_domain $shorthost --user_port ${zport} --username $USER &
#python display_server.py  ${zport}  &
sleep 1;

# zdisplay is now launched by Flask on file selection from the HTML sidebar.
# To auto-start with a default file, uncomment the following:
# echo zdisplay --input  ${rootfile} --entry 0 --seq 5 --ip 127.0.0.1 --port ${zport} --shorturl "/herddisplay/${shorthost}/${zport}/${USER}"
"$zdisplay_cmd" --input "${rootfile}" --geometry "${xmlfile}" --entry 0 --seq 5 --ip 127.0.0.1 --port "${zport}" --shorturl "/herddisplay/${shorthost}/${zport}/${USER}"
