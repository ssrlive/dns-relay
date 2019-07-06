// DNS Relay.cpp : Defines the entry point for the application.
//

#include "DnsServer.h"
#include "DNS Relay.h"
#include <signal.h>
#include "getopt.h"
int main(int argc, char* argv[]) {
	int ch;
	std::string hostFilePath = "../hosts";
	while ((ch = getopt(argc, argv, "dDc:")) != -1){
		switch (ch)
		{
		case 'd':
			debugLevel = 1;
			break;
		case 'D':
			debugLevel = 2;
			break;
		case 'c':
			hostFilePath = optarg;
			break;
		default:
			break;
		}
	}
	server = unique_ptr<DnsServer>{ new DnsServer("127.0.0.1",53) };
	if(!hostFilePath.empty())
	server->loadHost(hostFilePath.c_str());
	server->setRedis("127.0.0.1", 6379);
	server->start();
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);
	return 0;
}

//capture the Ctrl+C event
void signalHandler(int sig) {
	cout << endl;
	switch (sig)
	{
	case SIGINT:
		cout << "Captured Signal SIGINT Events,exitting..." << endl;
		break;
	case SIGTERM:
		cout << "Captured Signal SIGTERM Events,exitting..." << endl;
		break;
	default:
		cout << "Captured Signal" << sig << " Events,exitting..." << endl;
		break;
	}
	server.release();
	exit(sig);
}
