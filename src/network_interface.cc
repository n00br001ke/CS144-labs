#include <cstdint>
#include <iostream>

#include "address.hh"
#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "exception.hh"
#include "helpers.hh"
#include "ipv4_datagram.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( InternetDatagram dgram, const Address& next_hop )
{
  // debug( "unimplemented send_datagram called" );
  // ARP缓存中已有对应IP到MAC的映射
  uint32_t next_ip = next_hop.ipv4_numeric();
  if ( arp_cache_.contains( next_ip ) ) {
    EthernetFrame frame;
    frame.header.dst = arp_cache_[next_ip].mac;
    frame.header.src = ethernet_address_;
    frame.header.type = EthernetHeader::TYPE_IPv4;
    frame.payload = serialize( dgram ); // 直接封装整个IPv4数据报
    transmit( frame );
  } else {
    // ARP缓存中没有对应的IP到MAC, 映射就先把数据帧缓存起来, 然后发送ARP请求帧
    arp_waiting_datagrams_[next_ip].push_back( dgram );
    // 五秒前是否发送过ARP请求帧(限流)
    if ( arp_waiting_requests_.contains( next_ip ) == 0 ) {
      ARPMessage arp_request; // 构造ARP请求
      arp_request.opcode = ARPMessage::OPCODE_REQUEST;
      arp_request.sender_ip_address = ip_address_.ipv4_numeric();
      arp_request.sender_ethernet_address = ethernet_address_;
      arp_request.target_ip_address = next_hop.ipv4_numeric();

      EthernetFrame frame;                   // 构造以太网帧
      frame.header.dst = ETHERNET_BROADCAST; // 广播请求
      frame.header.src = ethernet_address_;
      frame.header.type = EthernetHeader::TYPE_ARP;
      frame.payload = serialize( arp_request );
      transmit( frame );
      // 设置五秒ARP请求间隔(限流)
      arp_waiting_requests_[next_ip] = 5000;
    }
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  // debug( "unimplemented recv_frame called" );
  // 数据链路层(OSI标准)直接丢弃非广播以及目的地址非自己的帧
  if ( frame.header.dst != ETHERNET_BROADCAST && frame.header.dst != ethernet_address_ ) {
    return;
  }
  // 处理ARP帧( 可能包含请求帧和响应帧 )
  if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage arp_request;
    if ( parse( arp_request, frame.payload ) ) {
      const uint32_t ip = arp_request.sender_ip_address;
      const EthernetAddress ethernet_address = arp_request.sender_ethernet_address;
      // 无论是ARP请求还是ARP响应, 只要是发给我们的或者广播的, 都可以"顺便"学习
      arp_cache_[ip] = { ethernet_address, 30000 };

      // 学习过后, 检查有没有可以发送的帧
      if ( arp_waiting_datagrams_.contains( ip ) ) {
        for ( const auto& dgram : arp_waiting_datagrams_[ip] ) {
          send_datagram( dgram, Address::from_ipv4_numeric( ip ) );
        }
        arp_waiting_requests_.erase( ip );
        arp_waiting_datagrams_.erase( ip );
      }
      // 处理请求本接口MAC地址的ARP请求帧
      if ( arp_request.opcode == ARPMessage::OPCODE_REQUEST
           && ip_address_.ipv4_numeric() == arp_request.target_ip_address ) {

        ARPMessage arp_reply; // 构造ARP响应
        arp_reply.opcode = ARPMessage::OPCODE_REPLY;
        arp_reply.sender_ip_address = ip_address_.ipv4_numeric();
        arp_reply.sender_ethernet_address = ethernet_address_;
        arp_reply.target_ip_address = ip;
        arp_reply.target_ethernet_address = ethernet_address;

        EthernetFrame reply_frame;
        reply_frame.header.src = ethernet_address_;
        reply_frame.header.dst = arp_request.sender_ethernet_address;
        reply_frame.header.type = EthernetHeader::TYPE_ARP;
        reply_frame.payload = serialize( arp_reply );
        transmit( reply_frame );
      }
    }
  } else if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    // 处理属于本接口的IP数据帧
    InternetDatagram datagram;
    if ( parse( datagram, frame.payload ) ) {
      datagrams_received_.push( datagram );
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // debug( "unimplemented tick({}) called", ms_since_last_tick );
  // 检查ARP缓存的映射条目是否超时, 清理过期的 ARP 缓存
  for ( auto it = arp_cache_.begin(); it != arp_cache_.end(); ) {
    if ( ms_since_last_tick >= it->second.remaining_ttl ) {
      it = arp_cache_.erase( it );
    } else {
      it->second.remaining_ttl -= ms_since_last_tick;
      ++it;
    }
  }

  // 检查是否有ARP请求条目已经超过了5s的限时
  for ( auto it = arp_waiting_requests_.begin(); it != arp_waiting_requests_.end(); ) {
    if ( ms_since_last_tick >= it->second ) {
      arp_waiting_datagrams_.erase( it->first ); // 移除这个ARP请求对应所有数据, 防止待发送队列无限积压
      // 移除请求限时
      it = arp_waiting_requests_.erase( it );
    } else {
      it->second -= ms_since_last_tick;
      ++it;
    }
  }
}
