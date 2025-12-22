set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(HTTP_CLIENT_CTL_DIR ${MINDIE_SERVICE_ROOT}/mindie_service/utils/http_client_ctl)

set(MINDIE_MS_SRCS
    ${MINDIE_MS_SRC_ROOT}/common
    ${MINDIE_MS_SRC_ROOT}/common/http_server
    ${MINDIE_MS_SRC_ROOT}/common/http_client
    ${MINDIE_MS_SRC_ROOT}/deployer/infer_manager
    ${MINDIE_MS_TEST_ROOT}/fuzz/common
    ${MINDIE_MS_TEST_ROOT}/dt/common/helper
    ${HTTP_CLIENT_CTL_DIR}/log
    ${HTTP_CLIENT_CTL_DIR}/http_client
    ${HTTP_CLIENT_CTL_DIR}/common
    ${HTTP_CLIENT_CTL_DIR}/params
)

include_directories(
    # 源码
    ${MINDIE_MS_SRC_ROOT}/
    ${MINDIE_MS_SRC_ROOT}/common
    ${MINDIE_MS_SRC_ROOT}/common/http_server
    ${MINDIE_MS_SRC_ROOT}/common/http_client
    ${MINDIE_MS_SRC_ROOT}/deployer/infer_manager
    ${MINDIE_MS_SRC_ROOT}/deployer/server/http_server

    #三方
    ${MINDIE_MS_SRC_ROOT}/build/open_source/boost
    ${MINDIE_MS_SRC_ROOT}/build/open_source/nlohmann/include
    ${MINDIE_MS_SRC_ROOT}/build/open_source/openssl/include
    ${MINDIE_MS_SRC_ROOT}/build/open_source
    ${MINDIE_MS_TEST_ROOT}/dt/common/helper
)

foreach(DIR ${MINDIE_MS_SRCS})
    aux_source_directory(${DIR} DEPLOYER_SRCS_FILES)
endforeach()


list(REMOVE_ITEM DEPLOYER_SRCS_FILES "${MINDIE_MS_SRC_ROOT}/deployer/infer_manager/CrossNodeServer.cpp")
list(REMOVE_ITEM DEPLOYER_SRCS_FILES "${MINDIE_MS_SRC_ROOT}/deployer/infer_manager/SingleNodeServer.cpp")

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} DEPLOYER_SRCS_FILES)

add_executable(mindie_ms_deployer_fuzz ${DEPLOYER_SRCS_FILES})
target_link_libraries(mindie_ms_deployer_fuzz gtest)
target_link_libraries(mindie_ms_deployer_fuzz crypto ssl)
target_link_libraries(mindie_ms_deployer_fuzz Secodefuzz)
target_link_libraries(mindie_ms_deployer_fuzz Secodepits)
target_link_libraries(mindie_ms_deployer_fuzz xml2)
target_link_libraries(mindie_ms_deployer_fuzz boundscheck)

target_link_libraries(mindie_ms_deployer_fuzz dl pthread)
