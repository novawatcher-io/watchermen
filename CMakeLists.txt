cmake_minimum_required(VERSION 3.16)

set(APP_NAME watchermen)
project(${APP_NAME})

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 17)

# Get the current Git short hash
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE)
add_definitions(-DGIT_HASH="${GIT_HASH}")

set(watchermen_ABSL_PROVIDER
    "package"
    CACHE STRING "Provider of Abseil-cpp library")
set_property(CACHE watchermen_ABSL_PROVIDER PROPERTY STRINGS "module" "package")

set(watchermen_PROTOBUF_PROVIDER
    "package"
    CACHE STRING "Provider of Protobuf library")
set_property(CACHE watchermen_PROTOBUF_PROVIDER PROPERTY STRINGS "module" "package")

set(watchermen_FMT_PROVIDER
    "package"
    CACHE STRING "Provider of fmt library")
set_property(CACHE watchermen_FMT_PROVIDER PROPERTY STRINGS "module" "package")

set(watchermen_GRPC_PROVIDER
    "package"
    CACHE STRING "Provider of gRPC library")
set_property(CACHE watchermen_GRPC_PROVIDER PROPERTY STRINGS "module" "package")

set(watchermen_LIBEVENT_PROVIDER
    "package"
    CACHE STRING "Provider of libevent library")
set_property(CACHE watchermen_LIBEVENT_PROVIDER PROPERTY STRINGS "module" "package")

set(watchermen_NLOHMANN_PROVIDER
    "package"
    CACHE STRING "Provider of nlohmann library")
set_property(CACHE watchermen_NLOHMANN_PROVIDER PROPERTY STRINGS "module" "package")

set(watchermen_PROMETHEUS_CPP_PROVIDER
    "package"
    CACHE STRING "Provider of prometheus-cpp library")
set_property(CACHE watchermen_PROMETHEUS_CPP_PROVIDER PROPERTY STRINGS "module" "package")

set(watchermen_SPDLOG_PROVIDER
    "package"
    CACHE STRING "Provider of spdlog library")
set_property(CACHE watchermen_SPDLOG_PROVIDER PROPERTY STRINGS "module" "package")
add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

set(SANITIZER_TYPE
    "address"
    CACHE STRING "Choose the type of sanitizer: address or thread")
set_property(CACHE SANITIZER_TYPE PROPERTY STRINGS "address" "thread")

# handle build type
if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-DUSE_JEMALLOC)
    add_compile_options(-O3 -DNDEBUG)
else()
    add_definitions(-DUSE_DEBUG)
    add_compile_options(-Wall -Wextra -g -O0)
    if(SANITIZER_TYPE STREQUAL "address")
        add_compile_options(-fsanitize=address)
        add_link_options(-fsanitize=address)
    elseif(SANITIZER_TYPE STREQUAL "thread")
        add_compile_options(-fsanitize=thread -fPIE -pie)
        add_link_options(-fsanitize=thread -fPIE -pie)
    endif()
endif()

if(CMAKE_COMPILER_IS_GNUCC)
    add_compile_options("-pthread")
endif()

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(watchermen_INSTALL ON)

# handle openssl
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# handle libcgroup by pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCGRP REQUIRED libcgroup)
include_directories(${LIBCGRP_INCLUDE_DIRS})
# message(STATUS "LIBCGRP_LIBRARIES ${LIBCGRP_LIBRARIES} ${LIBCGRP_LINK_LIBRARIES}")

# handle jemalloc by pkg-config
pkg_check_modules(LIBJEMALLOC REQUIRED jemalloc)
include_directories(${LIBJEMALLOC_INCLUDE_DIRS})
# message(STATUS "LIBJEMALLOC_LIBRARIES ${LIBJEMALLOC_LIBRARIES} ${LIBJEMALLOC_LINK_LIBRARIES}")

# handle cmake-based dependencies
include(cmake/import_dependency.cmake)
include(cmake/abseil-cpp.cmake)

if(${watchermen_PROTOBUF_PROVIDER} STREQUAL package)
    find_package(utf8_range CONFIG REQUIRED) # protobuf use utf8_range
endif()
include(cmake/protobuf.cmake)

process_provider(fmt ${watchermen_FMT_PROVIDER})
process_provider(grpc ${watchermen_GRPC_PROVIDER})
process_provider(libevent ${watchermen_LIBEVENT_PROVIDER})
process_provider(nlohmann ${watchermen_NLOHMANN_PROVIDER})
process_provider(prometheus-cpp ${watchermen_PROMETHEUS_CPP_PROVIDER})
process_provider(spdlog ${watchermen_SPDLOG_PROVIDER})
add_subdirectory(third_party/libcore)

include_directories(${CMAKE_CURRENT_LIST_DIR})

include_directories(src/app/include)
include(cmake/config.cmake)
include_directories(${CMAKE_BINARY_DIR}/generated/)
message("GENERATED_PROTOBUF_PATH:${GENERATED_PROTOBUF_PATH}")

file(GLOB_RECURSE APP_SRCS "src/app/*.cc")
file(GLOB_RECURSE APP_C_SRCS "src/app/*.c")
add_subdirectory(src)

add_executable(${APP_NAME} main.cc ${APP_SRCS} ${APP_C_SRCS})
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_options(${APP_NAME} PRIVATE -static-libgcc -static-libstdc++)
endif()

set(ABSEL_LIBARARY
    absl::log_internal_message
    absl::log_internal_check_impl
    absl::cord_internal
    absl::cord
    absl::flat_hash_set
    absl::status
    absl::statusor
    absl::flags
    absl::flags_parse)

include(cmake/controller.cmake)

if(libcore_LIBEVENT_PROVIDER STREQUAL "module")
    set(LIBEVENT_LINK_LIBRARIES event_core event_pthreads event_extra)
else()
    set(LIBEVENT_LINK_LIBRARIES libevent::core libevent::extra libevent::pthreads)
endif()

target_link_libraries(
    ${APP_NAME}
    ${THIRD_LIBRIRES}
    ${OPENSSL_LIBRARIES}
    protobuf::libprotobuf
    systemd
    gRPC::grpc++
    ${ABSEL_LIBARARY}
    ${LIBCGRP_LINK_LIBRARIES}
    ${LIBEVENT_LINK_LIBRARIES}
    manager_config_proto
    controller_config_proto
    nlohmann_json::nlohmann_json
    utf8_range::utf8_range
    utf8_range::utf8_validity
    spdlog::spdlog
    core)
