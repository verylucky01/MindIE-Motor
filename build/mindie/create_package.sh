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

function make_run() {
    ARCH=$1
    RELEASE_TMP_DIR_ARCH=$RELEASE_TMP_DIR/${ARCH}
    mkdir -p ${RELEASE_TMP_DIR_ARCH}

    touch ${RELEASE_TMP_DIR_ARCH}/version.info
    cat>${RELEASE_TMP_DIR_ARCH}/version.info<<EOF
Ascend-mindie : ${mindie_version}
mindie-rt: ${rt_version}
mindie-torch: ${torch_version}
mindie-service: ${service_version}
mindie-llm: ${llm_version}
mindie-sd:${sd_version}
Platform : ${ARCH}
EOF

    mkdir -p ${RELEASE_TMP_DIR_ARCH}/scripts
    cp $BUILD_DIR/scripts/uninstall.sh ${RELEASE_TMP_DIR_ARCH}/scripts
    cp $BUILD_DIR/scripts/set_env.sh ${RELEASE_TMP_DIR_ARCH}
    cp $BUILD_DIR/scripts/install.sh ${RELEASE_TMP_DIR_ARCH}
    cp $BUILD_DIR/../MindIE-RT/${ARCH}*/Ascend-mindie-rt*.run ${RELEASE_TMP_DIR_ARCH}
    cp $BUILD_DIR/../MindIE-Torch/${ARCH}*/Ascend-mindie-torch*.tar.gz ${RELEASE_TMP_DIR_ARCH}
    cp $BUILD_DIR/../MindIE-Service/${ARCH}*/Ascend-mindie-service_*.run ${RELEASE_TMP_DIR_ARCH}
    cp $BUILD_DIR/../MindIE-LLM/${ARCH}*/Ascend-mindie-llm*.run ${RELEASE_TMP_DIR_ARCH}
    cp $BUILD_DIR/../MindIE-SD/Ascend-mindie-sd*.tar.gz ${RELEASE_TMP_DIR_ARCH}
    cp $BUILD_DIR/scripts/*.txt ${RELEASE_TMP_DIR_ARCH}

    chmod +x ${RELEASE_TMP_DIR_ARCH}/*

    sed -i "s!VERSION=replace_version!VERSION=${version}!" ${RELEASE_TMP_DIR_ARCH}/install.sh
    sed -i "s!LOG_PATH=replace_log_path!LOG_PATH=${LOG_PATH}!" ${RELEASE_TMP_DIR_ARCH}/install.sh
    sed -i "s!LOG_NAME=replace_log_name!LOG_NAME=${LOG_NAME}!" ${RELEASE_TMP_DIR_ARCH}/install.sh
    sed -i "s!ARCH=replace_arch!ARCH=${ARCH}!" $RELEASE_TMP_DIR_ARCH/install.sh

    sed -i "s!VERSION=replace_version!VERSION=${version}!" ${RELEASE_TMP_DIR_ARCH}/scripts/uninstall.sh
    sed -i "s!LOG_PATH=replace_log_path!LOG_PATH=${LOG_PATH}!" ${RELEASE_TMP_DIR_ARCH}/scripts/uninstall.sh
    sed -i "s!LOG_NAME=replace_log_name!LOG_NAME=${LOG_NAME}!" ${RELEASE_TMP_DIR_ARCH}/scripts/uninstall.sh

    cd $BUILD_DIR/../../CI/script/
    ./signature.sh cms
    cd -
    mkdir -p ${OUTPUT_DIR}/${ARCH}-linux/
    chmod +x $MAKESELF_DIR/*
    $MAKESELF_DIR/makeself.sh --header $MAKESELF_DIR/makeself-header.sh --help-header $BUILD_DIR/scripts/help.info \
        --gzip --complevel 4 --nomd5 --sha256 --chown ${RELEASE_TMP_DIR_ARCH} \
        $OUTPUT_DIR/${ARCH}-linux/Ascend-mindie_${version}_linux-${ARCH}.run "Ascend-mindie" ./install.sh
}
