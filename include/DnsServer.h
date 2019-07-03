#include <iostream>
#include <uvw.hpp>
using DNSHeader=struct {
	unsigned short ID, Flags, QuestNum, AnswerNum, AuthorNum, AdditionNum;
};
constexpr int BUFFERSIZE = 1024;
class DnsServer
{
public:
	DnsServer(std::string address,int port);
	//set upstream dns server
	void setUpstream(std::string upstreamDns);
	void start();
	~DnsServer();
private:
	std::shared_ptr<uvw::Loop> loop;
	uvw::Addr host;
	std::string upstream = "8.8.8.8";
	const char* queryDns(char* rawData,size_t length);
	std::string getHostName(const char* raw, size_t length);
};

