#! /bin/bash

set -e

# Find the root directory of Hydra app
script_dir=$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )
hydra_root_dir=$( cd ${script_dir}/../.. >/dev/null 2>&1 && pwd )

source ${hydra_root_dir}/scripts/utils/utils.sh

# Set the root directory of Hydra remote runner
HYDRA_RUNNER_ROOT="~/HydraRemoteRunner"

# Read HYDRA_RUNNER_ROOT from file config/config.json
hydra_master_config_json=${hydra_root_dir}/config/config.json
if [ ! -f ${hydra_master_config_json} ]; then
  echored "ERROR: config file ${hydra_master_config_json} does not exist"
fi
res=$(cat ${hydra_master_config_json} | jq '.hydra_runner_root' | tr -d '"')
if [ "${res}" != "null" ]; then
  HYDRA_RUNNER_ROOT=${res}
fi

eval "source ${HYDRA_RUNNER_ROOT}/intel/oneapi/setvars.sh"
eval "export LIBRARY_PATH=${LIBRARY_PATH}:${HYDRA_RUNNER_ROOT}/rdma-core/build/lib"
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${LIBRARY_PATH}
eval "export RTE_SDK=${HYDRA_RUNNER_ROOT}/dpdk-stable-20.11.3"
