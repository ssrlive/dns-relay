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

		auto result = queryDns(request, event.length + 1);//process query
		delete[] request;
		if (result.first == false) {
			return;
		}
		else {
			std::string reply = result.second;
			//copy reply to buffer
			auto toSend = new char[reply.size()];
#ifdef _MSC_VER
			memcpy_s(toSend, reply.size(), reply.c_str(), reply.size());
#else
			memcpy(toSend, response.c_str(), response.size());
#endif
			server->trySend(event.sender, toSend, reply.length());
			delete[] toSend;
		}
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

std::pair<bool,const char*> DnsServer::queryDns(char* rawData, size_t length)
{
	auto hostName = getHostName(rawData, length);
	auto upStreamRequest = loop->resource<uvw::GetAddrInfoReq>();
	auto result = upStreamRequest->nodeAddrInfoSync(hostName);
	if (result.first) {
		auto current = result.second.get();
		std::vector<std::string> results;
		auto buffer = new char[INET6_ADDRSTRLEN];
		do {
			results.push_back(inet_ntop(current->ai_family, &((const sockaddr_in*)current->ai_addr)->sin_addr, buffer, current->ai_addrlen));
			current = current->ai_next;
		} while (current);
		delete[] buffer;
		return std::pair(true, results[0].c_str());
	}
	else
		return std::pair(false, nullptr);
}

void DnsServer::setUpstream(std::string upstreamDns){
	upstream = upstreamDns;
}

std::string DnsServer::getHostName(const char* raw, size_t length) {
	auto buf = new char[BUFFERSIZE];
	std::string hostName;
	memset(buf, 0, BUFFERSIZE);
	memcpy(buf, &(raw[sizeof(DNSHeader)]), length - 16);
	for (int i = 0; i < length - 16; i++) {
		if (buf[i] > 0 && buf[i] <= 63) {
			for (int j = buf[i]; j > 0; j--) {//byte count
				hostName += buf[++i];
			}
			hostName += '.';
		}
		else
			break;
	}
	delete[] buf;
	return hostName;
}