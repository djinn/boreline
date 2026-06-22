cmake_minimum_required(VERSION 3.28)
project(boreline VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Options
option(BORELINE_BUILD_TESTS "Build tests" ON)
option(BORELINE_BUILD_FUZZ "Build fuzz targets" OFF)
option(BORELINE_BUILD_SHARED "Build shared library" ON)
option(BORELINE_WITH_ASAN "Enable AddressSanitizer" OFF)
option(BORELINE_WITH_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(BORELINE_WITH_TSAN "Enable ThreadSanitizer" OFF)
option(BORELINE_ENABLE_COVERAGE "Enable coverage" OFF)
option(BORELINE_INSECURE_BYPASS "Allow insecure TLS bypass (test builds only)" OFF)

# Compiler setup
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
    add_compile_options(-Wstrict-prototypes -Wmissing-prototypes)
    add_compile_options(-Wshadow -Wpointer-arith -Wcast-align)
    add_compile_options(-Wwrite-strings -Wundef -Wunreachable-code)
    add_compile_options(-Wformat=2 -Wnull-dereference)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-Wjump-misses-init -Wlogical-op)
    endif()
endif()

# Sanitizers
if(BORELINE_WITH_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()
if(BORELINE_WITH_UBSAN)
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=undefined)
endif()
if(BORELINE_WITH_TSAN)
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
endif()
if(BORELINE_ENABLE_COVERAGE)
    add_compile_options(--coverage)
    add_link_options(--coverage)
endif()

# Dependencies
find_package(OpenSSL 3 REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${OPENSSL_INCLUDE_DIR}
)

# Core sources (portable shared code)
set(CORE_SOURCES
    src/common/protocol.c
    src/common/ringbuf.c
    src/common/stream.c
    src/common/log.c
    src/common/config.c
    src/common/timer.c
)

# Platform sources
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PLATFORM_SOURCES
        src/platform/linux/epoll.c
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(PLATFORM_SOURCES
        src/platform/macos/kqueue.c
    )
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

# Shared library
if(BORELINE_BUILD_SHARED)
    add_library(boreline_core SHARED ${CORE_SOURCES} ${PLATFORM_SOURCES})
    target_link_libraries(boreline_core PRIVATE OpenSSL::Crypto OpenSSL::SSL)
    set_target_properties(boreline_core PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 0
    )
endif()

# Static library
add_library(boreline_static STATIC ${CORE_SOURCES} ${PLATFORM_SOURCES})
target_link_libraries(boreline_static PUBLIC OpenSSL::Crypto OpenSSL::SSL)

# Server executable
add_executable(boreline-server
    src/server/main.c
    src/server/listener.c
    src/server/session.c
    src/server/routing.c
    src/server/admin.c
)
target_link_libraries(boreline-server PRIVATE boreline_static)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(boreline-server PRIVATE pthread rt)
endif()

# Client executable
add_executable(boreline
    src/client/main.c
    src/client/tunnel.c
    src/client/connection.c
)
target_link_libraries(boreline PRIVATE boreline_static)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(boreline PRIVATE pthread rt)
endif()

# Tests
if(BORELINE_BUILD_TESTS)
    enable_testing()

    # Unit tests
    add_executable(test-protocol
        tests/unit/test_protocol.c
    )
    target_link_libraries(test-protocol PRIVATE boreline_static)
    add_test(NAME test-protocol COMMAND test-protocol)

    add_executable(test-ringbuf
        tests/unit/test_ringbuf.c
    )
    target_link_libraries(test-ringbuf PRIVATE boreline_static)
    add_test(NAME test-ringbuf COMMAND test-ringbuf)

    add_executable(test-stream
        tests/unit/test_stream.c
    )
    target_link_libraries(test-stream PRIVATE boreline_static)
    add_test(NAME test-stream COMMAND test-stream)

    add_executable(test-config
        tests/unit/test_config.c
    )
    target_link_libraries(test-config PRIVATE boreline_static)
    add_test(NAME test-config COMMAND test-config)

    add_executable(test-http
        tests/unit/test_http.c
    )
    target_link_libraries(test-http PRIVATE boreline_static)
    add_test(NAME test-http COMMAND test-http)

    # Integration tests
    add_executable(test-e2e
        tests/integration/test_e2e.c
        src/server/listener.c
        src/server/session.c
        src/server/routing.c
        src/server/admin.c
    )
    target_include_directories(test-e2e PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test-e2e PRIVATE boreline_static)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(test-e2e PRIVATE pthread rt)
    endif()
    add_test(NAME test-e2e COMMAND test-e2e)

    # Fuzz targets
    if(BORELINE_BUILD_FUZZ)
        add_executable(fuzz-protocol
            tests/fuzz/fuzz_protocol.c
        )
        target_link_libraries(fuzz-protocol PRIVATE boreline_static)
    endif()
endif()

# Installation
install(TARGETS boreline-server boreline_static boreline_core
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
install(DIRECTORY include/boreline DESTINATION include)
install(DIRECTORY examples/ DESTINATION share/boreline/examples)
install(DIRECTORY docs/ DESTINATION share/boreline/docs)
install(FILES LICENSE README.md SECURITY.md CONTRIBUTING.md DESTINATION share/boreline)