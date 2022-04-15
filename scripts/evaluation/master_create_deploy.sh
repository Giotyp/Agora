#! /bin/bash

set -e

script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
hydra_root_dir=$( cd ${script_dir}/../.. >/dev/null 2>&1 && pwd )

source ${hydra_root_dir}/scripts/utils/utils.sh
source ${hydra_root_dir}/scripts/control/init_platform.sh

ant_list=(150)
ue_list=(32)

# for i in ${!ant_list[@]}; do
#     ant_num=${ant_list[$i]}
#     ue_num=${ue_list[$i]}
#     for (( c=60; c>=20; c-=10 )) do
#         python ${hydra_root_dir}/scripts/evaluation/create_deploy.py ${c} \
#             ${hydra_root_dir}/config/deploy_cloudlab/deploy_cloudlab_hydra_${ant_num}_${ue_num}_dl.json \
#             ${hydra_root_dir}/config/template_cloudlab/template_cloudlab_hydra_${ant_num}_${ue_num}_dl.json
#     done
# done

for i in ${!ant_list[@]}; do
    ant_num=${ant_list[$i]}
    ue_num=${ue_list[$i]}
    for (( c=80; c>=20; c-=10 )) do
        python ${hydra_root_dir}/scripts/evaluation/create_deploy.py ${c} \
            ${hydra_root_dir}/config/deploy_msr/deploy_msr_${ant_num}_${ue_num}.json \
            ${hydra_root_dir}/config/template_msr/template_msr_${ant_num}_${ue_num}.json
    done
done