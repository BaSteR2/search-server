cmake_minimum_required(VERSION 3.11)

project(search_server CXX)
set(CMAKE_CXX_STANDARD 20)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(search_server
	src/main.cpp
	src/concurrent_map.h
	src/document.h
	src/document.cpp
	src/log_duration.h
	src/paginator.h
	src/process_queries.h
	src/read_input_functions.h
	src/read_input_functions.cpp
	src/remove_duplicates.cpp
	src/remove_duplicates.h
	src/request_queue.h
	src/request_queue.cpp
	src/search_server.h
	src/search_server.cpp
	src/string_processing.h
	src/string_processing.cpp
	src/test_example_functions.h
	src/test_example_functions.cpp
)



target_link_libraries(search_server PRIVATE Threads::Threads) 
