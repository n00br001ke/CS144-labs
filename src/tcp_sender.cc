#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"

using namespace std;

// How many sequence numbers are outstanding?
// 多少序列号未被确认(已发送序号 - 确认号)
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // debug( "unimplemented sequence_numbers_in_flight() called" );
  return next_seqno_ - ack_seqno_;
}

// How many consecutive retransmissions have happened?
uint64_t TCPSender::consecutive_retransmissions() const
{
  // debug( "unimplemented consecutive_retransmissions() called" );
  return consecutive_retransmissions_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // debug( "unimplemented push() called" );
  // 接收方窗口大小为0时, 设为1(零窗口探测)
  uint64_t current_window = window_size_ == 0 ? 1 : window_size_;
  // 只有接收窗口 > 未确认缓存 且还有剩余的时候, 才可以继续发送数据
  while ( current_window > sequence_numbers_in_flight() ) {
    // 之前已经发了FIN, 就结束
    if ( FIN ) {
      break;
    }

    // 构造TCP报文段, 设置序号等信息
    TCPSenderMessage msg;
    msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
    msg.RST = reader().has_error();
    // 还没建立连接, 先建立连接(SYN报文段)
    if ( !SYN ) {
      current_RTO_ms_ = initial_RTO_ms_;
      msg.SYN = true; // 只有在连接建立的时候才会设置SYN
      SYN = true;
    }
    // 还可以发送的报文段长度 = 总接收窗口 - 未确认报文段 (减到这里是剩余可发送窗口) - SYN和FIN占用的字节
    uint64_t payload_size = current_window - sequence_numbers_in_flight() - msg.sequence_length();
    // 防止超过MTU
    payload_size = min( static_cast<uint64_t>( TCPConfig::MAX_PAYLOAD_SIZE ), payload_size );

    // string_view操作(c++17的新特性)
    while ( msg.payload.size() < payload_size ) {
      auto view = reader().peek(); // 获取流数据的内容
      if ( view.empty() ) {
        break;
      }
      auto part = view.substr( 0, payload_size - msg.payload.size() );
      msg.payload += part;         // 真正数据拼接的部分
      reader().pop( part.size() ); // 消耗数据
    }

    // 底层流标记为结束时, 发送结束, 要断开连接
    if ( !FIN && reader().is_finished() ) {
      // 包长度 + 1(FIN消耗序号)没超过窗口大小, 继续发送
      if ( current_window > sequence_numbers_in_flight() + msg.sequence_length() ) {
        msg.FIN = true;
        FIN = true;
      }
    }

    // 拼凑出的长度为0, 无SYN、FIN和数据, 说明不能发送了, 就退出
    if ( msg.sequence_length() == 0 && !msg.RST ) {
      break;
    }

    // 启动超时重传计时器
    if ( !timer_running_ ) {
      timer_running_ = true;
      timer_ms_ = 0;
    }
    // 函数回调(由框架执行发送任务)
    transmit( msg );

    // 缓存此未确认段, 直到被确认
    outstanding_segments_.push_back( msg );
    next_seqno_ = next_seqno_ + msg.sequence_length();

    // FIN 和 RST都意味着需要断开连接, 不能继续发送数据了
    if ( msg.FIN || msg.RST ) {
      break;
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  // debug( "unimplemented make_empty_message() called" );
  TCPSenderMessage msg;
  msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
  msg.RST = reader().has_error();
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // debug( "unimplemented receive() called" );
  // 接收方发送了RST信号, 要处理
  if ( msg.RST ) {
    reader().set_error();
    return;
  }
  // 更新窗口的大小
  window_size_ = msg.window_size;

  // 检查是否有确认号
  if ( !msg.ackno.has_value() ) {
    return;
  }

  // 确认号解包:32位序列号转换为64位绝对序列号
  uint64_t recv_ack = msg.ackno.value().unwrap( isn_, next_seqno_ );

  // 防御性检查:ackno 不能超过 next_seqno( 不能确认还没发的数据 )
  if ( recv_ack > next_seqno_ ) {
    return;
  }

  // 检查是否有被确认
  bool new_data_acked = false;
  if ( recv_ack > ack_seqno_ ) {
    ack_seqno_ = recv_ack; // 更新确认号
    new_data_acked = true;

    while ( !outstanding_segments_.empty() ) {
      auto& seg = outstanding_segments_.front();
      uint64_t seg_end = seg.seqno.unwrap( isn_, next_seqno_ ) + seg.sequence_length();
      // 报文段已经完全被确认, 就从缓存队列里删除, 否则是没有被完全确认, 就继续等待
      if ( seg_end <= recv_ack ) {
        outstanding_segments_.pop_front();
      } else {
        break;
      }
    }
  }
  // 有数据包被确认, 清空超时设置
  if ( new_data_acked ) {
    current_RTO_ms_ = initial_RTO_ms_;
    timer_ms_ = 0;
    consecutive_retransmissions_ = 0;
  }
  // 没有需要记录的数据
  if ( outstanding_segments_.empty() ) {
    timer_running_ = false;
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // debug( "unimplemented tick({}, ...) called", ms_since_last_tick );
  // 没有超时重传计时器被启动
  if ( !timer_running_ ) {
    return;
  }

  timer_ms_ += ms_since_last_tick;
  // 计时器到时, 要重传
  if ( timer_ms_ >= current_RTO_ms_ ) {
    // 重传最早的包
    transmit( outstanding_segments_.front() );
    // 非零窗口, 就进行指数退避算法(如果是零窗口探测包丢失，不应该翻倍，否则恢复太慢
    if ( window_size_ > 0 ) {
      consecutive_retransmissions_++; // 连续重传计数器加一
      current_RTO_ms_ *= 2;
    }
    timer_ms_ = 0; // 重置超时重传计时器
  }
}
