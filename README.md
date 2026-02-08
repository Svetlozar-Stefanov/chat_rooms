## Building the Project

### Method 1: Manual Build with g++

**Build Server:**
```bash
g++ -std=c++11 -pthread \
    server/chat_server.cpp \
    server/src/server.cpp \
    server/src/chat_manager.cpp \
    -Iserver/include \
    -o chat_server
```

**Build Client:**
```bash
g++ -std=c++11 -pthread \
    client/chat_client.cpp \
    client/src/client.cpp \
    -Iclient/include \
    -o chat_client
```

### Method 2: CMake Build

**Build:**
```bash
mkdir build
cd build
cmake ..
make
```
