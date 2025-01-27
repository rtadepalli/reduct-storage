set(PROTO_SPEC_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Generate protoc files
set(PROTOBUF_SPEC_FILES
        ${PROTO_SPEC_ROOT_DIR}/reduct/proto/api/auth.proto
        ${PROTO_SPEC_ROOT_DIR}/reduct/proto/api/bucket.proto
        ${PROTO_SPEC_ROOT_DIR}/reduct/proto/api/server.proto
        ${PROTO_SPEC_ROOT_DIR}/reduct/proto/api/entry.proto
        ${PROTO_SPEC_ROOT_DIR}/reduct/proto/storage/entry.proto
        )

set(PROTOBUF_FILES
        ${CMAKE_BINARY_DIR}/reduct/proto/api/auth.pb.cc
        ${CMAKE_BINARY_DIR}/reduct/proto/api/bucket.pb.cc
        ${CMAKE_BINARY_DIR}/reduct/proto/api/server.pb.cc
        ${CMAKE_BINARY_DIR}/reduct/proto/api/entry.pb.cc
        ${CMAKE_BINARY_DIR}/reduct/proto/storage/entry.pb.cc
        )


add_custom_command(OUTPUT ${PROTOBUF_FILES}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE} -I=${PROTO_SPEC_ROOT_DIR}/
        --cpp_out=${CMAKE_BINARY_DIR} ${PROTO_SPEC_ROOT_DIR}/reduct/proto/storage/*

        COMMAND ${Protobuf_PROTOC_EXECUTABLE} -I=${PROTO_SPEC_ROOT_DIR}/
        --cpp_out=${CMAKE_BINARY_DIR} ${PROTO_SPEC_ROOT_DIR}/reduct/proto/api/*

        DEPENDS ${PROTOBUF_SPEC_FILES}
        )


set(SRC_FILES
        reduct/api/bucket_api.cc
        reduct/api/console.cc
        reduct/api/entry_api.cc
        reduct/api/http_server.cc
        reduct/api/server_api.cc
        reduct/api/token_api.cc

        reduct/asset/asset_manager.cc
        reduct/async/loop.cc

        reduct/auth/policies.cc
        reduct/auth/token_auth.cc
        reduct/auth/token_repository.cc

        reduct/core/env_variable.cc
        reduct/core/logger.cc
        reduct/core/error.cc

        reduct/storage/io/async_reader.cc
        reduct/storage/io/async_writer.cc
        reduct/storage/bucket.cc
        reduct/storage/entry.cc
        reduct/storage/storage.cc
        reduct/storage/block_manager.cc)


add_library(reduct STATIC ${SRC_FILES} ${PROTOBUF_FILES})

if (MSVC)
    target_compile_options(reduct PUBLIC /Zc:preprocessor)
    target_compile_definitions(reduct PUBLIC NOMINMAX)
endif ()


configure_file(config.h.in ${CMAKE_BINARY_DIR}/reduct/config.h @ONLY)


if (WEB_CONSOLE_PATH)
    message(STATUS "Embedding Console from ${WEB_CONSOLE_PATH}")

    file(ARCHIVE_CREATE FORMAT zip OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/console.zip PATHS ${WEB_CONSOLE_PATH})
    file(READ ${CMAKE_CURRENT_BINARY_DIR}/console.zip ZIPPED_CONSOLE HEX)
    configure_file(console.h.in ${CMAKE_BINARY_DIR}/reduct/console.h @ONLY)
    execute_process(COMMAND python3 ${CMAKE_SOURCE_DIR}/cmake/split_string.py ${CMAKE_BINARY_DIR}/reduct/console.h)
    target_compile_definitions(reduct PUBLIC WITH_CONSOLE)
endif ()

add_executable(reduct-storage main.cc)
target_link_libraries(reduct-storage reduct ${CONAN_LIBS})  # For backward compatibility

add_executable(reductstore main.cc)
target_link_libraries(reductstore reduct ${CONAN_LIBS})
