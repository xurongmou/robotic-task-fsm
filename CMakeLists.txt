cmake_minimum_required(VERSION 3.10)
project(FSM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# 选项：编译为共享库或静态库（使用 -DBUILD_SHARED_LIBS=ON 切换）2
option(BUILD_SHARED_LIBS "Build FSM as a shared library" OFF)

# 源文件与库
file(GLOB_RECURSE FSM_SOURCES "src/*.cpp")

add_library(fsm ${FSM_SOURCES})
# 添加测试程序
add_executable(fsm_test test/test.cpp)

# 链接 fsm 库
target_link_libraries(fsm_test PRIVATE fsm)

# 公开 include 目录
target_include_directories(fsm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# 添加警告或编译选项（示例）
if(MSVC)
    target_compile_options(fsm PRIVATE /W4 /WX)
else()
    target_compile_options(fsm PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# 可选：安装规则（简要）
install(TARGETS fsm
    EXPORT fsmTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)

# 导出 CMake 包配置（可选）
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/fsmConfigVersion.cmake"
    VERSION 0.1
    COMPATIBILITY AnyNewerVersion
)
install(EXPORT fsmTargets FILE fsmTargets.cmake NAMESPACE fsm:: DESTINATION lib/cmake/fsm)