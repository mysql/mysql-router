set(protoc_files
  ${protobuf_source_dir}/src/google/protobuf/compiler/main.cc
)

add_executable(protoc_3_0 ${protoc_files})
set_target_properties(protoc_3_0 PROPERTIES
  OUTPUT_NAME protoc)
target_link_libraries(protoc_3_0 libprotobuf_3_0 libprotoc_3_0)
