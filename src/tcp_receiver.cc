#include "tcp_receiver.hh"
#include "debug.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  // debug( "unimplemented receive() called" );
  // 先判断是否有复位信号, 说明流出错需要重置
  if ( message.RST ) {
    reader().set_error();
    return;
  }
  // 如果收到SYN信号, 则是连接请求, 设置初始基准序号
  if ( message.SYN ) {
    ISN_ = message.seqno;
  }
  if ( !ISN_.has_value() ) {
    return;
  }

  // 已写入字节 +1 作为checkpint (流序号与绝对序号相差1)
  uint64_t checkpoint = writer().bytes_pushed();
  // 计算绝对序号
  uint64_t abs_seqno = message.seqno.unwrap( ISN_.value(), checkpoint );

  // SYN对应绝对序列号0, 但不会在数据流中, 第一个有效数据字节对应绝对序列号1, 所以对应的流编号是0
  uint64_t stream_idx = abs_seqno + ( message.SYN ? 1 : 0 ) - 1;
  // 插入流重组器中
  reassembler_.insert( stream_idx, std::move( message.payload ), message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  // debug( "unimplemented send() called" );
  TCPReceiverMessage msg;
  // 接收方的接收容量最大是65535(16b)
  uint64_t capacity = writer().available_capacity();
  msg.window_size = static_cast<uint16_t>( min( capacity, static_cast<uint64_t>( UINT16_MAX ) ) );

  if ( ISN_.has_value() ) {
    // 接收方已经成功重组并写入 ByteStream 的字节数是 writer().bytes_pushed()
    // 期待收到下一个, 所以要再加一(再加上起始的SYN)
    uint64_t abs_ackno = writer().bytes_pushed() + 1;
    // 关闭了流, 传输结束, FIN也要消耗一个序号, 所以加一
    if ( writer().is_closed() ) {
      abs_ackno++;
    }
    msg.ackno = Wrap32::wrap( abs_ackno, ISN_.value() );
  }
  msg.RST = writer().has_error();
  return msg;
}