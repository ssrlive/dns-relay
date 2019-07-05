#include "DnsServer.h"
#include <fstream>
#include <sstream>
using std::cout;
using std::endl;
DnsServer::DnsServer(std::string address, int port) :host(uvw::Addr{ address, port }) {
	loop = uvw::Loop::getDefault();
}

void DnsServer::start() {
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

		auto [status, result] = queryDns(request, event.length + 1);//process query
		delete[] request;
		if (!status) {
			return;
		}
		else {
			//copy reply to buffer
			auto toSend = new char[512];
#ifdef _MSC_VER
			memcpy_s(toSend, 512, result, 512);
#else
			memcpy(toSend, response.c_str(), response.size());
#endif
			server->trySend(event.sender, toSend, 512);
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

bool DnsServer::loadHost(const char* filePath) {
	std::fstream hostStream(filePath, std::ios::in);
	if (!hostStream.is_open()) {
		hostStream.close();
		return false;
	}
	auto buffer = new char[1024];
	while (!hostStream.eof()) {
		hostStream.getline(buffer, 1024);
		if (buffer[0] == '#' || buffer[0] == '\r')// ignore comments
			continue;
		std::string address, host;
		std::stringstream lineStream(buffer);
		lineStream >> address;
		lineStream >> host;
		if (address.empty() || host.empty()) {
			hostStream.close();
			delete[] buffer;
			return false;
		}
		config[host] = address;
	}
	delete[] buffer;
	return true;
}

DnsServer::~DnsServer()
{
	loop->close();
}

std::pair<bool, const char*> DnsServer::queryDns(char* rawData, size_t length) {
	if (getType(rawData, length) != 1)
		return std::pair(false, nullptr);
	auto hostName = getHostName(rawData, length);
	if (hostName[hostName.length() - 1] == '.')
		hostName.pop_back();//remove the last '.'
	std::vector<std::string> results;
	if (auto findResult = config.find(hostName); findResult != config.end()) {
		results.push_back(config[hostName]);
	}
	else {
		auto upStreamRequest = loop->resource<uvw::GetAddrInfoReq>();
		auto result = upStreamRequest->nodeAddrInfoSync(hostName);
		if (result.first) {
			auto current = result.second.get();
			auto buffer = new char[INET6_ADDRSTRLEN];
			do {
				results.push_back(inet_ntop(current->ai_family, &((const sockaddr_in*)current->ai_addr)->sin_addr, buffer, current->ai_addrlen));
				current = current->ai_next;
			} while (current);
			delete[] buffer;
		}
	}
	auto response = new char[512];
	memset(response, 0, 512);
	memcpy(response, rawData, length);
	auto a = results.empty() || config[hostName] == "0.0.0.0" ? htons(0x8183) : htons(0x8180);//0x8180=1000 0001 1000 000?
	memcpy(&response[2], &a, sizeof(unsigned short));
	response[7] = config[hostName] == "0.0.0.0" ? 0 : (char)results.size();
	int offset = 0;
	for (auto ips : results) {
		char answer[28] = { 0 };//ipv4 16bytes ipv6 28bytes
		//Name
		unsigned short Name = htons(0xc00c); //1100 0000 0000
		memcpy(answer, &Name, sizeof(unsigned short));
		offset += sizeof(unsigned short);
		//Type
		unsigned short queryType;
		memcpy(&queryType, rawData + length - 4, sizeof(unsigned short));
		auto type = ntohs(queryType) == 28 ? htons(0x001c) : htons(0x0001);//0x001c for AAAA, 0x0001 for A
		memcpy(answer + offset, &type, sizeof(unsigned short));
		offset += sizeof(unsigned short);
		//Class
		unsigned short Class = htons(0x0001);
		memcpy(answer + offset, &Class, sizeof(unsigned short));
		offset += sizeof(unsigned short);
		//ttl
		int8_t ttl = htonl(0x0258); //600 seconds
		memcpy(answer + offset, &ttl, sizeof(int8_t));
		offset += sizeof(int8_t);
		//data length
		if (queryType != 28 && ips.length() > 16)//ipv6 address longer than "255.255.255.255"
			continue;
		unsigned short dataLength;
		if (queryType == 28)
			dataLength = htons(0x0010);
		else
			dataLength = htons(0x0004);
		memcpy(answer + offset, &dataLength, sizeof(unsigned short));
		offset += sizeof(unsigned short);
		//ip data
		auto ipData = new char[16];
		inet_pton(queryType == 28 ? AF_INET6 : AF_INET, ips.c_str(), ipData);
		memcpy(answer + offset, &ipData, queryType == 28 ? sizeof(int8_t) : sizeof(int16_t));
		offset += queryType == 28 ? sizeof(int8_t) : sizeof(int16_t);
		memcpy(response + length, answer, offset);
		length += offset;
	}
	return std::pair(true, response);
}


std::string DnsServer::getHostName(const char* raw, size_t length) {
	auto buf = new char[BUFFERSIZE];
	std::string hostName;
	memset(buf, 0, BUFFERSIZE);
	memcpy(buf, &(raw[sizeof(DNSHeader)]), length - 16);
	for (int i = 0; i < length - 16; i++) {
		if (buf[i] > 0 && buf[i] <= 63) {
			for (int count = buf[i]; count > 0; count--) {//byte count
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

unsigned short DnsServer::getType(const char* raw, size_t length){
	raw += sizeof(DNSHeader);
	int nameLength = 0;
	do
		nameLength++;//Get QueryName length
	while (*(raw + nameLength) != '\0');
	raw += (++nameLength);
	auto tmp = (unsigned short*)raw;
	return htons(*(tmp++));
}