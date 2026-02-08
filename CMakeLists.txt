cmake_minimum_required (VERSION 3.10)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project ("chat_rooms")

add_subdirectory ("server")
add_subdirectory ("client")
