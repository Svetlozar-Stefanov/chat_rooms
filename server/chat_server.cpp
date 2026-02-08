#include "server.h"

#include <iostream>
#include <exception>

int main()
{
	try {
		Server server;
		server.setup();
		server.start();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	
	return 0;
}
