#! /bin/bash

set -e

# Check necessary env vars exist
if [ -z "${hydra_root_dir}" ]; then
  echo "hydra_root_dir variable not set"
  exit
fi
if [ -z "${HYDRA_SERVER_DEPLOY_JSON}" ]; then
  echo "HYDRA_SERVER_DEPLOY_JSON variable not set"
  exit
fi
if [ -z "${hydra_server_num}" ]; then
  echo "hydra_server_num variable not set"
  exit
fi
if [ -z "${HYDRA_SYSTEM_CONFIG_JSON}" ]; then
  echo "HYDRA_SYSTEM_CONFIG_JSON variable not set"
  exit
fi
if [ -z "${hydra_rru_num}" ]; then
  echo "hydra_rru_num variable not set"
  exit
fi
if [ -z "${hydra_app_num}" ]; then
  echo "hydra_app_num variable not set"
  exit
fi

# Check whether it is bigstation mode
bigstation_mode=$(cat ${HYDRA_SYSTEM_CONFIG_JSON} | jq '.use_bigstation_mode')
use_time_domain_iq=$(cat ${HYDRA_SYSTEM_CONFIG_JSON} | jq '.use_time_domain_iq')

if [ "${bigstation_mode}" == "true" ]; then
  frame_str=$(cat ${HYDRA_SYSTEM_CONFIG_JSON} | jq '.frames[0]')
  if [ ${frame_str} == '"PDDDDDDDDDDDDD"' ]; then
    # Check the length of 
    #   * num_ifft_workers
    #   * num_zf_workers
    #   * num_precode_workers
    #   * num_encode_workers
    # are equal
    ifft_worker_len=$(cat ${HYDRA_SERVER_DEPLOY_JSON} | jq '.num_ifft_workers | length')
    zf_worker_len=$(cat ${HYDRA_SERVER_DEPLOY_JSON} | jq '.num_zf_workers | length')
    precode_worker_len=$(cat ${HYDRA_SERVER_DEPLOY_JSON} | jq '.num_precode_workers | length')
    encode_worker_len=$(cat ${HYDRA_SERVER_DEPLOY_JSON} | jq '.num_encode_workers | length')
    if [ "${hydra_app_num}" -ne "${ifft_worker_len}" ]; then
      echored "ERROR: hydra_servers != num_ifft_workers"
      exit
    fi
    if [ "${hydra_app_num}" -ne "${zf_worker_len}" ]; then
      echored "ERROR: hydra_servers != num_zf_workers"
      exit
    fi
    if [ "${hydra_app_num}" -ne "${precode_worker_len}" ]; then
      echored "ERROR: hydra_servers != num_precode_workers"
      exit
    fi
    if [ "${hydra_app_num}" -ne "${encode_worker_len}" ]; then
      echored "ERROR: hydra_servers != num_encode_workers"
      exit
    fi

    # Check ofdm_data_num is a multiple of 8 and ue_num
    sc_num=$(cat ${HYDRA_SYSTEM_CONFIG_JSON} | jq '.ofdm_data_num')
    ue_num=$(cat ${HYDRA_SYSTEM_CONFIG_JSON} | jq '.ue_num')
    sc_ue_mol=$(( ${sc_num}%${ue_num} ))
    if [ "${sc_ue_mol}" -ne "0" ]; then
      echored "ERROR: ofdm_data_num % ue_num != 0"
      exit
    fi
    sc_ue_mol=$(( ${sc_num}%8 ))
    if [ "${sc_ue_mol}" -ne "0" ]; then
      echored "ERROR: ofdm_data_num % 8 != 0"
      exit
    fi
  else
    # Check the length of 
    #   * num_fft_workers
    #   * num_zf_workers
    #   * num_demul_workers
    #   * num_decode_workers
    # are equal
    fft_worker_len=$(cat ${HYDRA_SERVER_DEPLOY_JSON} | jq '.num_fft_workers | length')
    zf_worker_len=$(cat ${HYDRA_SERVER_DEPLOY_JSON} | jq '.num_zf_workers | length')
    demul_worker_len=$(cat ${HYDRA_SERVER_DEPLOY_JSON} | jq '.num_demul_workers | length')
    decode_worker_len=$(cat ${HYDRA_SERVER_DEPLOY_JSON} | jq '.num_decode_workers | length')
    if [ "${hydra_app_num}" -ne "${fft_worker_len}" ]; then
      echored "ERROR: hydra_servers != num_fft_workers"
      exit
    fi
    if [ "${hydra_app_num}" -ne "${zf_worker_len}" ]; then
      echored "ERROR: hydra_servers != num_zf_workers"
      exit
    fi
    if [ "${hydra_app_num}" -ne "${demul_worker_len}" ]; then
      echored "ERROR: hydra_servers != num_demul_workers"
      exit
    fi
    if [ "${hydra_app_num}" -ne "${decode_worker_len}" ]; then
      echored "ERROR: hydra_servers != num_decode_workers"
      exit
    fi

    # Check ofdm_data_num is a multiple of 8 and ue_num
    sc_num=$(cat ${HYDRA_SYSTEM_CONFIG_JSON} | jq '.ofdm_data_num')
    ue_num=$(cat ${HYDRA_SYSTEM_CONFIG_JSON} | jq '.ue_num')
    sc_ue_mol=$(( ${sc_num}%${ue_num} ))
    if [ "${sc_ue_mol}" -ne "0" ]; then
      echored "ERROR: ofdm_data_num % ue_num != 0"
      exit
    fi
    sc_ue_mol=$(( ${sc_num}%8 ))
    if [ "${sc_ue_mol}" -ne "0" ]; then
      echored "ERROR: ofdm_data_num % 8 != 0"
      exit
    fi
  fi
elif [ "${use_time_domain_iq}" == "true" ]; then
  # Check the length of 
  #  * subcarrier_num_list
  #  * subcarrier_block_list
  #  * coding_thread_num
  # are equal 
  sc_num_list_len=$(cat ${HYDRA_SERVER_DEPLOY_JSON} | jq '.subcarrier_num_list | length')
  sc_block_list_len=$(cat ${HYDRA_SERVER_DEPLOY_JSON} | jq '.subcarrier_block_list | length')
  dc_thread_list_len=$(cat ${HYDRA_SERVER_DEPLOY_JSON} | jq '.coding_thread_num | length')
  fft_thread_list_len=$(cat ${HYDRA_SERVER_DEPLOY_JSON} | jq '.fft_thread_num | length')
  if [ "${hydra_app_num}" -ne "${sc_num_list_len}" ]; then
    echored "ERROR: hydra_servers != subcarrier_num_list"
    exit
  fi
  if [ "${hydra_app_num}" -ne "${sc_block_list_len}" ]; then
    echored "ERROR: hydra_servers != subcarrier_block_list"
    exit
  fi
  if [ "${hydra_app_num}" -ne "${dc_thread_list_len}" ]; then
    echored "ERROR: hydra_servers != coding_thread_num"
    exit
  fi
  if [ "${hydra_app_num}" -ne "${fft_thread_list_len}" ]; then
    echored "ERROR: hydra_servers != fft_thread_num"
    exit
  fi

  # Check ofdm_data_num is a multiple of 8 and ue_num
  sc_num=$(cat ${HYDRA_SYSTEM_CONFIG_JSON} | jq '.ofdm_data_num')
  ue_num=$(cat ${HYDRA_SYSTEM_CONFIG_JSON} | jq '.ue_num')
  sc_ue_mol=$(( ${sc_num}%${ue_num} ))
  if [ "${sc_ue_mol}" -ne "0" ]; then
    echored "ERROR: ofdm_data_num % ue_num != 0"
    exit
  fi
  sc_ue_mol=$(( ${sc_num}%8 ))
  if [ "${sc_ue_mol}" -ne "0" ]; then
    echored "ERROR: ofdm_data_num % 8 != 0"
    exit
  fi

  # Check subcarrier_num_list
  # * The sum of the list should be equal to ofdm_data_num
  # * Each item in this list should be a multiple of ue_num and 8
  sc_num_check=0
  for (( i=0; i<${hydra_app_num}; i++ )) do
    sc_num_cur=$(cat ${HYDRA_SERVER_DEPLOY_JSON} | jq --argjson i $i '.subcarrier_num_list[$i]')
    sc_ue_mol=$(( ${sc_num_cur}%${ue_num} ))
    if [ "${sc_ue_mol}" -ne "0" ]; then
      echored "ERROR: subcarrier_num_list[$i] % ue_num != 0"
      exit
    fi
    sc_ue_mol=$(( ${sc_num_cur}%8 ))
    if [ "${sc_ue_mol}" -ne "0" ]; then
      echored "ERROR: subcarrier_num_list[$i] % 8 != 0"
      exit
    fi
    sc_num_check=$(( ${sc_num_check}+${sc_num_cur} ))
  done
  if [ "${sc_num}" -ne "${sc_num_check}" ]; then
    echored "ERROR: the sum of subcarrier_num_list != ofdm_data_num"
    exit
  fi

  # Check subcarrier_block_list
  # * Each item in this list should be a multiple of 8
  # * Each item should be larger than 0
  for (( i=0; i<${hydra_app_num}; i++ )) do
    sc_block_cur=$(cat ${HYDRA_SERVER_DEPLOY_JSON} | jq --argjson i $i '.subcarrier_block_list[$i]')
    # sc_ue_mol=$(( ${sc_block_cur}%8 ))
    # if [ "${sc_ue_mol}" -ne "0" ]; then
    #   echored "ERROR: subcarrier_block_list[$i] % 8 != 0"
    #   exit
    # fi
    if [ "${sc_block_cur}" == "0" ]; then
      echored "ERROR: subcarrier_block_list[$i] == 0"
      exit
    fi
  done
else
  # Check the length of 
  #  * subcarrier_num_list
  #  * subcarrier_block_list
  #  * coding_thread_num
  # are equal 
  sc_num_list_len=$(cat ${HYDRA_SERVER_DEPLOY_JSON} | jq '.subcarrier_num_list | length')
  sc_block_list_len=$(cat ${HYDRA_SERVER_DEPLOY_JSON} | jq '.subcarrier_block_list | length')
  dc_thread_list_len=$(cat ${HYDRA_SERVER_DEPLOY_JSON} | jq '.coding_thread_num | length')
  if [ "${hydra_app_num}" -ne "${sc_num_list_len}" ]; then
    echored "ERROR: hydra_servers != subcarrier_num_list"
    exit
  fi
  if [ "${hydra_app_num}" -ne "${sc_block_list_len}" ]; then
    echored "ERROR: hydra_servers != subcarrier_block_list"
    exit
  fi
  if [ "${hydra_app_num}" -ne "${dc_thread_list_len}" ]; then
    echored "ERROR: hydra_servers != coding_thread_num"
    exit
  fi

  # Check ofdm_data_num is a multiple of 8 and ue_num
  sc_num=$(cat ${HYDRA_SYSTEM_CONFIG_JSON} | jq '.ofdm_data_num')
  ue_num=$(cat ${HYDRA_SYSTEM_CONFIG_JSON} | jq '.ue_num')
  sc_ue_mol=$(( ${sc_num}%${ue_num} ))
  if [ "${sc_ue_mol}" -ne "0" ]; then
    echored "ERROR: ofdm_data_num % ue_num != 0"
    exit
  fi
  sc_ue_mol=$(( ${sc_num}%8 ))
  if [ "${sc_ue_mol}" -ne "0" ]; then
    echored "ERROR: ofdm_data_num % 8 != 0"
    exit
  fi

  # Check subcarrier_num_list
  # * The sum of the list should be equal to ofdm_data_num
  # * Each item in this list should be a multiple of ue_num and 8
  sc_num_check=0
  for (( i=0; i<${hydra_app_num}; i++ )) do
    sc_num_cur=$(cat ${HYDRA_SERVER_DEPLOY_JSON} | jq --argjson i $i '.subcarrier_num_list[$i]')
    sc_ue_mol=$(( ${sc_num_cur}%${ue_num} ))
    if [ "${sc_ue_mol}" -ne "0" ]; then
      echored "ERROR: subcarrier_num_list[$i] % ue_num != 0"
      exit
    fi
    sc_ue_mol=$(( ${sc_num_cur}%8 ))
    if [ "${sc_ue_mol}" -ne "0" ]; then
      echored "ERROR: subcarrier_num_list[$i] % 8 != 0"
      exit
    fi
    sc_num_check=$(( ${sc_num_check}+${sc_num_cur} ))
  done
  if [ "${sc_num}" -ne "${sc_num_check}" ]; then
    echored "ERROR: the sum of subcarrier_num_list != ofdm_data_num"
    exit
  fi

  # Check subcarrier_block_list
  # * Each item in this list should be a multiple of 8
  # * Each item should be larger than 0
  for (( i=0; i<${hydra_app_num}; i++ )) do
    sc_block_cur=$(cat ${HYDRA_SERVER_DEPLOY_JSON} | jq --argjson i $i '.subcarrier_block_list[$i]')
    # sc_ue_mol=$(( ${sc_block_cur}%8 ))
    # if [ "${sc_ue_mol}" -ne "0" ]; then
    #   echored "ERROR: subcarrier_block_list[$i] % 8 != 0"
    #   exit
    # fi
    if [ "${sc_block_cur}" == "0" ]; then
      echored "ERROR: subcarrier_block_list[$i] == 0"
      exit
    fi
  done
fi