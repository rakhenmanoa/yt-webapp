cmake_minimum_required(VERSION 3.16)
project(yt-webapp VERSION 0.10.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Core DBus Gui Network Quick WebEngineCore WebEngineQuick)

add_executable(yt-webapp
  src/main.cpp
  src/backend.cpp
  src/backend.h
  src/interceptor.cpp
  src/interceptor.h
  src/mpris.cpp
  src/mpris.h
  resources.qrc
)

# Version affichée dans le menu : version + commit git compilé.
set(YTW_VERSION "${PROJECT_VERSION}")
find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(COMMAND ${GIT_EXECUTABLE} -C ${CMAKE_SOURCE_DIR} rev-parse --short HEAD
                  OUTPUT_VARIABLE YTW_GIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(YTW_GIT)
    set(YTW_VERSION "${YTW_VERSION} (${YTW_GIT})")
  endif()
endif()
target_compile_definitions(yt-webapp PRIVATE YTW_VERSION="${YTW_VERSION}")

target_link_libraries(yt-webapp PRIVATE
  Qt6::Core Qt6::DBus Qt6::Gui Qt6::Network Qt6::Quick Qt6::WebEngineCore Qt6::WebEngineQuick)

include(GNUInstallDirs)
install(TARGETS yt-webapp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/yt-webapp.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/yt-webapp.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
