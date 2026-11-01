cmake_minimum_required(VERSION 3.16)
project(yt-webapp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Network Quick WebEngineCore WebEngineQuick)

add_executable(yt-webapp
  src/main.cpp
  src/backend.cpp
  src/backend.h
  src/interceptor.cpp
  src/interceptor.h
  resources.qrc
)

target_link_libraries(yt-webapp PRIVATE
  Qt6::Core Qt6::Gui Qt6::Network Qt6::Quick Qt6::WebEngineCore Qt6::WebEngineQuick)

include(GNUInstallDirs)
install(TARGETS yt-webapp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/yt-webapp.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/yt-webapp.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
