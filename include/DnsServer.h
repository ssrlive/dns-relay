#include <iostream>
#include <uvw.hpp>
#include <map>
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
	~DnsServer();
private:
	std::shared_ptr<uvw::Loop> loop;
	uvw::Addr host;
	std::map<std::string, std::string> config;
	std::pair<bool, const char*> queryDns(char* rawData,size_t length);
	std::string getHostName(const char* raw, size_t length);
};

