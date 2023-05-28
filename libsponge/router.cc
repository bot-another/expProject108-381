#include "router.hh"
#include "poptrie.hh"

#include <iostream>
#include <limits>
using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    // cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
    //      << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    //_routing_table.push_back(RouteEntry(route_prefix, prefix_length, next_hop, interface_num, prefix_length == 0 ? 0 : numeric_limits<int>::min() >> (prefix_length - 1)));
    _new_routing_tables[prefix_length][route_prefix] = RouteEntry(route_prefix, prefix_length, next_hop, interface_num, prefix_length == 0 ? 0 : numeric_limits<int>::min() >> (prefix_length - 1));
    //long add = _routing_table.size();
    //poptrie_route_add(poptrie, route_prefix, prefix_length, reinterpret_cast<void*>(add));
}

void* Router::FIND(const uint32_t destination){
    return poptrie_lookup(poptrie, destination);
}


int Router::find(const uint32_t destination) {
    int match_idx = -1;
    int max_matched_len = -1;
    for (size_t i = 0; i < _routing_table.size(); i++) {
        auto mask = _routing_table[i]._prefix_mask;
        if ((destination & mask) == _routing_table[i]._route_prefix && max_matched_len < _routing_table[i]._prefix_length) {
            match_idx = i;
            max_matched_len = _routing_table[i]._prefix_length;
        }
    }
    return match_idx;
}

std::unordered_map<uint32_t, RouteEntry>::iterator Router::new_find(const uint32_t destination){
    for (int i = 32; i >= 0; i--) {
        if(_new_routing_tables[i].size() == 0)continue;
        auto mask = _new_routing_tables[i].begin()->second._prefix_mask;
        auto res = _new_routing_tables[i].find(destination & mask);
        if(res != _new_routing_tables[i].end()){
            return res;
        }
    }
    return _new_routing_tables[0].end();
}
//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    if (dgram.header().ttl <= 1) return;
    uint32_t destination = dgram.header().dst;
    auto match_idx = new_find(destination);
    if (match_idx == _new_routing_tables[0].end()) return;
    dgram.header().ttl -= 1;
    auto next_hop = match_idx->second._next_hop;
    auto interface_num = match_idx->second._interface_num;
    if (next_hop.has_value()) {
        _interfaces[interface_num].send_datagram(dgram, next_hop.value());
    } else {
        _interfaces[interface_num].send_datagram(dgram, Address::from_ipv4_numeric(destination));
    }
    // if (dgram.header().ttl <= 1) return;
    // uint32_t destination = dgram.header().dst;
    // auto match_idx = find(destination);
    // if (match_idx == -1) return;
    // dgram.header().ttl -= 1;
    // auto next_hop = _routing_table[match_idx]._next_hop;
    // auto interface_num = _routing_table[match_idx]._interface_num;
    // if (next_hop.has_value()) {
    //     _interfaces[interface_num].send_datagram(dgram, next_hop.value());
    // } else {
    //     _interfaces[interface_num].send_datagram(dgram, Address::from_ipv4_numeric(destination));
    // }
    // if (dgram.header().ttl <= 1) return;
    // uint32_t destination = dgram.header().dst;
    // long match_idx = reinterpret_cast<long>(FIND(destination));
    // if (match_idx == 0) return;
    // dgram.header().ttl -= 1;
    // auto next_hop = _routing_table[match_idx - 1]._next_hop;
    // auto interface_num = _routing_table[match_idx - 1]._interface_num;
    // if (next_hop.has_value()) {
    //     _interfaces[interface_num].send_datagram(dgram, next_hop.value());
    // } else {
    //     _interfaces[interface_num].send_datagram(dgram, Address::from_ipv4_numeric(destination));
    // }
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
