#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <functional>

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms )
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive retransmissions have happened?
  const Writer& writer() const { return input_.writer(); }
  const Reader& reader() const { return input_.reader(); }
  Writer& writer() { return input_.writer(); }

private:
  Reader& reader() { return input_.reader(); }

  ByteStream input_;
  Wrap32 isn_;              // 保存随机生成的起始序号
  uint64_t initial_RTO_ms_; // 初始超时时间

  // 连接管理
  bool SYN = false; // 是否已发送SYN, 即:是否建立连接
  bool FIN = false; // 是否已发送FIN, 即:是否关闭连接

  // 序号管理
  uint64_t next_seqno_ = 0;  // 发送方下一个要发送的绝对序号
  uint64_t ack_seqno_ = 0;   // 接收方确认的绝对序号
  uint16_t window_size_ = 1; // 接收方通知的接收窗口大小

  // 超时重传管理
  uint64_t consecutive_retransmissions_ = 0; // 连续重传次数
  uint64_t current_RTO_ms_ = 0;              // 当前超时时间(由于退避算法存在, 会翻倍)
  uint64_t timer_ms_ = 0;                    // 计时器运行时间
  bool timer_running_ = false;               // 计时器运行状态

  std::deque<TCPSenderMessage> outstanding_segments_ {}; // 缓存未被接收方确认的段的队列
};
