set(CONTROLLER_PROTO_PATH "${PROJECT_SOURCE_DIR}/third_party/deep_agent_payload/agent/protobuf")
set(CONTROLLER_GENERATED_PATH "${CMAKE_BINARY_DIR}/generated/")

add_library(proto-objects OBJECT 
    "${CONTROLLER_PROTO_PATH}/pbentity/do_agent.proto"
    "${CONTROLLER_PROTO_PATH}/grpc/agent/v1/controller.proto"
)

target_link_libraries(proto-objects PUBLIC protobuf::libprotobuf)
# target_include_directories(proto-objects PUBLIC "${CONTROLLER_GENERATED_PATH}")

protobuf_generate(TARGET proto-objects IMPORT_DIRS "${CONTROLLER_PROTO_PATH}" PROTOC_OUT_DIR "${CONTROLLER_GENERATED_PATH}")

add_library(grpc-objects OBJECT "${CONTROLLER_PROTO_PATH}/grpc/agent/v1/controller.proto")

protobuf_generate(
    TARGET grpc-objects
    LANGUAGE grpc
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
    PLUGIN "protoc-gen-grpc=\$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
    IMPORT_DIRS ${CONTROLLER_PROTO_PATH}
    PROTOC_OUT_DIR "${CONTROLLER_GENERATED_PATH}"
)

add_library(
    controller_config_proto
    "${CONTROLLER_GENERATED_PATH}/grpc/agent/v1/controller.pb.cc"
    "${CONTROLLER_GENERATED_PATH}/grpc/agent/v1/controller.grpc.pb.cc"
    "${CONTROLLER_GENERATED_PATH}/pbentity/do_agent.pb.cc"
)

target_include_directories(controller_config_proto PUBLIC "${CONTROLLER_GENERATED_PATH}")
target_link_libraries(controller_config_proto PUBLIC protobuf::libprotobuf gRPC::grpc++)
