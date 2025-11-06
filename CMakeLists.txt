cmake_minimum_required(VERSION 3.16)
project(Zongshe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(wxWidgets REQUIRED COMPONENTS core base adv propgrid)
include(${wxWidgets_USE_FILE})

set(SOURCES
    App.cpp
    BoardModel.cpp
    BoardSerializer.cpp
    CommandManager.cpp
    Commands.cpp
    DrawBoard.cpp
    MainFrame.cpp
    PropertyPane.cpp
    ResourceManager.cpp
    json/jsoncpp.cpp
)

add_executable(Zongshe WIN32 MACOSX_BUNDLE ${SOURCES})

target_include_directories(Zongshe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/json)

target_link_libraries(Zongshe PRIVATE ${wxWidgets_LIBRARIES})

target_compile_definitions(Zongshe PRIVATE _CRT_SECURE_NO_WARNINGS)
