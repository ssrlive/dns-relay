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

		auto [status, response] = queryDns(request, event.length + 1);//process query
		delete[] request;
		if (!status) {
			return;
		}
		else {
			//copy reply to buffer
			auto toSend = new char[response.second];
#ifdef _MSC_VER
			memcpy_s(toSend, response.second, response.first, response.second);
#else
			memcpy(toSend, response.c_str(), response.size());
#endif
			server->trySend(event.sender, toSend, response.second);
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

std::pair<bool, std::pair<const char*, int>> DnsServer::queryDns(char* rawData, size_t length) {
	auto hostName = getHostName(rawData, length);
	if (hostName[hostName.length() - 1] == '.')
		hostName.pop_back();//remove the last '.'
	std::vector<std::string> results;
	if (config.find(hostName)!= config.end()) {//if this ip is in the host file
		results.push_back(config[hostName]);
	}
	else {
		auto [queryCacheStatus, cacheResult] = cacher->getStringSets(hostName.c_str());
		if (queryCacheStatus) {
			results.swap(cacheResult);
		}
		else {
			auto upStreamRequest = loop->resource<uvw::GetAddrInfoReq>();
			auto result = upStreamRequest->nodeAddrInfoSync(hostName);
			if (result.first) {
				auto current = result.second.get();
				auto buffer = new char[2 * INET6_ADDRSTRLEN];
				do {
					std::string ip = inet_ntop(current->ai_family, &((const sockaddr_in*)current->ai_addr)->sin_addr, buffer, current->ai_addrlen);
					results.push_back(ip);
					current = current->ai_next;
				} while (current);
				if (cacher && !results.empty())
					cacher->setCache(hostName.c_str(), results, 600);
				delete[] buffer;
			}
		}
	}
	auto response = new char[512];
	memset(response, 0, 512);
	memcpy(response, rawData, length);
	auto a = results.empty()
		|| (config.find(hostName) != config.end() && config[hostName] == "0.0.0.0") //blocked ip
		|| (getType(rawData, length) != 1 && getType(rawData, length) != 28) //not A or AAAA
		? htons(0x8183) : htons(0x8180);//0x8180=1000 0001 1000 000(0/3)
	memcpy(&response[2], &a, sizeof(unsigned short));
	response[7] = results.empty() || results[0] == "0.0.0.0" ? 0 : (char)results.size();
	for (auto ips : results) {
		//if it is ipv6 address
		unsigned short queryType;
		memcpy(&queryType, rawData + length - 4, sizeof(unsigned short));
		bool isIpv6 = false;
		if (queryType == 28) {
			if (ips.length() > 15) {
				isIpv6 = true;
			}
			else
				continue;//invaild result
		}
		//start answer
		int offset = 0;
		char answer[28] = { 0 };//ipv4 16bytes ipv6 28bytes
		//Name
		unsigned short Name = htons(0xc00c); //1100 0000 0000
		memcpy(answer, &Name, sizeof(unsigned short));
		offset += sizeof(unsigned short);
		//Type
		auto type = isIpv6 ? htons(0x001c) : htons(0x0001);//0x001c for AAAA, 0x0001 for A
		memcpy(answer + offset, &type, sizeof(unsigned short));
		offset += sizeof(unsigned short);
		//Class
		unsigned short Class = htons(0x0001);
		memcpy(answer + offset, &Class, sizeof(unsigned short));
		offset += sizeof(unsigned short);
		//ttl
		auto ttl = htonl(0x0258); //600 seconds
		memcpy(answer + offset, &ttl, sizeof(u_long));
		offset += sizeof(u_long);
		//data length
		unsigned short dataLength;
		if (isIpv6)
			dataLength = htons(0x0010);
		else
			dataLength = htons(0x0004);
		memcpy(answer + offset, &dataLength, sizeof(unsigned short));
		offset += sizeof(unsigned short);
		//ip data
		auto ipData = new char[isIpv6 ? 16 : 4];
		inet_pton(isIpv6 ? AF_INET6 : AF_INET, ips.c_str(), ipData);
		memcpy(answer + offset, ipData, isIpv6 ? 16 : 4);
		offset += isIpv6 ? 16 : 4;
		memcpy(response + length -1, answer, offset);
		length += offset;
		delete[] ipData;
	}
	return std::pair(true, std::pair(response, length));
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
void DnsServer::setRedis(const char* host, size_t port){
	if (cacher == nullptr)
		cacher = new cache(host, port);
}