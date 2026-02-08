#include <string>
#include <vector>
#include <unistd.h>
#include <iostream>
#include <termio.h>

#include "client.h"

using std::cout;
using std::cerr;
using std::endl;

using std::string;
using std::vector;

#define HOST_PORT_DELIMITER ":"

int main(int argc, char ** argv)
{
	// Set terminal to non-canonical mode for real-time input processing
    termios oldt;
    tcgetattr(STDIN_FILENO, &oldt);
    termios t = oldt;

    t.c_lflag &= ~(ICANON | ECHO);
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSANOW, &t);

	// Parse arguments
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <host>:<port> <USER_NAME> <ROOM_NAME_1> [ROOM_NAME_2...ROOM_NAME_N]" << endl;
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return 1;
    }

	string host_port = string(argv[1]);
    size_t delimiter_pos = host_port.find(HOST_PORT_DELIMITER);
    string host = host_port.substr(0, delimiter_pos);
    int port = stoi(host_port.substr(delimiter_pos + 1, host_port.size()));
	
	string username = string(argv[2]);
    
	vector<string> room_names;
	for (int i = 3; i < argc; ++i) {
        room_names.push_back(string(argv[i]));
    }

    // Run Client
    try {
        Client client(username, room_names);

		client.setupConnection(host, port);
        client.connectToServer();

		cout << endl << "Shutting down..." << endl;

    } catch (const std::exception& e) {
        cerr << e.what() << endl;
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return 1;
	}

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return 0;
}
