#include "server.h"

#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <system_error>
#include <netinet/in.h>
#include "chat_manager.h"

using std::endl;
using std::cerr;
using std::vector;
using std::string;
using std::stringstream;
using std::system_error;
using std::runtime_error;
using std::generic_category;

Server::Server() : mServerFD(-1), mIsRunning(false) {
}

Server::~Server() {
	for (pollfd fd : mPollEvents) {
		if (fd.fd != -1) {
			close(fd.fd);
		}
	}
}

void Server::setup() {
	mServerFD = socket(AF_INET, SOCK_STREAM, 0);
	if (mServerFD == -1) {
		throw system_error(errno, generic_category(), "Failed to create Server socket");
	}

	sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(SERVER_PORT);

	if (bind(mServerFD, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
		throw system_error(errno, generic_category(), "Failed to bind Server socket");
	}
}

void Server::start() {
	if (listen(mServerFD, MAX_CONNECTIONS) == -1) {
		throw system_error(errno, generic_category(), "Failed to bind Server socket");
	}

	mPollEvents.push_back({ mServerFD, POLLIN, 0 });

	ChatManager workerPool;
	mIsRunning = true;
	while (mIsRunning) {
		poll(mPollEvents.data(), mPollEvents.size(), -1);

		//Check for new client connection
		if (mPollEvents[0].revents & POLLIN) {
			try {
				handleNewConnection();
			}
			catch (const runtime_error& e) {
				cerr << "Error accepting new connection: " << e.what() << endl;
			}
		}

		//Check for incoming messages from clients
		for (size_t i = 1; i < mPollEvents.size();) {
			bool closeClient = false;

			if (mPollEvents[i].revents & POLLIN) {
				try {
					vector<string> messages;
					receiveClientMessage(mPollEvents[i].fd, messages);

					for (const string& message : messages) {
						workerPool.enqueueTask({ mPollEvents[i].fd, message, Task::Type::Message });
					}
				} catch (const runtime_error& e) {
					closeClient = true;
					cerr << "Client closed while transmitting message " << mPollEvents[i].fd << endl;
				}
			}

			if (mPollEvents[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
				closeClient = true;
			}

			if (closeClient) {
				workerPool.enqueueTask({ mPollEvents[i].fd, "", Task::Type::Close });
				close(mPollEvents[i].fd);
				mPollEvents.erase(mPollEvents.begin() + i);
				continue;
			}

			++i;
		}
	}
}

void Server::receiveClientMessage(int clientFD, vector<string>& messages) {
	char buf[512];

	ssize_t n = recv(clientFD, buf, sizeof(buf), 0);

	if (n > 0) {
		if (mReceiveBuffer.find(clientFD) == mReceiveBuffer.end()) {
			mReceiveBuffer[clientFD] = "";
		}
		mReceiveBuffer[clientFD].append(buf, n);

		size_t pos;
		while ((pos = mReceiveBuffer[clientFD].find('\n')) != string::npos) {
			string message = mReceiveBuffer[clientFD].substr(0, pos);
			mReceiveBuffer[clientFD].erase(0, pos + 1);

			messages.push_back(message);
		}
	}
	else if (n == 0) {
		throw runtime_error("Connection closed while sending");
	}
	else {
		switch (errno) {
		case EPIPE:
		case ECONNRESET:
			throw runtime_error("Peer disconnected (broken pipe)");
			return;
		default:
			throw system_error(errno, std::generic_category(), "Failed to receive message");
		}
	}
}

void Server::handleNewConnection() {
	int clientFD = accept(mServerFD, nullptr, nullptr);
	if (clientFD == -1) {
		throw runtime_error("Failed to accept new client connection");
	}

	mPollEvents.push_back({ clientFD, POLLIN, 0 });
}