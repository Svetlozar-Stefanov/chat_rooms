#pragma once
#include <vector>
#include <string>
#include <sys/poll.h>
#include <unordered_map>

#define SERVER_PORT 8080
#define MAX_CONNECTIONS 5

class Server {
private:
	int mServerFD;

	std::unordered_map<int, std::string> mReceiveBuffer;

	bool mIsRunning;
	std::vector<pollfd> mPollEvents;

public:
	Server();
	~Server();

	Server(const Server&) = delete;
	Server(const Server&&) = delete;
	Server& operator=(const Server&) = delete;
	Server& operator=(const Server&&) = delete;

	void setup();
	void start();

private:
	void handleNewConnection();
	void receiveClientMessage(int clientFD, std::vector <std::string>& messages);
};

