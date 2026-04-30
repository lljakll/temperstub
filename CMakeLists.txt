cmake_minimum_required(VERSION 3.16)
project(TemperStub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql)

add_executable(TemperStub
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/dbmanager.cpp
    src/dbmanager.h
)

target_link_libraries(TemperStub PRIVATE Qt6::Widgets Qt6::Sql)