set(PROTO_PATH "${PROJECT_SOURCE_DIR}/third_party/nova-agent-payload/agent/protobuf")
set(GENERATED_PROTOBUF_PATH "${CMAKE_BINARY_DIR}/generated/")

set(MANAGER_PROTO "${PROTO_PATH}/watchermen/v1/manager.proto")


set(MANAGER_PB_H_FILE "${GENERATED_PROTOBUF_PATH}/watchermen/v1/manager.pb.h")
set(MANAGER_PB_CPP_FILE
        "${GENERATED_PROTOBUF_PATH}/watchermen/v1/manager.pb.cc")

set(PROTOBUF_GENERATED_FILES
        ${MANAGER_PB_H_FILE}
        ${MANAGER_PB_CPP_FILE})

file(MAKE_DIRECTORY "${GENERATED_PROTOBUF_PATH}")


set(PROTOBUF_COMMON_FLAGS "--proto_path=${PROTO_PATH}"
        "--cpp_out=${GENERATED_PROTOBUF_PATH}")

# --experimental_allow_proto3_optional is available from 3.13 and be stable and
# enabled by default from 3.16
if(Protobuf_VERSION AND Protobuf_VERSION VERSION_LESS "3.16")
    list(APPEND PROTOBUF_COMMON_FLAGS "--experimental_allow_proto3_optional")
elseif(PROTOBUF_VERSION AND PROTOBUF_VERSION VERSION_LESS "3.16")
    list(APPEND PROTOBUF_COMMON_FLAGS "--experimental_allow_proto3_optional")
endif()



message("PROTOBUF_PROTOC_EXECUTABLE:${_watchermen_PROTOBUF_PROTOC_EXECUTABLE}")
add_custom_command(
        OUTPUT ${PROTOBUF_GENERATED_FILES}
        COMMAND
        ${_watchermen_PROTOBUF_PROTOC_EXECUTABLE} ${PROTOBUF_COMMON_FLAGS}
        ${PROTOBUF_INCLUDE_FLAGS} ${MANAGER_PROTO}
        COMMENT "[Run]: ${_watchermen_PROTOBUF_PROTOC_EXECUTABLE}")
include_directories(${GENERATED_PROTOBUF_PATH})
add_library(
        manager_config_proto
        ${PROTOBUF_GENERATED_FILES}
)
target_link_libraries(manager_config_proto
        protobuf::libprotobuf
)
