#include <iostream>
#include <uvw.hpp>
#include <map>
#include "cache.h"
using DNSHeader=struct {
	unsigned short ID, Flags, QuestNum, AnswerNum, AuthorNum, AdditionNum;
};
constexpr int BUFFERSIZE = 1024;
class DnsServer
{
public:
	DnsServer(std::string address,int port);
	//set upstream dns server
	void start();
	//load host file
	bool loadHost(const char* filePath);
	//set redis server conncection
	void setRedis(const char* host, size_t port);
	~DnsServer();
private:
	std::shared_ptr<uvw::Loop> loop;
	uvw::Addr host;
	std::map<std::string, std::string> config;
	std::pair<bool, std::pair<const char*, int>> queryDns(char* rawData, size_t length);
	std::string getHostName(const char* raw, size_t length);
	unsigned short getType(const char* raw, size_t length);
	cache* cacher = nullptr;
};

