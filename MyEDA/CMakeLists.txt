cmake_minimum_required(VERSION 3.20)
project(MyEDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

# ================================================================
# 源码按"模块"分目录,对应小组分工(详见 docs/课程资料/分工与答辩手册.md):
#   core/   数据模型(共同语言)        —— 全组共用,由 B 维护
#   ui/     主窗口与对话框              —— A 负责
#   editor/ 绘图区与编辑交互            —— B 负责
#   file/   存档 / 网表 / KiCad 导出     —— C 负责
#   sim/    仿真引擎(纯函数)          —— D 负责
#   test/   单元测试                    —— D 负责
# include 路径以 src/ 为根,所以写 "core/Component.h" 这样一眼知道来自哪层
# ================================================================

# WIN32 表示 GUI 子系统程序（入口 WinMain），否则按控制台程序链接会报“无法解析 main”
add_executable(MyEDA WIN32
    src/ui/MyApp.cpp
    src/ui/MyFrame.cpp
    src/editor/DrawingCanvas.cpp
)

# 让 #include "core/xxx.h" 这类写法的根目录是 src/
target_include_directories(MyEDA PRIVATE src)

target_link_libraries(MyEDA PRIVATE ${wxWidgets_LIBRARIES})

# 源码是 UTF-8(无BOM),MSVC 默认按 GBK 解析会报错;只用 /source-charset 解析源码,
# 字符串仍按系统编码(GBK)输出,wxString 才能正确显示中文。不能用 /utf-8(会导致界面中文乱码)
if(MSVC)
    target_compile_options(MyEDA PRIVATE "/source-charset:utf-8")
endif()

# 把 res 文件夹复制到可执行文件旁边
add_custom_command(TARGET MyEDA POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/src/res
    $<TARGET_FILE_DIR:MyEDA>/res
)

# 仿真引擎单元测试(不弹窗口):cmake --build build --target test_sim --config Debug
add_executable(test_sim EXCLUDE_FROM_ALL test/test_sim.cpp)
target_include_directories(test_sim PRIVATE src)
target_link_libraries(test_sim PRIVATE ${wxWidgets_LIBRARIES})
target_compile_options(test_sim PRIVATE "/source-charset:utf-8")

# Interactive paint regression checks: run on an unlocked Windows desktop.
add_executable(test_canvas EXCLUDE_FROM_ALL test/test_canvas.cpp src/editor/DrawingCanvas.cpp)
target_include_directories(test_canvas PRIVATE src)
target_link_libraries(test_canvas PRIVATE ${wxWidgets_LIBRARIES})
if(MSVC)
    target_compile_options(test_canvas PRIVATE "/source-charset:utf-8")
endif()

# Hierarchical subcircuit behavior tests.
add_executable(test_modules EXCLUDE_FROM_ALL test/test_modules.cpp)
target_include_directories(test_modules PRIVATE src)
target_link_libraries(test_modules PRIVATE ${wxWidgets_LIBRARIES})
if(MSVC)
    target_compile_options(test_modules PRIVATE "/source-charset:utf-8")
endif()