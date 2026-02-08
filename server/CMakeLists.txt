include_directories("include")

add_executable (chat_server "chat_server.cpp" "src/server.cpp" "src/chat_manager.cpp" "include/server.h" "include/chat_manager.h" )

target_link_libraries(chat_server PRIVATE pthread)