
cmake_minimum_required (VERSION 3.8)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_FIND_LIBRARY_SUFFIXES .a .lib)
set(APR_LIB_PATH /usr/local/apr/lib)

include_directories(./inc)


add_executable (CCachePool 
    src/CCachePool.cpp
	src/CommonDecl.cpp
    src/disk.cpp
    src/hash_bucket.cpp
    src/memPool.cpp
    src/SimpleBufferPool.cpp
)

add_executable(
        example
        src/disk.cpp
        src/hash_bucket.cpp
        src/memPool.cpp
        src/SimpleBufferPool.cpp
        src/main.cpp
)

add_executable(
        testMemPool
        src/testMemPool.cpp
)

set(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -g ")


find_library(LIB_LOG4CXX log4cxx)
find_library(LIB_APR apr-1 PATHS ${APR_LIB_PATH})
find_library(LIB_APRUTIL aprutil-1 PATHS ${APR_LIB_PATH})


if(LIB_LOG4CXX AND LIB_APR AND LIB_APRUTIL)
target_link_libraries(CCachePool pthread expat ${LIB_LOG4CXX} ${LIB_APR} ${LIB_APRUTIL})
target_link_libraries(example pthread expat ${LIB_LOG4CXX} ${LIB_APR} ${LIB_APRUTIL})
else()
message(${Log4cxx_LIBRARY})
target_link_libraries(CCachePool pthread ${Log4cxx_LIBRARY})
target_link_libraries(example pthread ${Log4cxx_LIBRARY})
endif()


configure_file(conf/log4cxx.properties log4cxx.properties COPYONLY)