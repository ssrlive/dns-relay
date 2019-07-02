#include <iostream>
#include <uvw.hpp>
class DnsServer
{
public:
	DnsServer(std::string address,int port);
	void start();
	~DnsServer();
private:
	std::shared_ptr<uvw::Loop> loop;
	uvw::Addr host;
	const char* queryDns(char* rawData,size_t length);
};

