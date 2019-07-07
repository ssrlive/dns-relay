#include <cpp_redis/cpp_redis>
class cache {
public:
	cache(const char* host, size_t port);
	template<typename T>
	void setCache(const char* key, T value, size_t expireTime);
	std::pair<bool, std::vector<std::string>> getStringSets(const char* key);
private:
	std::unique_ptr<cpp_redis::logger> logger;
	cpp_redis::client client;
};

template<typename T>
inline void cache::setCache(const char* key, T value, size_t expireTime) {
	client.sadd(key, value, [](cpp_redis::reply& reply) {
		if (reply.is_string())
			std::cout << reply.as_string() << std::endl;
		});
	client.expire(key, expireTime);
	client.commit();
}
