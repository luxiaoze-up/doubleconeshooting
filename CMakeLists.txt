cmake_minimum_required(VERSION 3.16)
project(DoubleConeShooting)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "DoubleConeShooting supports Ubuntu 24.04/Linux only")
endif()

# Generate compile_commands.json for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Bypass Qt license check (for development)
set(ENV{QTFRAMEWORK_BYPASS_LICENSE_CHECK} "1")

# Use C++17 for Qt6 compatibility
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GCC/Clang flags used on Ubuntu 24.04
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated -Wno-sign-compare")

# Force system Tango headers to come before the project include directory.
if(EXISTS "/usr/include/tango/tango.h")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isystem /usr/include/tango")
    message(STATUS "Prepending system Tango headers via CMAKE_CXX_FLAGS")
elseif(EXISTS "/usr/local/include/tango/tango.h")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isystem /usr/local/include/tango")
    message(STATUS "Prepending system Tango headers via CMAKE_CXX_FLAGS")
endif()

# Include directories
# Note: Project include comes after system Tango (set in CMAKE_CXX_FLAGS above)
include_directories(${CMAKE_SOURCE_DIR}/include)

# Find open62541 OPC UA library
find_library(OPEN62541_LIB open62541 PATHS /usr/local/lib)
if(OPEN62541_LIB)
    message(STATUS "Found open62541 library: ${OPEN62541_LIB}")
    add_definitions(-DUSE_OPEN62541)
    include_directories(/usr/local/include)
else()
    message(WARNING "open62541 library not found - OPC UA communication disabled")
endif()

#Find snap7 S7 protocol library
find_library(SNAP7_LIB snap7 PATHS /usr/local/lib)
if(SNAP7_LIB)
    message(STATUS "Found snap7 library: ${SNAP7_LIB}")
    add_definitions(-DUSE_SNAP7)
    include_directories(/usr/local/include)
else()
    message(WARNING "snap7 library not found - S7 communication disabled")
endif()

# Create common library (always build)
add_library(common_lib
    src/common/device_interface.cpp
    src/common/kinematics.cpp
    src/common/system_config.cpp
    src/common/plc_communication.cpp
    src/common/encoder_acquisition.cpp
)

# Link open62541 to common_lib if found
if(OPEN62541_LIB)
    target_link_libraries(common_lib PUBLIC ${OPEN62541_LIB})
endif()
if(SNAP7_LIB)
    target_link_libraries(common_lib PUBLIC ${SNAP7_LIB})
endif()

# Add Tango include directories early for common_lib
if(EXISTS "/usr/include/tango/tango.h")
    target_include_directories(common_lib BEFORE PUBLIC /usr/include/tango)
elseif(EXISTS "/usr/local/include/tango/tango.h")
    target_include_directories(common_lib BEFORE PUBLIC /usr/local/include/tango)
endif()

# Try to find Tango (optional)
# find_package(PkgConfig)
# if(PkgConfig_FOUND)
#     pkg_check_modules(TANGO tango)
# endif()

# If pkg-config didn't find Tango, try manual detection
if(NOT TANGO_FOUND AND EXISTS "/usr/include/tango/tango.h")
    message(STATUS "Tango found manually at /usr/include/tango")
    set(TANGO_FOUND TRUE)
    set(TANGO_INCLUDE_DIRS "/usr/include/tango")
    set(TANGO_LIBRARIES "/usr/lib/libtango.so")
elseif(NOT TANGO_FOUND AND EXISTS "/usr/local/include/tango/tango.h")
    message(STATUS "Tango found manually at /usr/local/include/tango")
    set(TANGO_FOUND TRUE)
    set(TANGO_INCLUDE_DIRS "/usr/local/include/tango")
    set(TANGO_LIBRARIES "/usr/local/lib/libtango.so")
endif()

#find omniORB libraries
find_library(OMNIORB4_LIB omniORB4 REQUIRED)
find_library(OMNITHREAD_LIB omnithread REQUIRED)
find_library(OMNIDYNAMIC4_LIB omniDynamic4 REQUIRED)
if (NOT OMNIORB4_LIB OR NOT OMNITHREAD_LIB OR NOT OMNIDYNAMIC4_LIB)
    message(FATAL_ERROR "未找到 omniORB 全套库！请安装 libomniorb4-dev")
endif()

#find ZeroMQ library
find_library(ZMQ_LIB zmq REQUIRED)
if (NOT ZMQ_LIB)
    message(FATAL_ERROR "未找到 ZeroMQ 库！请安装 libzmq5-dev")
endif()

# Only build Tango servers if Tango is properly found
if(TANGO_FOUND)
    message(STATUS "Tango found - building device servers")
    add_definitions(-DHAS_TANGO)
    include_directories(${TANGO_INCLUDE_DIRS})
    link_directories(${TANGO_LIBRARY_DIRS})
    
    # Add Tango include directories to common_lib
    target_include_directories(common_lib PUBLIC ${TANGO_INCLUDE_DIRS})
    
    # Add Tango libraries to common_lib
    target_link_libraries(common_lib PUBLIC ${TANGO_LIBRARIES})
    
    # Add Tango-dependent implementations only when Tango is available.
    target_sources(common_lib PRIVATE
        src/common/standard_system_device.cpp
        src/common/tango_device_wrapper.cpp
    )
    
    # Link directories for local libraries
    link_directories(${CMAKE_SOURCE_DIR}/lib)

    # New Device Servers
    add_executable(motion_controller_server
        src/device_services/motion_controller_device.cpp
    )
    target_link_libraries(motion_controller_server ${TANGO_LIBRARIES} ${OMNIORB4_LIB} ${OMNIDYNAMIC4_LIB} ${OMNITHREAD_LIB} ${ZMQ_LIB} common_lib LTSMC)

    add_executable(encoder_server
        src/device_services/encoder_device.cpp
    )
    target_link_libraries(encoder_server ${TANGO_LIBRARIES} ${OMNIORB4_LIB} ${OMNIDYNAMIC4_LIB} ${OMNITHREAD_LIB} ${ZMQ_LIB} common_lib)
    # Device servers that compile successfully
    add_executable(large_stroke_server
        src/device_services/large_stroke_device.cpp
    )
    target_link_libraries(large_stroke_server ${TANGO_LIBRARIES} ${OMNIORB4_LIB} ${OMNIDYNAMIC4_LIB} ${OMNITHREAD_LIB} ${ZMQ_LIB} common_lib LTSMC)
    target_compile_options(large_stroke_server PRIVATE -Wno-deprecated)

    # 辅助支撑系统服务 (Auxiliary Support Device Server)
    add_executable(auxiliary_support_server
        src/device_services/auxiliary_support_device.cpp
    )
    target_link_libraries(auxiliary_support_server ${TANGO_LIBRARIES} ${OMNIORB4_LIB} ${OMNIDYNAMIC4_LIB} ${OMNITHREAD_LIB} ${ZMQ_LIB} common_lib LTSMC)
    target_compile_options(auxiliary_support_server PRIVATE -Wno-deprecated)

    add_executable(vacuum_server
        src/device_services/vacuum_device.cpp
    )
    target_link_libraries(vacuum_server ${TANGO_LIBRARIES} ${OMNIORB4_LIB} ${OMNIDYNAMIC4_LIB} ${OMNITHREAD_LIB} ${ZMQ_LIB} common_lib)
    if(SNAP7_LIB)
        target_link_libraries(vacuum_server ${SNAP7_LIB})
    endif()
    target_compile_options(vacuum_server PRIVATE -Wno-deprecated)

    # New Vacuum System Server (Replaces old vacuum_server)
    add_executable(vacuum_system_server
        src/device_services/vacuum_system_device.cpp
    )
    target_link_libraries(vacuum_system_server ${TANGO_LIBRARIES} ${OMNIORB4_LIB} ${OMNIDYNAMIC4_LIB} ${OMNITHREAD_LIB} ${ZMQ_LIB} common_lib)
    target_compile_options(vacuum_system_server PRIVATE -Wno-deprecated)
    
    # Link nlohmann_json if found, otherwise assume it's in include path
    find_package(nlohmann_json QUIET)
    if(nlohmann_json_FOUND)
        target_link_libraries(vacuum_system_server nlohmann_json::nlohmann_json)
    endif()

    add_executable(interlock_server
        src/system_services/interlock_service.cpp
    )
    target_link_libraries(interlock_server ${TANGO_LIBRARIES} ${OMNIORB4_LIB} ${OMNIDYNAMIC4_LIB} ${OMNITHREAD_LIB} ${ZMQ_LIB} common_lib)
    target_compile_options(interlock_server PRIVATE -Wno-deprecated)

    # Six DOF server - now fixed
    add_executable(six_dof_server
        src/device_services/six_dof_device.cpp
    )
    target_link_libraries(six_dof_server ${TANGO_LIBRARIES} ${OMNIORB4_LIB} ${OMNIDYNAMIC4_LIB} ${OMNITHREAD_LIB} ${ZMQ_LIB} common_lib LTSMC)
    target_compile_options(six_dof_server PRIVATE -Wno-deprecated)

    # Reflection Imaging Characterization Device Server (新版本)
    # 功能：反射光成像系统，包含4个CCD相机（上下各两个：1倍和10倍物镜）、
    #       双三坐标平台（上下各3轴）、辅助支撑和力传感器
    # 架构：通过Tango代理调用其他设备服务器，不直接使用LTSMC库
    add_executable(reflection_imaging_server
        src/device_services/reflection_imaging_device.cpp
        src/drivers/mv_cu020_19gc.cpp
    )
    target_link_libraries(reflection_imaging_server ${TANGO_LIBRARIES} ${OMNIORB4_LIB} ${OMNIDYNAMIC4_LIB} ${OMNITHREAD_LIB} ${ZMQ_LIB} common_lib)
    target_compile_options(reflection_imaging_server PRIVATE -Wno-deprecated)

else()
    message(STATUS "Tango not found - building simulation mode only")
    add_definitions(-DNO_TANGO)
endif()

# Qt MOC setup (set globally before finding Qt)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Set environment variable for all MOC operations
set(CMAKE_AUTOMOC_ENVIRONMENT "QTFRAMEWORK_BYPASS_LICENSE_CHECK=1")

# Qt5 is optional. On Ubuntu install it with qtbase5-dev when the C++ GUI is needed.
find_package(Qt5 COMPONENTS Core Widgets)

if(Qt5_FOUND)
    # Link Qt5 Core to common_lib for JSON parsing (only if Qt5 is found)
    target_link_libraries(common_lib PUBLIC Qt5::Core)
    add_definitions(-DHAS_QT5)

    get_target_property(QT_MOC_LOCATION Qt5::moc LOCATION)
    message(STATUS "Using Qt5 MOC: ${QT_MOC_LOCATION}")
else()
    message(STATUS "Qt5 not found - skipping GUI build and JSON parsing in common_lib")
    message(STATUS "On Ubuntu 24.04 install Qt5 with: sudo apt-get install qtbase5-dev")
    message(STATUS "Note: system_config.cpp will use default values without JSON parsing")
endif()

if(Qt5_FOUND AND TANGO_FOUND)
    message(STATUS "Qt and Tango found - building GUI")
    
    # Use Qt5 (system installation)
    set(QT_LIBRARIES Qt5::Core Qt5::Widgets)
    include_directories(${Qt5Core_INCLUDE_DIRS})
    include_directories(${Qt5Widgets_INCLUDE_DIRS})
    
    # Create main controller executable
    # Note: 头文件必须添加到源文件列表才能被AUTOMOC处理
    add_executable(main_controller
        src/integrated_control/main_controller.cpp
        include/integrated_control/main_controller.h
    )
    
    # Ensure MOC is properly configured
    set_target_properties(main_controller PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
    )
    
    # Set environment variable for MOC (must be set before target creation)
    set_target_properties(main_controller PROPERTIES
        AUTOMOC_ENVIRONMENT "QTFRAMEWORK_BYPASS_LICENSE_CHECK=1"
    )
    
    # Explicitly tell CMake to process the header file for MOC
    # This ensures the header is scanned for Q_OBJECT
    set_property(TARGET main_controller PROPERTY AUTOMOC_SOURCE_EXTENSIONS "h;hpp;hxx;hh")
    
    target_link_libraries(main_controller 
        ${TANGO_LIBRARIES}
        ${QT_LIBRARIES}
        common_lib
    )
    target_compile_options(main_controller PRIVATE -Wno-deprecated)
    
elseif(Qt5_FOUND)
    message(STATUS "Qt5 found but Tango missing - building simulation GUI")
    
    # Use Qt5 (system installation)
    set(QT_LIBRARIES Qt5::Core Qt5::Widgets)
    include_directories(${Qt5Core_INCLUDE_DIRS})
    include_directories(${Qt5Widgets_INCLUDE_DIRS})
    
    message(STATUS "Linking main_controller with: ${QT_LIBRARIES}")

    # Create main controller executable (using the same source, it handles --sim)
    add_executable(main_controller
        src/integrated_control/main_controller.cpp
        include/integrated_control/main_controller.h
    )
    
    # Ensure MOC is properly configured
    set_target_properties(main_controller PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
    )
    
    # Set environment variable for MOC (must be set before target creation)
    set_target_properties(main_controller PROPERTIES
        AUTOMOC_ENVIRONMENT "QTFRAMEWORK_BYPASS_LICENSE_CHECK=1"
    )
    
    # Explicitly tell CMake to process the header file for MOC
    # This ensures the header is scanned for Q_OBJECT
    set_property(TARGET main_controller PROPERTY AUTOMOC_SOURCE_EXTENSIONS "h;hpp;hxx;hh")
    
    target_link_libraries(main_controller PRIVATE
        ${QT_LIBRARIES}
        common_lib
    )
else()
    message(STATUS "Qt not found - skipping GUI build")
endif()

# Print configuration summary
message(STATUS "=== Configuration Summary ===")
message(STATUS "Tango found: ${TANGO_FOUND}")
if(TANGO_FOUND)
    message(STATUS "Tango include dirs: ${TANGO_INCLUDE_DIRS}")
    message(STATUS "Tango libraries: ${TANGO_LIBRARIES}")
    message(STATUS "Building device servers: large_stroke_server, auxiliary_support_server, six_dof_server, vacuum_server, interlock_server, reflection_imaging_server")
else()
    message(STATUS "Building Tango servers: NO (Tango not found)")
endif()
message(STATUS "Qt5 found: ${Qt5_FOUND}")
if(Qt5_FOUND)
    message(STATUS "Building main_controller: YES")
else()
    message(STATUS "Building GUI: NO (Qt not found)")
endif()
message(STATUS "=============================")
