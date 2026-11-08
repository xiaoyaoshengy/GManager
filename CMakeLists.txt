cmake_minimum_required(VERSION 3.12)
cmake_policy(SET CMP0087 NEW)
project(GManager VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# 在这里配置QT路径，例如：D:/Qt/6.6.2/msvc2019_64、
# D:/Qt/5.15.2/msvc2019_64、/home/liniyous/Qt/5.15.2/gcc_64
set(QT_SDK_DIR /data/xiaoyaosheny/Programs/qt/6.6.2/gcc_64
  CACHE PATH "QT SDK DIR" FORCE)
message(
  "在CMAKE中配置 QT_SDK_DIR 选项指定QT路径；"
  "当前路径: ${QT_SDK_DIR}"
)
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/Install/${QT_VERSION}
  CACHE PATH "Installation path" FORCE)

list(APPEND CMAKE_PREFIX_PATH ${QT_SDK_DIR})

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Sql Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Sql Network)

if(${QT_VERSION} VERSION_GREATER 6.7.0 OR ${QT_VERSION} VERSION_LESS 5.12.0)
  message(FATAL_ERROR
    "你当前使用的QT版本不在维护范围内！"
    "可能出现编译失败或使用时崩溃问题，"
    "推荐QT版本为QT5.15.2 QT6.6.2 QT6.6.3！请知悉~"
  )
  message(FATAL_ERROR
    "The QT version you are currently using is no longer under maintenance! "
    "There may be issues such as compilation failures or crashes during use. "
    "The recommended QT versions are QT5.15.2, QT6.6.2, and QT6.6.3! "
    "Please be advised"
  )
endif()

if(
  ${QT_VERSION} VERSION_GREATER_EQUAL 6.5.3
  AND
  ${QT_VERSION} VERSION_LESS_EQUAL 6.6.1
)
  message(FATAL_ERROR
    "你当前使用的QT版本为不推荐版本！"
    "可能出现使用时异常或崩溃问题，"
    "推荐QT版本为QT5.15.2 QT6.6.2 QT6.6.3！请知悉~"
  )
  message(FATAL_ERROR
    "The QT version you are currently using is not recommended! "
    "It may cause exceptions or crashes during operation. "
    "The recommended QT versions are QT5.15.2, QT6.6.2, and QT6.6.3! "
    "Please be advised"
  )
endif()

if(NOT WIN32)
  add_link_options(-Wl,--disable-new-dtags)
  set(CMAKE_SKIP_INSTALL_RPATH FALSE)
  set(CMAKE_INSTALL_RPATH
    "${QT_SDK_DIR}/lib:${CMAKE_INSTALL_PREFIX}/ElaWidgetTools/lib"
  )
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

