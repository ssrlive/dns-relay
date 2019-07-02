#include "DnsServer.h"
using std::cout;
using std::endl;
DnsServer::DnsServer(std::string address, int port):host(uvw::Addr{ address, port }){
	loop = uvw::Loop::getDefault();
}

void DnsServer::start(){
	auto server = loop->resource<uvw::UDPHandle>();
	server->on<uvw::UDPDataEvent>([&](const auto& event, const auto&) {
		cout << "Receive from " << event.sender.ip << ":" << event.sender.port << endl;
		if (event.length == 0)
			return;
		auto request = new char[event.length + 1];
#ifdef _MSC_VER
		memcpy_s(request, event.length, event.data.get(), event.length);
#else
		memcpy(request, event.data.get(), event.length);
#endif
		request[event.length] = '\0';

		std::string reply = queryDns(request, event.length + 1);//process query

		//copy reply to buffer
		auto toSend = new char[reply.size()];
#ifdef _MSC_VER
		memcpy_s(toSend, reply.size(), reply.c_str(), reply.size());
#else
		memcpy(toSend, response.c_str(), response.size());
#endif
		server->trySend(event.sender, toSend, reply.length());
		delete[] request;
		delete[] toSend;
		});
	server->on<uvw::ErrorEvent>([](const auto& event, auto&) {
		cout << "Error occurred:" << event.what() << endl;
		});
	try {
		server->bind("127.0.0.1", host.port);
		server->recv();
		cout << "Start listening at 127.0.0.1:" << host.port << endl;
		loop->run();
	}
	catch (std::exception& e) {
		std::cerr << "Error occurred:" << e.what() << endl;
		loop->close();
		exit(-1);
	}
}

DnsServer::~DnsServer()
{
	loop->close();
}

const char* DnsServer::queryDns(char* rawData, size_t length)
{
	//process query here
	return nullptr;
}

void DnsServer::setUpstream(std::string upstreamDns){
	upstream = upstreamDns;
}