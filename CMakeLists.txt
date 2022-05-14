cmake_minimum_required(VERSION 3.4 FATAL_ERROR)

project(Encryption VERSION 1.0 LANGUAGES CXX)

add_subdirectory(td)

add_executable(encryption main.cpp client.cpp symmetry_encryption.cpp)
target_link_libraries(encryption PRIVATE Td::TdStatic)
set_property(TARGET encryption PROPERTY CXX_STANDARD 14)