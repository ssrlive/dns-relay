#include "cache.h"
cache::cache(const char* host, size_t port){
	logger = std::unique_ptr<cpp_redis::logger>(new cpp_redis::logger);
	try{
		client.connect(host, port);
	}
	catch(exception& e){
		std::cout << "Connect to Redis failed: "<< e.what() << std::endl;
		exit(255);
	}
	std::cout << "Server connect to Redis： " << host << ":" << port << std::endl;
}
std::pair<bool,std::vector<std::string>> cache::getStringSets(const char* key){
	std::vector<std::string> toReturn;
	bool status = false;
	client.smembers(key, [&toReturn, &status](cpp_redis::reply& reply) {
		if (reply.is_array()) {
			for (auto item : reply.as_array()) {
				toReturn.push_back(item.as_string());
			}
			if(!toReturn.empty())
				status = true;
		}
		});
	client.sync_commit();
	return std::pair(status, toReturn);
}