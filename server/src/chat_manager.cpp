#include "chat_manager.h"

#include <mutex>
#include <sstream>
#include <unistd.h>
#include <sys/socket.h>

#include <iostream>

using std::endl;
using std::cerr;
using std::mutex;
using std::vector;
using std::string;
using std::thread;
using std::shared_ptr;
using std::unique_ptr;
using std::unique_lock;
using std::system_error;
using std::stringstream;
using std::unordered_map;
using std::runtime_error;

ChatManager::ChatManager(size_t numThreads)
	: mStop(false) {
	for (size_t i = 0; i < numThreads; ++i) {
		mThreads.emplace_back(&ChatManager::workLoop, this);
	}
}

ChatManager::~ChatManager()
{
	{
		unique_lock<mutex> lock(mQueueMutex);
		mStop = true;
	}

	mCondition.notify_all();

	for (thread& thread : mThreads) {
		thread.join();
	}
}

void ChatManager::enqueueTask(Task task) {
	{
		unique_lock<mutex> lock(mQueueMutex);
		mTaskQueue.emplace(std::move(task));
	}
	mCondition.notify_one();
}

void ChatManager::workLoop() {
	while (true) {
		Task task;
		{
			unique_lock<mutex> lock(mQueueMutex);
			mCondition.wait(lock, [this] { return !mTaskQueue.empty() || mStop; });

			if (mStop) {
				return;
			}

			task = std::move(mTaskQueue.front());
			mTaskQueue.pop();
		}

		handleTask(task);
	}
}

void ChatManager::handleTask(const Task& task) {
	if (task.type == Task::Type::Message) {
		mDataMutex.lock_shared();
		if (mClients.find(task.clientFD) == mClients.end()) {
			mDataMutex.unlock_shared();
			handleNewClient(task);
		}
		else {
			mDataMutex.lock_shared();
			broadcastMessage(*mClients[task.clientFD], task.clientMessage);
			mDataMutex.unlock_shared();
		}
	}
	else if (task.type == Task::Type::Close) {
		handleClientDisconnect(task);
	}
}

void ChatManager::handleNewClient(const Task& task) {
	vector<string> arguments;
	parse(task.clientMessage, '|', arguments);

	Client* client = new Client(task.clientFD, arguments[0]);
	
	mDataMutex.lock();

	mClients.emplace(task.clientFD, client);

	for (size_t i = 1; i < arguments.size(); ++i) {
		if (mRooms.find(arguments[i]) == mRooms.end()) {
			mRooms.emplace(arguments[i], new Room(arguments[i]));
		}

		mClients[task.clientFD]->joinRoom(mRooms[arguments[i]]);
	}

	mDataMutex.unlock();

	mDataMutex.lock_shared();
	broadcastMessage(*mClients[task.clientFD], "joined the room.");
	mDataMutex.unlock_shared();
}

void ChatManager::handleClientDisconnect(const Task& task) {
	mDataMutex.lock();
	broadcastMessage(*mClients[task.clientFD], "left the room.");

	Client& client = *mClients[task.clientFD];
	for (size_t i = 0; i < client.rooms.size(); ++i) {
		client.rooms[i]->removeClient(task.clientFD);
		if (client.rooms[i]->isEmpty()) {
			mRooms.erase(client.rooms[i]->name);
		}
	}
	client.rooms.clear();
	mClients.erase(task.clientFD);

	mDataMutex.unlock();
}

void ChatManager::broadcastMessage(const Client& client, const string& message) const {
	for (size_t i = 0; i < client.rooms.size(); ++i) {
		for (int clientFD : client.rooms[i]->clientFDs) {
			if (clientFD != client.clientFD) {
				string fullMessage(client.rooms[i]->name);
				fullMessage.append(" : ")
					.append(client.username)
					.append(" : ")
					.append(message)
					.append("\n");
				try {
					sendMessage(clientFD, fullMessage);
				}
				catch (const runtime_error& e) {
					cerr << "Client closed during send " << clientFD << endl;
				}
			}
		}
	}
}

void ChatManager::sendMessage(int clientFD, const string& message) const {
	ssize_t totalSent = 0;
	while (totalSent < message.length()) {
		ssize_t sent = write(clientFD, message.c_str() + totalSent, message.length() - totalSent);
		if (sent > 0) {
			totalSent += sent;
		}
		else if (sent == 0) {
			throw runtime_error("Connection closed while sending");
		}
		else {
			switch (errno) {
			case EPIPE:
			case ECONNRESET:
				throw runtime_error("Peer disconnected (broken pipe)");
			default:
				throw system_error(errno, std::generic_category(), "Failed to send message");
			}
		}
	}
}

void ChatManager::parse(const string& message, char delimeter, vector<string>& tokens) {
	stringstream ss(message);
	string token;
	while (std::getline(ss, token, delimeter)) {
		tokens.emplace_back(token);
	}
}