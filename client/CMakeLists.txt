include_directories ("include")

add_executable (chat_client "chat_client.cpp" "include/client.h" "src/client.cpp")

target_link_libraries(chat_client PRIVATE pthread)