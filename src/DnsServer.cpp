#include "DnsServer.h"
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <sstream>
#include <cstring>

using std::memcpy;
using std::cout;
using std::endl;
extern int debugLevel;
DnsServer::DnsServer(std::string address, int port, const char* upStream) :host(uvw::Addr{ address, port }), upStream(upStream) {
	loop = uvw::Loop::getDefault();
}

void DnsServer::start() {
	server = loop->resource<uvw::UDPHandle>();
	server->on<uvw::UDPDataEvent>([&](const auto& event, const auto&) {
		if (debugLevel == 2)
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
		try {
			auto [status, response] = queryDns(request, event.length + 1, event.sender);//process query
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
				memcpy(toSend, response.first, response.second);
#endif
				server->trySend(event.sender, toSend, response.second);
				delete[] toSend;
			}
		}
		catch (std::exception& e) {
			cout << "Error occurred:" << e.what() << endl;
			if (request)
				delete[] request;
			return;
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

std::pair<bool, std::pair<const char*, int>> DnsServer::queryDns(char* rawData, size_t length, const uvw::Addr& sender) {
	auto hostName = getHostName(rawData, length);
	if (hostName[hostName.length() - 1] == '.')
		hostName.pop_back();//remove the last '.'
	std::vector<std::string> results;
	auto response = new char[512];
	if (config.find(hostName) != config.end()) {//if this ip is in the host file
		results.push_back(config[hostName]);
	}
	else {
		auto [queryCacheStatus, cacheResult] = cacher->getStringSets(hostName.c_str());
		if (queryCacheStatus)//will be true if cache hit
			results.swap(cacheResult);
	}
	if (results.size() == 0) {
		try {
			auto newLoop = uvw::Loop::create();//create a new loop 
			auto upStreamRequest = newLoop->resource<uvw::UDPHandle>();//new UDP Handler
			upStreamRequest->once<uvw::UDPDataEvent>([&](const auto& dataEvent, const auto& handler) {
				if (cacher)
					setCache(dataEvent.data.get(), dataEvent.length, length, hostName.c_str());//set cache is async, don't worry
				memcpy(response, dataEvent.data.get(), dataEvent.length);
				length = dataEvent.length;
				newLoop->stop();//stop loop
				});
			upStreamRequest->recv();
			upStreamRequest->trySend(upStream, 53, rawData, length);
			newLoop->run();//start new event loop until memcpy finished
			newLoop->close();
			return std::pair(true, std::pair(response, length));
		}
		catch (std::exception& e) {
			cout << e.what() << endl;
			exit(-1);
		}
	}
	else {
		int queryType = getType(rawData, length);
		memset(response, 0, 512);
		memcpy(response, rawData, length);
		auto a = results.empty()
			|| (config.find(hostName) != config.end() && config[hostName] == "0.0.0.0") //blocked ip
			|| (queryType != 1 && queryType != 28) //not A or AAAA
			? htons(0x8183) : htons(0x8180);//0x8180=1000 0001 1000 000(0/3)
		memcpy(&response[2], &a, sizeof(unsigned short));
		for (auto IPs : results) {
			//if it is ipv6 address
			bool isIpv6 = false;
			if (IPs.length() > 15) {
				if (queryType == 28)
					isIpv6 = true;
				else {
					auto current = std::find(results.begin(), results.end(), IPs);
					if (current != results.end())
						results.erase(current);//discard invaild data (Query A, return AAAA)
					continue;
				}
			}
			else {
				if (queryType != 1) {
					auto current = std::find(results.begin(), results.end(), IPs);
					if (current != results.end())
						results.erase(current);//discard invaild data (Query AAAA, return AAA)
					continue;
				}
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
			inet_pton(isIpv6 ? AF_INET6 : AF_INET, IPs.c_str(), ipData);
			memcpy(answer + offset, ipData, isIpv6 ? 16 : 4);
			offset += isIpv6 ? 16 : 4;
			memcpy(response + length - 1, answer, offset);
			length += offset;
			delete[] ipData;
		}
		response[7] = results.empty() || results[0] == "0.0.0.0" ? 0 : (char)results.size();
		return std::pair(true, std::pair(response, length));
	}
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

unsigned short DnsServer::getType(const char* raw, size_t length) {
	unsigned short queryType;
	memcpy(&queryType, raw + length - 4, sizeof(unsigned short));
	return queryType;
}
void DnsServer::setRedis(const char* host, size_t port) {
	if (cacher == nullptr)
		cacher = new cache(host, port);
}


void DnsServer::setCache(const char* rawData, size_t dataLength, size_t headerLength, const char* keys) {
	auto answerData = rawData + headerLength - 1;
	using responseStruct= struct {
		unsigned short name;
		unsigned short type;
		unsigned short classes;
		u_long ttl;
		unsigned short length;
	};
	std::vector<std::string> answers;
	size_t minTTL = INTMAX_MAX;
	for (int restAnswers = rawData[7]; restAnswers > 0; restAnswers--) {
		responseStruct temp;
		memcpy(&temp, answerData, sizeof(responseStruct));
		unsigned short type;
		memcpy(&type, (char*)& temp + sizeof(unsigned short), sizeof(unsigned short));
		type = ntohs(type);
		u_long ttl;
		memcpy(&ttl, (char*)& temp + 3 * sizeof(unsigned short), sizeof(u_long));
		ttl = ntohl(ttl);
		if (minTTL > ttl)
			minTTL = ttl;
		unsigned short length;
		memcpy(&length, (char*)& temp + 3 * sizeof(unsigned short) + sizeof(u_long), sizeof(unsigned short));
		length = ntohs(length);
		auto buffer = new char[INET6_ADDRSTRLEN];
		//get IPs from answer
		answers.push_back(inet_ntop(type == 28 ? AF_INET6 : AF_INET, answerData + 4 * sizeof(unsigned short) + sizeof(u_long), buffer, INET6_ADDRSTRLEN));
		delete[] buffer;
		answerData += 4 * sizeof(unsigned short) + sizeof(u_long) + length;//next answer
	}
	if (answers.size() != 0)
		cacher->setCache(keys, answers, minTTL);
}
