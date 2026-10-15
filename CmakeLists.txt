cmake_minimum_required(VERSION 3.10)
project(ue4cheat)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Наша библиотека
add_library(cheat SHARED libcheat.cpp)

# Системные библиотеки Android
find_library(log-lib log)
find_library(dl-lib dl)

target_link_libraries(cheat
    ${log-lib}
    ${dl-lib}
)
