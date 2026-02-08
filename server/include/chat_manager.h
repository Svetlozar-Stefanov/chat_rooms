#pragma once

#include <queue>
#include <thread>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <unordered_map>
#include <condition_variable>

struct Room {
	std::string name;
	std::vector<int> clientFDs;

	Room(std::string name) : name(name) {};

	void addClient(int clientFD) {
		clientFDs.push_back(clientFD);
	}

	void removeClient(int clientFD) {
		std::vector<int>::iterator el = std::find(clientFDs.begin(), clientFDs.end(), clientFD);
		if (el != clientFDs.end()) {
			clientFDs.erase(el);
		}
	}

	bool isEmpty() const {
		return clientFDs.empty();
	}
};

struct Client {
	int clientFD;
	std::string username;
	std::vector<std::shared_ptr<Room>> rooms;

	Client(int clientFD, std::string username) : clientFD(clientFD), username(username) {};

	void joinRoom(std::shared_ptr<Room> room) {
		rooms.push_back(room);
		room->addClient(clientFD);
	}
};

struct Task {
	int clientFD;
	std::string clientMessage;
	enum Type {
		Close,
		Message
	} type;
};

class ReaderWritterMutex {
private:
	std::mutex mtx;
	std::condition_variable cv;
	int readerCount = 0;
	bool writerActive = false;

public:
	void lock_shared() {
		std::unique_lock<std::mutex> lock(mtx);
		cv.wait(lock, [&] { return !writerActive; });
		readerCount++;
	}

	void unlock_shared() {
		std::unique_lock<std::mutex> lock(mtx);
		readerCount--;
		if (readerCount == 0) {
			cv.notify_all();
		}
	}

	void lock() {
		std::unique_lock<std::mutex> lock(mtx);
		cv.wait(lock, [&] { return !writerActive && readerCount == 0; });
		writerActive = true;
	}

	void unlock() {
		std::unique_lock<std::mutex> lock(mtx);
		writerActive = false;
		cv.notify_all();
	}
};

class ChatManager {
private:
	std::vector<std::thread> mThreads;

	std::mutex mQueueMutex;
	std::condition_variable mCondition;
	std::queue<Task> mTaskQueue;

	bool mStop;

	ReaderWritterMutex mDataMutex;
	std::unordered_map<int, std::unique_ptr<Client>> mClients;
	std::unordered_map<std::string, std::shared_ptr<Room>> mRooms;

public:
	ChatManager(size_t numThreads = std::thread::hardware_concurrency());
	~ChatManager();

	void enqueueTask(Task task);
private:
	void workLoop();
	
	void handleTask(const Task& task);
	void handleNewClient(const Task& task);
	void handleClientDisconnect(const Task& task);

	void broadcastMessage(const Client& client, const std::string& message) const;
	void sendMessage(int clientFD, const std::string& message) const;

	void parse(const std::string& message, char delimeter, std::vector<std::string>& tokens);
};