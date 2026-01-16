# =============================================================================
# 真空系统 Tango 设备服务 CMake 配置
# =============================================================================

# 设备服务名称
set(VACUUM_SYSTEM_DEVICE_NAME "VacuumSystemDevice")

# 源文件
set(VACUUM_SYSTEM_SOURCES
    ${CMAKE_SOURCE_DIR}/src/device_services/vacuum_system_device.cpp
)

# 头文件
set(VACUUM_SYSTEM_HEADERS
    ${CMAKE_SOURCE_DIR}/include/device_services/vacuum_system_device.h
    ${CMAKE_SOURCE_DIR}/include/device_services/vacuum_system_plc_mapping.h
)

# 创建设备服务可执行文件
add_executable(${VACUUM_SYSTEM_DEVICE_NAME}
    ${VACUUM_SYSTEM_SOURCES}
    ${VACUUM_SYSTEM_HEADERS}
)

# 链接库
target_link_libraries(${VACUUM_SYSTEM_DEVICE_NAME}
    ${TANGO_LIBRARIES}
    ${OMNIORB_LIBRARIES}
    plc_communication
    system_config
)

# 如果使用 OPC UA
if(USE_OPEN62541)
    target_compile_definitions(${VACUUM_SYSTEM_DEVICE_NAME} PRIVATE USE_OPEN62541)
    target_link_libraries(${VACUUM_SYSTEM_DEVICE_NAME} open62541)
endif()

# nlohmann_json (用于报警 JSON 序列化)
find_package(nlohmann_json QUIET)
if(nlohmann_json_FOUND)
    target_link_libraries(${VACUUM_SYSTEM_DEVICE_NAME} nlohmann_json::nlohmann_json)
else()
    # 如果没有找到，假设头文件在系统路径中
    message(STATUS "nlohmann_json not found via find_package, assuming header-only installation")
endif()

# 安装
install(TARGETS ${VACUUM_SYSTEM_DEVICE_NAME}
    RUNTIME DESTINATION bin
)

# 安装配置文件
install(FILES
    ${CMAKE_SOURCE_DIR}/config/vacuum_system_config.json
    DESTINATION etc/tango
)

