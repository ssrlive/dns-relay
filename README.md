# DNS relay
A DNS relay server, powered by libuv and cpp_redis

# Build
Requirements:
- CMake 3.8+
- C++ Compiler support C++/17
To build this repo, please build [libuv](http://libuv.org/) and [cpp_redis](https://github.com/cpp-redis/cpp_redis) first. Also you need to add [uvw](https://github.com/skypjack/uvw) to your include path.

Then change the path in `CMakeList.txt` to your location.

Run:
```
cmake .
make
./dnsRelay -R -c host
```

# Feature
1. Load rules from `host` files.
2. Blocking domains which have a `0.0.0.0` address in `host`.
3. Cache result in Redis, expire by its TTL.

If you meet any bugs or questions, please feel free to add issues. Thank you very much!