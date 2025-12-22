project(MindIE-MS-Deployer)

include_directories(
    ${UTILS_HTTP_CLIENT_CTL_PATH}/common/
    ${UTILS_HTTP_CLIENT_CTL_PATH}/params/
    ${UTILS_HTTP_CLIENT_CTL_PATH}/log/
    ${UTILS_HTTP_CLIENT_CTL_PATH}/http_client/
    ${ROOT_PATH}/common/http_server
    ${CMAKE_CURRENT_SOURCE_DIR}/server
    ${CMAKE_CURRENT_SOURCE_DIR}/infer_manager
    ${CMAKE_CURRENT_SOURCE_DIR}/
)

file(GLOB ms_server_source_files
    ${CMAKE_CURRENT_SOURCE_DIR}/infer_manager/*.cpp
    ${UTILS_HTTP_CLIENT_CTL_PATH}/common/*.cpp
    ${UTILS_HTTP_CLIENT_CTL_PATH}/params/*.cpp
    ${UTILS_HTTP_CLIENT_CTL_PATH}/log/*.cpp
    ${UTILS_HTTP_CLIENT_CTL_PATH}/http_client/*.cpp
    ${ROOT_PATH}/common/http_server/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/server/main.cpp
)

set(MS_SERVER_SOURCE ${ms_server_source_files} ${CMAKE_CURRENT_SOURCE_DIR}/server/main.cpp)

add_executable(ms_server ${MS_SERVER_SOURCE})
target_link_libraries(ms_server crypto ssl boundscheck pthread hse_cryption -ldl)

file(GLOB msctl_source_files
    ${CMAKE_CURRENT_SOURCE_DIR}/msctl/main.cpp
    ${UTILS_HTTP_CLIENT_CTL_PATH}/common/*.cpp
    ${UTILS_HTTP_CLIENT_CTL_PATH}/params/*.cpp
    ${UTILS_HTTP_CLIENT_CTL_PATH}/log/*.cpp
    ${UTILS_HTTP_CLIENT_CTL_PATH}/http_client/*.cpp
)

set(MS_CTL_SOURCE ${msctl_source_files})
add_executable(msctl ${MS_CTL_SOURCE})
target_link_libraries(msctl crypto ssl boundscheck pthread hse_cryption -ldl)

install(TARGETS msctl DESTINATION bin)
install(TARGETS ms_server DESTINATION bin)

install(FILES ${ROOT_PATH}/config/msctl.json DESTINATION config)
install(FILES ${ROOT_PATH}/config/infer_server.json DESTINATION config)
install(FILES ${ROOT_PATH}/config/ms_server.json DESTINATION config)
install(FILES ${ROOT_PATH}/config/update_server.json DESTINATION config)
