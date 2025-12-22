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

[ -z "$GLOBAL_ABI_VERSION" ] && GLOBAL_ABI_VERSION="0"
export GLOBAL_ABI_VERSION
echo "GLOBAL_ABI_VERSION: ${GLOBAL_ABI_VERSION}"

PROJECT_ROOT_DIR=$(dirname "$(dirname "$(dirname "$(realpath "$0")")")")
MODULES_DIR="${PROJECT_ROOT_DIR}"/modules
OUTPUT_DIR="${PROJECT_ROOT_DIR}/output/modules/atb_models/"
mkdir -p "$MODULES_DIR"
mkdir -p "$OUTPUT_DIR"

RELEASE="$1"
[ -z "$RELEASE" ] && RELEASE="release"

function copydir()
{
    src=$1
    dst=$2
    if [ -d "$dst" ]; then
        echo "$dst exists, skip to overwrite it."
        return
    fi
    if [ -d "$src" ]; then
        cp -rf "$src" "$dst"
        echo "--- Copy $src as $dst."
    fi
}

function prepare_dependency()
{
    [ -z "$BUILD_MIES_3RDPARTY_ROOT_DIR" ] && BUILD_MIES_3RDPARTY_ROOT_DIR="$PROJECT_ROOT_DIR"/third_party
    [ -z "$BUILD_MIES_3RDPARTY_INSTALL_DIR" ] && BUILD_MIES_3RDPARTY_INSTALL_DIR="$BUILD_MIES_3RDPARTY_ROOT_DIR"/install

    # Prepare dependency for batchscheduler
    src_dir="$BUILD_MIES_3RDPARTY_INSTALL_DIR"
    dst_dir="$MODULES_DIR"/MindIE-LLM/examples/atb_models/third_party
    mkdir -p "$dst_dir"

    copydir "$src_dir"/nlohmann-json "$dst_dir"/nlohmannJson
    copydir "$src_dir"/spdlog "$dst_dir"/spdlog
}

cd "$MODULES_DIR"/MindIE-LLM/examples/atb_models
prepare_dependency

rm -rf "$MODULES_DIR"/MindIE-LLM/examples/atb_models/output
cd "$MODULES_DIR"/MindIE-LLM/examples/atb_models
bash scripts/build.sh $RELEASE --use_cxx11_abi=$GLOBAL_ABI_VERSION
cp -f "$MODULES_DIR"/MindIE-LLM/examples/atb_models/output/atb_models/Ascend-mindie-atb-models_*.tar.gz "$OUTPUT_DIR"/
ls "$OUTPUT_DIR"
echo "Build atb models .tar.gz at $OUTPUT_DIR successfully."
