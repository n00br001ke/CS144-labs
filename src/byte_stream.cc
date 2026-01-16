#include "byte_stream.hh"
#include "debug.hh"
#include <algorithm>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

// Push data to stream, but only as much as available capacity allows.
void Writer::push( string data )
{
  // Your code here (and in each method below)
  // debug( "Writer::push({}) not yet implemented", data );
  if(is_closed() || data.empty()){
    return;
  }
  // 计算实际可写长度,不能超过容量
  uint64_t real = min( data.size(), capacity_ - buffer_.size() );
  for ( uint64_t i = 0; i < real; ++i){
    buffer_.push_back( data[i] );
  }
  pushed_count_ += real;
}

// Signal that the stream has reached its ending. Nothing more will be written.
void Writer::close()
{
  // debug( "Writer::close() not yet implemented" );
  closed_ = true;
}

// Has the stream been closed?
bool Writer::is_closed() const
{
  // debug( "Writer::is_closed() not yet implemented" );
  // Your code here.
  return closed_;
}

// How many bytes can be pushed to the stream right now?
uint64_t Writer::available_capacity() const
{
  // debug( "Writer::available_capacity() not yet implemented" );
  // Your code here.
  return capacity_ - buffer_.size(); // 容量 - 实际 = 可用
}

// Total number of bytes cumulatively pushed to the stream
uint64_t Writer::bytes_pushed() const
{
  // debug( "Writer::bytes_pushed() not yet implemented" );
  // Your code here.
  return pushed_count_;
}

// Peek at the next bytes in the buffer -- ideally as many as possible.
// It's not required to return a string_view of the *whole* buffer, but
// if the peeked string_view is only one byte at a time, it will probably force
// the caller to do a lot of extra work.
string_view Reader::peek() const
{
  // debug( "Reader::peek() not yet implemented" );
  if(buffer_.empty()){
    return {};
  }

  return {&buffer_.front(),1}; // Your code here.
}

// Remove `len` bytes from the buffer.
void Reader::pop( uint64_t len )
{
  // debug( "Reader::pop({}) not yet implemented", len );
  // 计算实际可读长度
  uint64_t real = min( len, buffer_.size() );
  for ( uint64_t i = 0; i < real; ++i){
    buffer_.pop_front();
  }
  popped_count_ += real ;
}

// Is the stream finished (closed and fully popped)?
bool Reader::is_finished() const
{
  // debug( "Reader::is_finished() not yet implemented" );
  // Your code here.
  return closed_ && buffer_.empty(); // 流完成的标志是:关闭 且 无数据可读
}

// Number of bytes currently buffered (pushed and not popped)
uint64_t Reader::bytes_buffered() const
{
  // debug( "Reader::bytes_buffered() not yet implemented" );
  // Your code here.
  return buffer_.size();
}

// Total number of bytes cumulatively popped from stream
uint64_t Reader::bytes_popped() const
{
  // debug( "Reader::bytes_popped() not yet implemented" );
  // Your code here.
  return popped_count_;
}
