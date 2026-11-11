cmake_minimum_required(VERSION 3.12)
cmake_policy(SET CMP0087 NEW)

# 版本号只在 utils/global_defines.h 的 kAppVersion 里定义一处，
# 这里解析出来给 project() 与打包脚本使用
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/utils/global_defines.h"
  GM_VERSION_LINE REGEX "kAppVersion")
string(REGEX MATCH "\"([0-9]+\\.[0-9]+\\.[0-9]+)\"" _gm_version "${GM_VERSION_LINE}")
set(GM_VERSION "${CMAKE_MATCH_1}")
if(NOT GM_VERSION)
  message(FATAL_ERROR
    "无法从 utils/global_defines.h 解析 kAppVersion（形如 \"1.0.0\"）")
endif()
message("GManager 版本（来自 utils/global_defines.h）: ${GM_VERSION}")

project(GManager VERSION ${GM_VERSION} LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# 在这里配置 QT 路径，例如：D:/Qt/6.8.3/msvc2019_64、
# /home/<user>/Qt/6.8.3/gcc_64；也可以用 -DQT_SDK_DIR=... 覆盖，
# 便于在 Windows / CI 上使用不同安装位置的 Qt
if(NOT DEFINED QT_SDK_DIR)
  set(QT_SDK_DIR /data/xiaoyaosheny/Programs/qt/6.8.3/gcc_64
    CACHE PATH "QT SDK DIR")
endif()
message(
  "在CMAKE中配置 QT_SDK_DIR 选项指定QT路径；"
  "当前路径: ${QT_SDK_DIR}"
)
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/Install/${QT_VERSION}
  CACHE PATH "Installation path" FORCE)

list(APPEND CMAKE_PREFIX_PATH ${QT_SDK_DIR})

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Sql Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Sql Network)

# 版本白名单：只接受 Qt 6.8.0 ~ 6.8.3。
# 6.5.x / 6.6.x 等旧版本缺少 Qt_6.8 符号，一旦与安装包自带的 6.8.3
# 运行库混用，启动时会因版本符号缺失而失败，因此在配置阶段就拦下。
if(${QT_VERSION} VERSION_GREATER 6.8.3 OR ${QT_VERSION} VERSION_LESS 6.8.0)
  message(FATAL_ERROR
    "当前仅在 Qt 6.8.3 上实测通过，请使用 Qt 6.8.3！"
  )
  message(FATAL_ERROR
    "Only Qt 6.8.3 has been verified by testing; please use Qt 6.8.3."
  )
endif()

if(NOT WIN32)
  set(CMAKE_SKIP_INSTALL_RPATH FALSE)
  # 安装树里可执行文件在 bin/、运行库在 ../lib/，用相对 $ORIGIN 的 RPATH，
  # 不写死构建机的 Qt 绝对路径。
  # 注意不要加 -Wl,--disable-new-dtags：旧式 DT_RPATH 的优先级高于
  # LD_LIBRARY_PATH，会让启动器指向的库失效；只有链接器默认的 DT_RUNPATH
  # 才能被 LD_LIBRARY_PATH 覆盖。
  set(CMAKE_INSTALL_RPATH "\$ORIGIN/../lib")
endif()

# 编译 ElaWidgetTools 库
add_subdirectory(third_party/ElaWidgetTools)

# 编译主程序
file(GLOB ORIGIN *.cpp *.h)
file(GLOB PAGES pages/*.h pages/*.cpp)
file(GLOB WIDGETS widgets/*.h widgets/*.cpp)
file(GLOB UTILS utils/*.h utils/*.cpp)
file(GLOB DATA data/*.h data/*.cpp)
file(GLOB SCRAPERS data/scrapers/*.h data/scrapers/*.cpp)

source_group(pages FILES ${PAGES})
source_group(widgets FILES ${WIDGETS})
source_group(utils FILES ${UTILS})
source_group(data FILES ${DATA})
source_group(scrapers FILES ${SCRAPERS})

set(PROJECT_SOURCES
    ${ORIGIN}
    ${PAGES}
    ${WIDGETS}
    ${UTILS}
    ${DATA}
    ${SCRAPERS}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
  qt_add_executable(${PROJECT_NAME}
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
  )
  # 遍历 assets 下的资源文件（避免把 build/third_party 的图片重复打包）
  file(GLOB_RECURSE RES_PATHS
    assets/*.png assets/*.jpg assets/*.svg assets/*.ico
    assets/*.ttf assets/*.webp assets/*.js
  )
  # assets/readme 是文档截图，只给 README 用，不打进程序资源
  list(FILTER RES_PATHS EXCLUDE REGEX "/assets/readme/")
  foreach(filepath ${RES_PATHS})
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" filename ${filepath})
    list(APPEND resource_files ${filename})
  endforeach()

  qt_add_resources(${PROJECT_NAME} "GManager"
    RESOURCES PREFIX "/"
    FILES
    ${resource_files}
  )
else()
  qt5_add_big_resources(PROJECT_SOURCES
    GManager.qrc
  )
  add_executable(${PROJECT_NAME}
    ${PROJECT_SOURCES}
  )
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PROJECT_NAME})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PROJECT_NAME})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC
    pages
    widgets
    utils
    data
    data/scrapers
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Network
    Ela::WidgetTools
)

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.${PROJECT_NAME})
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING
    ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}"
    VS_DEBUGGER_COMMAND
    "${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}/${PROJECT_NAME}.exe"
)

if(QT_VERSION_MAJOR EQUAL 6)
  qt_finalize_executable(${PROJECT_NAME})
endif()

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
  LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}
  RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}
)

install(TARGETS ElaWidgetTools
  LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}
  RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}
)

# 分发时必须一并提供许可证与第三方声明
install(FILES
  LICENSE
  LICENSE.GPL-3
  third_party/ElaWidgetTools/LICENSE
  DESTINATION ${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}/licenses
)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_SDK_DIR}/bin")
if(WIN32)
  install(CODE "
    if(
      EXISTS \"${WINDEPLOYQT_EXECUTABLE}\"
      AND
      NOT EXISTS \"${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}/platforms\"
    )
      execute_process(
        COMMAND \"${CMAKE_COMMAND}\" -E env PATH=\"${QT_SDK_DIR}/bin\"
        \"${WINDEPLOYQT_EXECUTABLE}\"
        \"$<TARGET_FILE_DIR:${PROJECT_NAME}>/$<TARGET_FILE_NAME:${PROJECT_NAME}>\"
        --dir \"${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}\"
        --no-compiler-runtime
        --no-system-d3d-compiler
        --no-opengl-sw
        --verbose 1
      )
    endif()"
  )
endif()

