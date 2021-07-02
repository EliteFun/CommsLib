cmake_minimum_required(VERSION 3.8)
project(CommsLib LANGUAGES CXX)

set(SOURCE_FILES
    ${PROJECT_SOURCE_DIR}/src/Client.cpp
    ${PROJECT_SOURCE_DIR}/src/Server.cpp
)

set(HEADER_FILES
    ${PROJECT_SOURCE_DIR}/include/Client.hpp
    ${PROJECT_SOURCE_DIR}/include/Server.hpp
    ${PROJECT_SOURCE_DIR}/include/Message.hpp
)

add_library(CommsLib STATIC ${SOURCE_FILES} ${HEADER_FILES})
target_include_directories(CommsLib PUBLIC ${PROJECT_SOURCE_DIR}/include/)
target_compile_features(CommsLib PUBLIC cxx_std_17)

#CommsLib only supports windows for now, so I commented this for clarity
#if (UNIX)
#    target_link_libraries(CommsLib pthread)
#endif (UNIX)

#CommsLib client test
add_executable(CommsLibClientTest ${PROJECT_SOURCE_DIR}/tests/clientTest.cpp)
target_link_libraries(CommsLibClientTest CommsLib)

#CommsLib server test
add_executable(CommsLibServerTest ${PROJECT_SOURCE_DIR}/tests/serverTest.cpp)
target_link_libraries(CommsLibServerTest CommsLib)
