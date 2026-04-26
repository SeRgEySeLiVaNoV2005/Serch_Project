cmake_minimum_required(VERSION 3.10)
project(SearchEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/h)

find_package(PostgreSQL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system locale thread)
find_package(OpenSSL REQUIRED)

find_package(libpqxx CONFIG REQUIRED)

add_executable(search_engine 
    main.cpp 
    cpp/Spider.cpp 
    cpp/Database.cpp 
    cpp/Config.cpp 
    cpp/Indexer.cpp
    cpp/SearchServer.cpp
)

target_link_libraries(search_engine 
    PRIVATE
    ${PostgreSQL_LIBRARIES} 
    libpqxx::pqxx         
    Boost::system 
    Boost::locale 
    Boost::thread
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_include_directories(search_engine PRIVATE 
    ${PostgreSQL_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/html DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config.ini")
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/config.ini DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()