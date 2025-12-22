set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(HTTP_CLIENT_CTL_DIR ${MINDIE_SERVICE_ROOT}/mindie_service/utils/http_client_ctl)

set(MINDIE_MS_SRCS
    ${MINDIE_MS_SRC_ROOT}/common
    ${MINDIE_MS_SRC_ROOT}/common/http_client
    ${MINDIE_MS_SRC_ROOT}/deployer/infer_manager
    ${MINDIE_MS_TEST_ROOT}/dt/common/helper
    ${MINDIE_MS_TEST_ROOT}/dt/common/main
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
)

file(GLOB TEST_MAIN_SRC
    ${MINDIE_MS_TEST_ROOT}/dt/common/helper/*.cpp
    ${MINDIE_MS_TEST_ROOT}/dt/common/main/*.cpp
    ${MINDIE_MS_TEST_ROOT}/common/backtrace/*.cpp
)

foreach(DIR ${MINDIE_MS_SRCS})
    aux_source_directory(${DIR} DEPLOYER_SRCS_FILES)
endforeach()

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/infer_manager DEPLOYER_SRCS_FILES)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/util DEPLOYER_SRCS_FILES)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/msctl DEPLOYER_SRCS_FILES)


list(REMOVE_ITEM DEPLOYER_SRCS_FILES "${MINDIE_MS_SRC_ROOT}/deployer/infer_manager/CrossNodeServer.cpp")
list(REMOVE_ITEM DEPLOYER_SRCS_FILES "${MINDIE_MS_SRC_ROOT}/deployer/infer_manager/SingleNodeServer.cpp")
list(REMOVE_ITEM DEPLOYER_SRCS_FILES "${MINDIE_MS_SRC_ROOT}/common/Util.cpp")
list(REMOVE_ITEM DEPLOYER_SRCS_FILES "${MINDIE_MS_SRC_ROOT}/common/Logger.cpp")
list(REMOVE_ITEM DEPLOYER_SRCS_FILES "${MINDIE_MS_SRC_ROOT}/common/ConfigParams.cpp")

add_executable(mindie_ms_deployer_ut ${DEPLOYER_SRCS_FILES}  ${TEST_MAIN_SRC})
target_link_libraries(mindie_ms_deployer_ut  gtest)
target_link_libraries(mindie_ms_deployer_ut crypto ssl boundscheck hse_cryption)

target_link_libraries(mindie_ms_deployer_ut dl pthread)