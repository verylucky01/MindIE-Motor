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


# Set directories for grpc and other dependencies

set -e
CURRENT_PATH=$(cd $(dirname -- $0); pwd)

[ -z "$MINDIE_SERVICE_SRC_DIR" ] && echo "MINDIE_SERVICE_SRC_DIR is not set, exit" && exit 1

BUILD_MIES_3RDPARPT_ROOT_DIR="$MINDIE_SERVICE_SRC_DIR"/third_party

export THIRD_PARTY_ZIP_DIR="$BUILD_MIES_3RDPARPT_ROOT_DIR"/zipped
export THIRD_PARTY_SRC_DIR="$BUILD_MIES_3RDPARPT_ROOT_DIR"/src

[ -z "${THIRD_PARTY_SRC_DIR}" ] && echo "THIRD_PARTY_SRC_DIR is not set, exit." && exit 1
mkdir -p "$THIRD_PARTY_SRC_DIR"

GPRC_DIR="${THIRD_PARTY_SRC_DIR}/grpc/grpc"


# copy patch
copy_grpc_patch() {
    cp -rf "$BUILD_MIES_3RDPARPT_ROOT_DIR/grpc_patch/"* "$THIRD_PARTY_SRC_DIR/grpc"
}

patch_grpc() {
    # Create directories
    # Unpack and patch gRPC
    GPRC_SOURCE_DIR="${THIRD_PARTY_SRC_DIR}/grpc/grpc_source"
    mkdir -p "${GPRC_SOURCE_DIR}/SOURCE" "${GPRC_DIR}"
    echo "Unpacking and patching gRPC..."
    tar -xf "${GPRC_SOURCE_DIR}/"*.tar.gz -C "${GPRC_SOURCE_DIR}/SOURCE"
    GRPC_FILE_NAME=$(ls "${GPRC_SOURCE_DIR}/SOURCE" | head -n 1)
    cd "${GPRC_SOURCE_DIR}/SOURCE/${GRPC_FILE_NAME}"
    for patch in $(grep 'Patch' "${GPRC_SOURCE_DIR}/"*.spec | awk '{print $2}'); do
        if [ -e "${GPRC_SOURCE_DIR}/$patch" ]; then
            patch -p1 < "${GPRC_SOURCE_DIR}/$patch"
        fi
    done
    cp -rf "${GPRC_SOURCE_DIR}/SOURCE/${GRPC_FILE_NAME}/"* "${GPRC_DIR}"
    patch "${GPRC_DIR}/CMakeLists.txt" "${THIRD_PARTY_SRC_DIR}/grpc/grpc-safe-compile.patch"
}

patch_zlib() {
    # Unpack and patch zlib
    ZLIB_SOURCE_DIR="${THIRD_PARTY_SRC_DIR}/grpc/zlib"
    GRPC_ZLIB_DIR="${THIRD_PARTY_SRC_DIR}/grpc/grpc/third_party/zlib"
    mkdir -p "${ZLIB_SOURCE_DIR}/SOURCE" "${GRPC_ZLIB_DIR}"
    echo "Unpacking and patching Zlib..."
    tar -xf "${ZLIB_SOURCE_DIR}/"*.tar.xz -C "${ZLIB_SOURCE_DIR}/SOURCE"
    ZLIB_FILE_NAME=$(ls "${ZLIB_SOURCE_DIR}/SOURCE" | head -n 1)
    cd "${ZLIB_SOURCE_DIR}/SOURCE/${ZLIB_FILE_NAME}"
    for patch in $(grep 'Patch' "${ZLIB_SOURCE_DIR}/"*.spec | awk '{print $2}'); do
        if [ -e "${ZLIB_SOURCE_DIR}/$patch" ]; then
            patch -p1 < "${ZLIB_SOURCE_DIR}/$patch"
        fi
    done
    cp -rf "${ZLIB_SOURCE_DIR}/SOURCE/${ZLIB_FILE_NAME}/"* "${GRPC_ZLIB_DIR}"
}

patch_protobuf() {
    # Unpack and patch protobuf
    echo "Unpacking and patching Protobuf..."
    PROTOBUF_SOURCE_DIR="${THIRD_PARTY_SRC_DIR}/grpc/protobuf"
    GRPC_PROTOBUF_DIR="${THIRD_PARTY_SRC_DIR}/grpc/grpc/third_party/protobuf"
    mkdir -p "${PROTOBUF_SOURCE_DIR}/SOURCE" "${GRPC_PROTOBUF_DIR}"
    tar -xzf "${PROTOBUF_SOURCE_DIR}/"*.tar.gz -C "${PROTOBUF_SOURCE_DIR}/SOURCE"
    PROTOBUF_FILE_NAME=$(ls "${PROTOBUF_SOURCE_DIR}/SOURCE" | head -n 1)
    cd "${PROTOBUF_SOURCE_DIR}/SOURCE/${PROTOBUF_FILE_NAME}"
    for patch in $(grep 'Patch' "${PROTOBUF_SOURCE_DIR}/"*.spec | awk '{print $2}'); do
        if [ -e "${PROTOBUF_SOURCE_DIR}/$patch" ]; then
            patch -p1 < "${PROTOBUF_SOURCE_DIR}/$patch"
        fi
    done
    cp -rf "${PROTOBUF_SOURCE_DIR}/SOURCE/${PROTOBUF_FILE_NAME}/"* "${GRPC_PROTOBUF_DIR}"
}

patch_abseil_cpp() {
    # Unpack and patch abseil
    echo "Unpacking and patching Abseil cpp..."
    ABSEIL_SOURCE_DIR="${THIRD_PARTY_SRC_DIR}/grpc/abseil-cpp"
    GRPC_ABSEIL_DIR="${THIRD_PARTY_SRC_DIR}/grpc/grpc/third_party/abseil-cpp"
    mkdir -p "${ABSEIL_SOURCE_DIR}/SOURCE" "${GRPC_ABSEIL_DIR}"
    tar -xzf "${ABSEIL_SOURCE_DIR}/"*.tar.gz -C "${ABSEIL_SOURCE_DIR}/SOURCE"
    ABSEIL_FILE_NAME=$(ls "${ABSEIL_SOURCE_DIR}/SOURCE" | head -n 1)
    patch "${ABSEIL_SOURCE_DIR}/SOURCE/${ABSEIL_FILE_NAME}/CMakeLists.txt" "${THIRD_PARTY_SRC_DIR}/grpc/absl_safe_compile.patch"
    cd "${ABSEIL_SOURCE_DIR}/SOURCE/${ABSEIL_FILE_NAME}"
    for patch in $(grep 'Patch' "${ABSEIL_SOURCE_DIR}/"*.spec | awk '{print $2}'); do
        if [ -e "${ABSEIL_SOURCE_DIR}/$patch" ]; then
            patch -p1 < "${ABSEIL_SOURCE_DIR}/$patch"
        fi
    done
    cp -rf "${ABSEIL_SOURCE_DIR}/SOURCE/${ABSEIL_FILE_NAME}/"* "${GRPC_ABSEIL_DIR}"
    # Fix options.h in abseil
    echo "-------------------->before fix options.h"
    # Read options.h
    ABSL_INTERNAL_OPTIONS_H_CONTENTS=$(cat "${GRPC_ABSEIL_DIR}/absl/base/options.h")
    # regex replace with sed
    ABSL_INTERNAL_OPTIONS_H_PINNED=$(echo "$ABSL_INTERNAL_OPTIONS_H_CONTENTS" | sed 's/#define ABSL_OPTION_USE_STD_\([^ ]*\) 2/#define ABSL_OPTION_USE_STD_\1 0/')
    # Write options.h
    echo "$ABSL_INTERNAL_OPTIONS_H_PINNED" > "${GRPC_ABSEIL_DIR}/absl/base/options.h"
    echo "-------------------->after fix options.h"
}

patch_re2() {
    # Unpack and patch re2
    echo "Unpacking and patching RE2..."
    RE2_SOURCE_DIR="${THIRD_PARTY_SRC_DIR}/grpc/re2"
    GRPC_RE2_DIR="${THIRD_PARTY_SRC_DIR}/grpc/grpc/third_party/re2"
    mkdir -p "${RE2_SOURCE_DIR}/SOURCE" "${GRPC_RE2_DIR}"
    tar -xzf "${RE2_SOURCE_DIR}/"*.tar.gz -C "${RE2_SOURCE_DIR}/SOURCE"
    RE2_FILE_NAME=$(ls "${RE2_SOURCE_DIR}/SOURCE" | head -n 1)
    cd "${RE2_SOURCE_DIR}/SOURCE/${RE2_FILE_NAME}"
    for patch in $(grep 'Patch' "${RE2_SOURCE_DIR}/"*.spec | awk '{print $2}'); do
        if [ -e "${RE2_SOURCE_DIR}/$patch" ]; then
            patch -p1 < "${RE2_SOURCE_DIR}/$patch"
        fi
    done
    cp -rf "${RE2_SOURCE_DIR}/SOURCE/${RE2_FILE_NAME}/"* "${GRPC_RE2_DIR}"
}

patch_cares() {
    # Unpack and patch cares
    echo "Unpacking and patching CARES..."
    CARES_SOURCE_DIR="${THIRD_PARTY_SRC_DIR}/grpc/cares"
    GRPC_CARES_DIR="${THIRD_PARTY_SRC_DIR}/grpc/grpc/third_party/cares/cares"
    mkdir -p "${CARES_SOURCE_DIR}/SOURCE" "${GRPC_CARES_DIR}"
    tar -xzf "${CARES_SOURCE_DIR}/"*.tar.gz -C "${CARES_SOURCE_DIR}/SOURCE"
    CARES_FILE_NAME=$(ls "${CARES_SOURCE_DIR}/SOURCE" | head -n 1)
    cd "${CARES_SOURCE_DIR}/SOURCE/${CARES_FILE_NAME}"
    for patch in $(grep 'Patch' "${CARES_SOURCE_DIR}/"*.spec | awk '{print $2}'); do
        if [ -e "${CARES_SOURCE_DIR}/$patch" ]; then
            patch -p1 < "${CARES_SOURCE_DIR}/$patch"
        fi
    done
    cp -rf "${CARES_SOURCE_DIR}/SOURCE/${CARES_FILE_NAME}/"* "${GRPC_CARES_DIR}"
}

clean_temp_dir() {
    # delete all temp file or dir whose name is not grpc/
    mv "${THIRD_PARTY_SRC_DIR}"/grpc/grpc "${THIRD_PARTY_SRC_DIR}"/grpc-src
    rm -rf "${THIRD_PARTY_SRC_DIR}"/grpc
    mv "${THIRD_PARTY_SRC_DIR}"/grpc-src "${THIRD_PARTY_SRC_DIR}"/grpc
}

create_dummy_dir() {
    # These are just placeholder empty directories, aim to avoid downloading this unused modules.
    mkdir -p "${GPRC_DIR}/third_party/xds"
    mkdir -p "${GPRC_DIR}/third_party/envoy-api"
    mkdir -p "${GPRC_DIR}/third_party/googleapis"
    mkdir -p "${GPRC_DIR}/third_party/opencensus-proto/src"
}

main() {
    copy_grpc_patch
    patch_grpc
    patch_zlib
    patch_protobuf
    patch_abseil_cpp
    patch_re2
    patch_cares
    create_dummy_dir

    echo "Prepare grpc success"
}

main
