#pragma once
#include <poll.h>
#include <string>
#include <vector>
#include <netinet/in.h>

class Client {
private:
	std::string mUsername;
	std::vector<std::string> mRoomNames;

	int mSignalFD;
	int mClientFD;
	struct sockaddr_in mServerAddr;

	bool mIsRunning;
	std::string mInputBuffer;
	std::string mReceiveBuffer;

	pollfd mPollEvents[3];

public:
	Client(std::string username, std::vector<std::string> roomNames);
	~Client();

	Client() = delete;
	Client(const Client&) = delete;
	Client(const Client&&) = delete;

	Client& operator=(const Client&) = delete;
	Client& operator=(const Client&&) = delete;

	void setupConnection(std::string host, int port);
	void connectToServer();
private:
	void run();

	void handleUserInput();
	void handleServerMessage();
	void handleSignal();

	void sendMessage(const std::string& message);
};