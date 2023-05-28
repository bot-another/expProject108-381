#include "../libsponge/router.hh"
#include <string>
#include <vector>
#include <chrono>
#include <iostream>
uint32_t ip(const std::string &str) { return Address{str}.ipv4_numeric(); }
int main(){
    Router _router;
    _router.add_route(ip("10.0.0.0"), 8, {}, 2);
    _router.add_route(ip("172.16.0.0"), 16, {}, 3);
    _router.add_route(ip("192.168.0.0"), 24, {}, 4);
    _router.add_route(ip("198.178.229.0"), 24, {}, 5);
    _router.add_route(ip("128.30.76.255"), 16, Address{"128.30.0.1"}, 9);
    for(int i = 0; i < 2000000;i++){
        _router.add_route(ip("11.0.0.0") + i, 32, Address{"128.30.0.1"}, 10);
    }
    //随机查找100w次，计时
    std::vector<uint32_t> random_ip;
    for(int i = 0; i < 1000000;i++){
        //ramdom
        random_ip.push_back(ip("1.0.0.0") + rand() % 0x00ffffff);
    }
    auto start = std::chrono::steady_clock::now();
    for(int i = 0; i < 10000;i++){
        _router.find(random_ip[i]);
    }
    auto end = std::chrono::steady_clock::now();
    std::cout << "old_find: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;
    start = std::chrono::steady_clock::now();
    for(int i = 0; i < 10000;i++){
        _router.new_find(random_ip[i]);
    }
    end = std::chrono::steady_clock::now();
    std::cout << "new_find: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;
        start = std::chrono::steady_clock::now();
    for(int i = 0; i < 10000;i++){
        _router.FIND(random_ip[i]);
    }
    end = std::chrono::steady_clock::now();
    std::cout << "radix: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;
}