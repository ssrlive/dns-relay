// DNS Relay.cpp : Defines the entry point for the application.
//

#include "DnsServer.h"
#include <signal.h>
using namespace std;
unique_ptr<DnsServer> server;
void signalHandler(int sig);
int main() {
	server = unique_ptr<DnsServer>{ new DnsServer("127.0.0.1",1053) };
	server->setUpstream("1.1.1.1");
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
