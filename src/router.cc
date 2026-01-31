#include "router.hh"
#include "address.hh"
#include "debug.hh"
#include "ipv4_datagram.hh"
#include "network_interface.hh"

#include <cstdint>
#include <iostream>
#include <optional>
#include <utility>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  // cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
  //      << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
  //      << " on interface " << interface_num << "\n";
  // debug( "unimplemented add_route() called" );

  // 路由条目存入路由表
  routing_table_.push_back( { route_prefix, prefix_length, next_hop, interface_num } );
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  // debug( "unimplemented route() called" );
  // 检查路由器的所有接口
  for ( auto& interface_ptr : interfaces_ ) {
    // 检查接口中的数据报
    auto& datagrams_in_queue = interface_ptr->datagrams_received();
    while ( !datagrams_in_queue.empty() ) {
      InternetDatagram datagram = std::move( datagrams_in_queue.front() );
      datagrams_in_queue.pop();
      if ( datagram.header.ttl <= 1 ) {
        continue; // TTL耗尽, 丢弃(也许会有发送ICMP数据报的逻辑, 但是实验手册提示并不是所有路由器都会发送ICMP数据包)
      }
      datagram.header.ttl--;
      datagram.header.compute_checksum();                  // 重新计算首部检验和
      uint32_t dst_ip = datagram.header.dst;               // 目的IP地址
      std::optional<RouteEntry> best_match = std::nullopt; // 最长前缀匹配结果

      for ( const auto& entry : routing_table_ ) {
        // 从路由表结构中获取掩码(为0时应该是对应默认路由)
        uint32_t mask = ( entry.prefix_length == 0 ) ? 0 : 0xffffffffU << ( 32 - entry.prefix_length );

        uint32_t dst_prefix = dst_ip & mask;
        uint32_t entry_prefix = entry.route_prefix & mask;
        if ( dst_prefix == entry_prefix ) {
          // 第一次匹配成功或者更精确的前缀(最长前缀)
          if ( !best_match.has_value() || best_match->prefix_length < entry.prefix_length ) {
            best_match = entry;
          }
        }
      }
      // 路由表项中next_hop有值, 数据报要经过本路由器间接转发
      // 路由表项中next_hop没有值, 目的ip与某接口直连, 直接交付
      if ( best_match.has_value() ) {
        const Address dst_address = best_match->next_hop.value_or( Address::from_ipv4_numeric( dst_ip ) );
        interface( best_match->interface_num )->send_datagram( datagram, dst_address );
      }
    }
  }
}
