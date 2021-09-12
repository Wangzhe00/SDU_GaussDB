
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_FIND_LIBRARY_SUFFIXES .a .lib)
set(APR_LIB_PATH /usr/local/apr/lib)

set(TARGET main)


add_executable(
        ${TARGET}
        src/disk.cpp
        src/hash_bucket.cpp
        src/memPool.cpp
        src/replacement.cpp
        src/main.cpp
)

set(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -g ")

include_directories(./inc)

find_library(LIB_LOG4CXX log4cxx)
find_library(LIB_APR apr-1 PATHS ${APR_LIB_PATH})
find_library(LIB_APRUTIL aprutil-1 PATHS ${APR_LIB_PATH})

target_link_libraries(${TARGET} pthread expat ${LIB_LOG4CXX} ${LIB_APR} ${LIB_APRUTIL})
