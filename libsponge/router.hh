#ifndef SPONGE_LIBSPONGE_ROUTER_HH
#define SPONGE_LIBSPONGE_ROUTER_HH

#include "network_interface.hh"
#include "poptrie.hh"

#include <optional>
#include <queue>
#include <unordered_map>
// void FREE(radix_node_t *radix, void *cbctx);
//! \brief A wrapper for NetworkInterface that makes the host-side
//! interface asynchronous: instead of returning received datagrams
//! immediately (from the `recv_frame` method), it stores them for
//! later retrieval. Otherwise, behaves identically to the underlying
//! implementation of NetworkInterface.
class AsyncNetworkInterface : public NetworkInterface {
    std::queue<InternetDatagram> _datagrams_out{};

  public:
    using NetworkInterface::NetworkInterface;

    //! Construct from a NetworkInterface
    AsyncNetworkInterface(NetworkInterface &&interface) : NetworkInterface(interface) {}

    //! \brief Receives and Ethernet frame and responds appropriately.

    //! - If type is IPv4, pushes to the `datagrams_out` queue for later retrieval by the owner.
    //! - If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
    //! - If type is ARP reply, learn a mapping from the "target" fields.
    //!
    //! \param[in] frame the incoming Ethernet frame
    void recv_frame(const EthernetFrame &frame) {
        auto optional_dgram = NetworkInterface::recv_frame(frame);
        if (optional_dgram.has_value()) {
            _datagrams_out.push(std::move(optional_dgram.value()));
        }
    };

    //! Access queue of Internet datagrams that have been received
    std::queue<InternetDatagram> &datagrams_out() { return _datagrams_out; }
};
class RouteEntry {
    public:
    uint32_t _route_prefix;
    uint8_t _prefix_length;
    std::optional<Address> _next_hop;
    size_t _interface_num;
    uint32_t _prefix_mask;
    RouteEntry(uint32_t a, uint8_t b, std::optional<Address> c, size_t d, uint32_t e):_route_prefix(a), _prefix_length(b), _next_hop(c), _interface_num(d), _prefix_mask(e){}
    RouteEntry(RouteEntry&& other):_route_prefix(other._route_prefix), _prefix_length(other._prefix_length), _next_hop(other._next_hop), _interface_num(other._interface_num), _prefix_mask(other._prefix_mask){}
    RouteEntry():_route_prefix(0), _prefix_length(0), _next_hop(std::nullopt), _interface_num(0), _prefix_mask(0){}
    RouteEntry& operator=(RouteEntry&& other){
        _route_prefix = other._route_prefix;
        _prefix_length = other._prefix_length;
        _next_hop = other._next_hop;
        _interface_num = other._interface_num;
        _prefix_mask = other._prefix_mask;
        return *this;
    }
};
//! \brief A router that has multiple network interfaces and
//! performs longest-prefix-match routing between them.
class Router {
    //! The router's collection of network interfaces
    std::vector<AsyncNetworkInterface> _interfaces{};

    //! Send a single datagram from the appropriate outbound interface to the next hop,
    //! as specified by the route with the longest prefix_length that matches the
    //! datagram's destination address.
    void route_one_datagram(InternetDatagram &dgram);

    std::vector<RouteEntry> _routing_table{};
    std::vector<std::unordered_map<uint32_t, RouteEntry>> _new_routing_tables{33};
    


    struct poptrie *poptrie{NULL};

  public:
    int find(const uint32_t destination);
    void* FIND(const uint32_t destination);
    std::unordered_map<uint32_t, RouteEntry>::iterator new_find(const uint32_t destination);
    //! Add an interface to the router
    //! \param[in] interface an already-constructed network interface
    //! \returns The index of the interface after it has been added to the router
    size_t add_interface(AsyncNetworkInterface &&interface) {
        _interfaces.push_back(std::move(interface));
        return _interfaces.size() - 1;
    }

    //! Access an interface by index
    AsyncNetworkInterface &interface(const size_t N) { return _interfaces.at(N); }

    //! Add a route (a forwarding rule)
    void add_route(const uint32_t route_prefix,
                   const uint8_t prefix_length,
                   const std::optional<Address> next_hop,
                   const size_t interface_num);

    //! Route packets between the interfaces
    void route();
    Router(){
        poptrie = poptrie_init(NULL, 22, 22);
    }
    ~Router(){
        poptrie_release(poptrie);
    }
    Router(const Router&other) = default;
    Router& operator=(const Router&) = default;
};
#endif  // SPONGE_LIBSPONGE_ROUTER_HH
