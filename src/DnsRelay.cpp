// DNS Relay.cpp : Defines the entry point for the application.
//

#include "DnsServer.h"
#include "DnsRelay.h"
#include <signal.h>
#include "getopt.h"
int main(int argc, char* argv[]) {
	int ch;
	std::string hostFilePath = "../hosts";
	std::string bindAddr = "127.0.0.1";
	bool useRedis = false;
	int port = 53;//default port
	while ((ch = getopt(argc, argv, "dDRb:c:p:h")) != -1){
		switch (ch)
		{
		case 'd':
			debugLevel = 1;
			break;
		case 'D':
			debugLevel = 2;
			break;
		case 'R':
			useRedis = true;
			break;
		case 'b':
			bindAddr = optarg;
			break;
		case 'c':
			hostFilePath = optarg;
			break;
		case 'p':
			std::stringstream(optarg) >> port;
			break;
		case 'h':
		default:
			cout << "Help:" << endl;
			cout << "\t-d\t\tShow debug message." << endl;
			cout <<"\t-D\t\tShow more debug message." << endl;
			cout << "\t-c [file]\tLoad host file." << endl;
			cout << "\t-R\t\tEnable Redis cache.Default port is 127.0.0.1:6379" << endl;
			cout << "\t-b [binding IP]\tBinding IP address, default is 127.0.0.1" << endl;
			cout << "\t-p [port]\tListening port, default is 53." << endl;
			cout << "\t-h\t\tShow this page." << endl;
			exit(0);
			break;
		}
	}
	server = unique_ptr<DnsServer>{ new DnsServer(bindAddr, port,"8.8.8.8") };
	if(!hostFilePath.empty())
	server->loadHost(hostFilePath.c_str());
	if (useRedis)
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
