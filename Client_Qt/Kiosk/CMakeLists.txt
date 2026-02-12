cmake_minimum_required(VERSION 3.19)
project(Kiosk LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network Multimedia)

qt_standard_project_setup()

qt_add_executable(Kiosk
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

target_link_libraries(Kiosk
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt6::Network
        Qt6::Multimedia
)

include(GNUInstallDirs)

install(TARGETS Kiosk
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Kiosk
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
