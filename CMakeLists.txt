cmake_minimum_required(VERSION 3.16)
project(dtk-iptv VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia)
find_package(Dtk6 REQUIRED COMPONENTS Core Gui Widget)

add_executable(dtk-iptv
    src/main.cpp
    src/channel.h
    src/m3uparser.h src/m3uparser.cpp
    src/catalog.h src/catalog.cpp
    src/playliststore.h src/playliststore.cpp
    src/channelmodel.h src/channelmodel.cpp
    src/videowidget.h src/videowidget.cpp
    src/streamrelay.h src/streamrelay.cpp
    src/channelscanner.h src/channelscanner.cpp
    src/settingsdialog.h src/settingsdialog.cpp
    src/mainwindow.h src/mainwindow.cpp
    resources/resources.qrc
)

target_compile_definitions(dtk-iptv PRIVATE APP_VERSION="${PROJECT_VERSION}")

target_link_libraries(dtk-iptv PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Multimedia
    Dtk6::Core Dtk6::Gui Dtk6::Widget
)

install(TARGETS dtk-iptv RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES resources/dtk-iptv.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES resources/dtk-iptv.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
