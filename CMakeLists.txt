cmake_minimum_required(VERSION 3.10)

# Nama proyek
project(MyProject)

# Set standar C++ yang digunakan
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Tambahkan file sumber
set(SOURCE_FILES
    main.cpp
    node.cpp
    segment_handler.cpp
    segment.cpp
    socket.cpp
)

# Tambahkan executable
add_executable(node ${SOURCE_FILES})
