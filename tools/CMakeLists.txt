cmake_minimum_required(VERSION 3.21)
project(make_android_icons CXX)

find_package(Qt6 REQUIRED COMPONENTS Gui)

qt_add_executable(make_android_icons make_android_icons.cpp)
target_link_libraries(make_android_icons PRIVATE Qt6::Gui)