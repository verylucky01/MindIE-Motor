#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
# MindIE is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#         http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

set -e

build_type="Release"
version="1.0.0"
while getopts "dv:" opt; do
  case ${opt} in
    d)
      build_type="Debug"
      ;;
    v)
      version=$OPTARG
      ;;
    \?)
      echo "Invalid option: -$OPTARG" >&2
      exit 1
      ;;
  esac
done

readonly CURRENT_PATH=$(cd "$(dirname "$0")"; pwd)
export LD_LIBRARY_PATH=${CURRENT_PATH}/build/open_source/kmc/lib/:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=${CURRENT_PATH}/build/open_source/libboundscheck/lib/:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=${CURRENT_PATH}/build/open_source/openssl/:$LD_LIBRARY_PATH

BUILD_PATH=${CURRENT_PATH}/build
OUTPUT_PATH=${CURRENT_PATH}/output
PACK_PATH=${CURRENT_PATH}/Ascend-MindIE-MS
CPU_TYPE=$(arch)

# 更新子模块：nlohmann/openssl/libboundscheck
cd ${CURRENT_PATH}/open_source/nlohmann && git submodule update --init --recursive
cd ${CURRENT_PATH}/open_source/openssl && git submodule update --init --recursive
cd ${CURRENT_PATH}/open_source/libboundscheck && git submodule update --init --recursive

# 下载KMC和boost
cd ${CURRENT_PATH}/open_source
rm -rf ${CURRENT_PATH}/open_source/kmc
mkdir -p ${CURRENT_PATH}/open_source/kmc
if [ ${CPU_TYPE} == "aarch64" ];then
  wget https://mindie.obs.cn-north-4.myhuaweicloud.com/cert/1.0.T61/MindIE-KMC_aarch64.zip --no-check-certificate
  unzip -o ${CURRENT_PATH}/open_source/MindIE-KMC_aarch64.zip -d ${CURRENT_PATH}/open_source/kmc/
  rm ${CURRENT_PATH}/open_source/MindIE-KMC_aarch64.zip
elif [ ${CPU_TYPE} == "x86_64" ];then
  wget https://mindie.obs.cn-north-4.myhuaweicloud.com/cert/1.0.T61/MindIE-KMC_x86_64.zip --no-check-certificate
  unzip -o ${CURRENT_PATH}/open_source/MindIE-KMC_x86_64.zip -d ${CURRENT_PATH}/open_source/kmc/
  rm ${CURRENT_PATH}/open_source/MindIE-KMC_x86_64.zip
fi

rm -rf ${CURRENT_PATH}/open_source/boost
wget https://boostorg.jfrog.io/artifactory/main/release/1.82.0/source/boost_1_82_0.tar.gz --no-check-certificate
tar -zxf boost_1_82_0.tar.gz
mv ${CURRENT_PATH}/open_source/boost_1_82_0 ${CURRENT_PATH}/open_source/boost
rm ${CURRENT_PATH}/open_source/boost_1_82_0.tar.gz
cd ${CURRENT_PATH}

# build libboundscheck
rm -rf ${BUILD_PATH}
mkdir ${BUILD_PATH}
cp -r ${CURRENT_PATH}/open_source ${BUILD_PATH}/
cd ${BUILD_PATH}/open_source/libboundscheck
make -j

# build openssl
cd ${BUILD_PATH}/open_source/openssl
./config
make -j

# build src
cd ${BUILD_PATH}
cmake -DCMAKE_BUILD_TYPE=$build_type -DCMAKE_INSTALL_PREFIX:PATH=`pwd`/install ..
make install -j8

# 打包
cd ${CURRENT_PATH}
if [ -d "${PACK_PATH}" ];then
  rm -rf ${PACK_PATH}/*
fi
mkdir -p ${PACK_PATH}/bin
mkdir -p ${PACK_PATH}/conf
mkdir -p ${PACK_PATH}/lib

cp "${OUTPUT_PATH}"/bin/* "${PACK_PATH}"/bin/
cp -r "${OUTPUT_PATH}"/config/* "${PACK_PATH}"/conf/
cp "${OUTPUT_PATH}"/lib/* "${PACK_PATH}"/lib/
cp "${BUILD_PATH}"/open_source/openssl/*.so* "${PACK_PATH}"/lib/
cp "${BUILD_PATH}"/open_source/libboundscheck/lib/*.so "${PACK_PATH}"/lib/
cp "${BUILD_PATH}"/open_source/kmc/lib/* "${PACK_PATH}"/lib/

tar -zcvf Ascend-MindIE-MS_${version}_linux-${CPU_TYPE}.tar.gz Ascend-MindIE-MS
mkdir -p ${CURRENT_PATH}/dist
mv Ascend-MindIE-MS_${version}_linux-${CPU_TYPE}.tar.gz ${CURRENT_PATH}/dist/