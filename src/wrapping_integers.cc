#include "wrapping_integers.hh"
#include "debug.hh"

using namespace std;
/**
 *
 * @param n 绝对64位序号
 * @param zero_point 初始序列号
 * @return Wrap32 相对于zero_point的32位的环绕序列号
 */
Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  // debug( "unimplemented wrap( {}, {} ) called", n, zero_point.raw_value_ );
  // Wrap32重载了 + 运算符, 会自动进行对2^32取模
  return zero_point + static_cast<uint32_t>( n );
}

/**
 *
 * @param zero_point //初始序列号
 * @param checkpoint //
 * @return uint64_t
 */
uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  // debug( "unimplemented unwrap( {}, {} ) called", zero_point.raw_value_, checkpoint );

  // 当前序号 与 ISN的32位距离
  uint64_t offset = raw_value_ - zero_point.raw_value_;

  // 一个环绕周期 = 2^32
  uint64_t wrap_size = 1ULL << 32;
  // ~wrap_size -1(wrap_size -1) = 0xFFFFFFFF00000000H
  // 这里的操作就是取checkpoint的高32位(序号32位一个周期)然后拼上offset, 但是它是一个候选的绝对序列号
  uint64_t res = ( checkpoint & ~( wrap_size - 1 ) ) | offset;

  // 前后周期检查, 数据包可能会乱序, 但通常不会"乱"到几 GB 以外去
  // 做法是寻找与checkpoint数值距离最小的那个绝对序列号
  // 比 checkpoint 小得太多, res更靠近后一个周期
  if ( res < checkpoint && ( checkpoint - res ) > wrap_size / 2 ) {
    res += wrap_size;
  }
  // 比 checkpoint 大得太多, res更靠前一个周期
  if ( res > checkpoint && ( res - checkpoint ) > wrap_size / 2 ) {
    // 尝试回去到前一个周期时, 要注意下溢
    if ( res >= wrap_size ) {
      res -= wrap_size;
    }
  }
  // res一定离 checkpoint 最近
  return res;
}