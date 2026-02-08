#include "client.h"

#include <thread>
#include <fcntl.h>
#include <signal.h>
#include <iostream>
#include <unistd.h>
#include <stdexcept>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <system_error>
#include <sys/signalfd.h>

using std::cin;
using std::cout;
using std::endl;
using std::flush;

using std::string;
using std::vector;

using std::system_error;
using std::runtime_error;
using std::invalid_argument;
using std::generic_category;

Client::Client(string username, vector<string> roomNames)
    : mSignalFD(-1), mUsername(username), mRoomNames(roomNames), 
    mIsRunning(false), mInputBuffer(""), mReceiveBuffer("") {
    for (pollfd& p : mPollEvents) {
        p.fd = -1;
        p.events = 0;
        p.revents = 0;
    }
}

Client::~Client() {
    if (mClientFD != -1) {
        close(mClientFD);
    }
    if (mSignalFD != -1) {
        close(mSignalFD);
    }
}

void Client::setupConnection(string host, int port) {
    // Setup server address struct
    mServerAddr.sin_family = AF_INET;
    mServerAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &mServerAddr.sin_addr) == 0) {
        throw invalid_argument("Invalid host IP address");
    }

    // Create socket
    mClientFD = socket(AF_INET, SOCK_STREAM, 0);
    if (mClientFD == -1) {
        throw system_error(errno, generic_category(), "Failed socket creation");
    }
}

void Client::connectToServer() {
	// Connect to the Server
    if (connect(mClientFD, (sockaddr*)&mServerAddr, sizeof(mServerAddr)) == -1) {
		throw system_error(errno, generic_category(), "Failed to connect to Server");
	}

    // Send connection message
    string message;
    message.append(mUsername);
    for (string roomName : mRoomNames) {
        message.append("|").append(roomName);
    }
	message.append("\n");
	sendMessage(message);

    run();
}

void Client::run() {
    // Block signals for manual handling
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, nullptr);

    mSignalFD = signalfd(-1, &mask, 0);
    if (mSignalFD == -1) {
        throw system_error(errno, generic_category(), "Failed to hijack signal handling");
    }

	// Setup poll fds
    mPollEvents[0] = { STDIN_FILENO, POLLIN, 0 };
    mPollEvents[1] = { mClientFD, POLLIN, 0 };
	mPollEvents[2] = { mSignalFD, POLLIN, 0 };

    mIsRunning = true;
    while (mIsRunning) {
        cout << "\r\033[2K> " << mInputBuffer << flush;
        poll(mPollEvents, 3, -1);

        if (mPollEvents[0].revents & POLLIN) {
			handleUserInput();
        }
        if (mPollEvents[1].revents & POLLIN) {
            handleServerMessage();
            cout << "> " << mInputBuffer << flush;
        }
		if (mPollEvents[2].revents & POLLIN) {
            handleSignal();
        }
    }
}

void Client::handleUserInput() {
    char c;
    ssize_t n = read(mPollEvents[0].fd, &c, 1);
    if (n == 0) {
        return;
    }
    if (n == -1) {
        throw system_error(errno, generic_category(), "Failed to read user input");
    }

    if (c == '\n') {
        mInputBuffer += c;
        sendMessage(mInputBuffer);
        cout << "\r\033[2K" << "Sent: " << mInputBuffer;
        mInputBuffer.clear();
    }
    else if (c == 127 || c == 8) {
        if (!mInputBuffer.empty()) mInputBuffer.pop_back();
    }
    else {
        mInputBuffer += c;
    }
}

void Client::handleServerMessage() {
    char buf[512];

    ssize_t n = recv(mClientFD, buf, sizeof(buf), 0);

    if (n > 0) {
        mReceiveBuffer.append(buf, n);

        size_t pos;
        while ((pos = mReceiveBuffer.find('\n')) != string::npos) {
            string message = mReceiveBuffer.substr(0, pos);
            mReceiveBuffer.erase(0, pos + 1);

            cout << "\r\033[2K" << message << endl;
        }
    }
    else if (n == 0) {
        mIsRunning = false;
        return;
    }
    else {
        switch (errno) {
        case EPIPE:
        case ECONNRESET:
            mIsRunning = false;
            return;
        default:
            throw system_error(errno, std::generic_category(), "Failed to receive message");
        }
    }
}

void Client::handleSignal() {
    struct signalfd_siginfo fdsi;
    ssize_t s = read(mSignalFD, &fdsi, sizeof(fdsi));
    if (s != sizeof(fdsi)) {
        throw system_error(errno, generic_category(), "Failed to read signal info");
    }

    switch (fdsi.ssi_signo) {
        case SIGTERM:
        case SIGINT:
            mIsRunning = false;
            break;
        default:
            break;
    }
}

void Client::sendMessage(const string& message) {
    ssize_t totalSent = 0;
	while (totalSent < message.length()) {
        ssize_t sent = write(mClientFD, message.c_str() + totalSent, message.length() - totalSent);
        if (sent > 0) {
            totalSent += sent;
        }
        else if (sent == 0) {
            mIsRunning = false;
            return;
        }
        else { 
            switch (errno) {
            case EPIPE:
            case ECONNRESET:
				mIsRunning = false;
                return;
            default:
                throw system_error(errno, std::generic_category(), "Failed to send message");
            }
        }
    }
}