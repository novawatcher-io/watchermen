# Copyright 2017 watchermen authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
if(watchermen_PROTOBUF_PROVIDER STREQUAL "module")
    # Building the protobuf tests require gmock what is not part of a standard protobuf checkout.
    # Disable them unless they are explicitly requested from the cmake command line (when we assume
    # gmock is downloaded to the right location inside protobuf).
    if(NOT protobuf_BUILD_TESTS)
        set(protobuf_BUILD_TESTS OFF CACHE BOOL "Build protobuf tests")
    endif()
    # Disable building protobuf with zlib. Building protobuf with zlib breaks
    # the build if zlib is not installed on the system.
    if(NOT protobuf_WITH_ZLIB)
        set(protobuf_WITH_ZLIB OFF CACHE BOOL "Build protobuf with zlib.")
    endif()
    if(NOT PROTOBUF_ROOT_DIR)
        set(PROTOBUF_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/protobuf)
    endif()

    if(EXISTS "${PROTOBUF_ROOT_DIR}/CMakeLists.txt")
        set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "Link static runtime libraries")
        add_subdirectory(${PROTOBUF_ROOT_DIR} third_party/protobuf)
        if(TARGET ${_watchermen_PROTOBUF_LIBRARY_NAME})
            set(_watchermen_PROTOBUF_LIBRARIES ${_watchermen_PROTOBUF_LIBRARY_NAME})
        endif()
        if(TARGET libprotoc)
            set(_watchermen_PROTOBUF_PROTOC_LIBRARIES libprotoc)
        endif()
        if(TARGET protoc)
            set(_watchermen_PROTOBUF_PROTOC protoc)
            if(CMAKE_CROSSCOMPILING)
                find_program(_watchermen_PROTOBUF_PROTOC_EXECUTABLE protoc)
            else()
                set(_watchermen_PROTOBUF_PROTOC_EXECUTABLE $<TARGET_FILE:protobuf::protoc>)
            endif()
        endif()
        # For well-known .proto files distributed with protobuf
        set(_watchermen_PROTOBUF_WELLKNOWN_INCLUDE_DIR "${PROTOBUF_ROOT_DIR}/src")
    else()
        message(WARNING "watchermen_PROTOBUF_PROVIDER is \"module\" but PROTOBUF_ROOT_DIR is wrong")
    endif()
    if(watchermen_INSTALL AND NOT _watchermen_INSTALL_SUPPORTED_FROM_MODULE)
        message(WARNING "watchermen_INSTALL will be forced to FALSE because watchermen_PROTOBUF_PROVIDER is \"module\" and CMake version (${CMAKE_VERSION}) is less than 3.13.")
        set(watchermen_INSTALL FALSE)
    endif()
elseif(watchermen_PROTOBUF_PROVIDER STREQUAL "package")
    find_package(Protobuf REQUIRED CONFIG)
    if(Protobuf_FOUND OR PROTOBUF_FOUND)
        include_directories(${Protobuf_INCLUDE_DIRS})
        if(TARGET protobuf::${_watchermen_PROTOBUF_LIBRARY_NAME})
            set(_watchermen_PROTOBUF_LIBRARIES protobuf::${_watchermen_PROTOBUF_LIBRARY_NAME})
        else()
            set(_watchermen_PROTOBUF_LIBRARIES ${PROTOBUF_LIBRARIES})
        endif()
        if(TARGET protobuf::libprotoc)
            set(_watchermen_PROTOBUF_PROTOC_LIBRARIES protobuf::libprotoc)
            # extract the include dir from target's properties
            get_target_property(_watchermen_PROTOBUF_WELLKNOWN_INCLUDE_DIR protobuf::libprotoc INTERFACE_INCLUDE_DIRECTORIES)
        else()
            set(_watchermen_PROTOBUF_PROTOC_LIBRARIES ${PROTOBUF_PROTOC_LIBRARIES})
            set(_watchermen_PROTOBUF_WELLKNOWN_INCLUDE_DIR ${PROTOBUF_INCLUDE_DIRS})
        endif()
        if(TARGET protobuf::protoc)
            set(_watchermen_PROTOBUF_PROTOC protobuf::protoc)
            if(CMAKE_CROSSCOMPILING)
                find_program(_watchermen_PROTOBUF_PROTOC_EXECUTABLE protoc)
            else()
                set(_watchermen_PROTOBUF_PROTOC_EXECUTABLE $<TARGET_FILE:protobuf::protoc>)
            endif()
        else()
            set(_watchermen_PROTOBUF_PROTOC ${PROTOBUF_PROTOC_EXECUTABLE})
            if(CMAKE_CROSSCOMPILING)
                find_program(_watchermen_PROTOBUF_PROTOC_EXECUTABLE protoc)
            else()
                set(_watchermen_PROTOBUF_PROTOC_EXECUTABLE ${PROTOBUF_PROTOC_EXECUTABLE})
            endif()
        endif()
        set(_watchermen_FIND_PROTOBUF "include(CMakeFindDependencyMacro)\nfind_dependency(Protobuf CONFIG)")
    endif()
endif()