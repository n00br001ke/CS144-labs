#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // 处理 EOF, 记录文件末尾的位置
  if ( is_last_substring ) {
    is_last_ = true;
    eof_index_ = first_index + data.size();
  }

  // 计算容量限制
  // 指导书提示 capacity是 ByteStream 缓冲区大小 + Reassembler 待重组大小的总和
  // 超出部分首字节的序号是 已经读取的索引 + 总容量 所计算出的序号
  uint64_t total_capacity = output_.writer().available_capacity() + output_.reader().bytes_buffered();
  uint64_t first_unacceptable = output_.reader().bytes_popped() + total_capacity;

  // 接下来是计算是否可接受
  // 如果数据完全在范围外, 或者数据是完全重复收到的, 直接返回
  if ( first_index >= first_unacceptable || first_index + data.size() <= next_index_ ) {
    // 特殊情况: 如果这是一个空的 EOF 包, 且刚好落在 next_index_ 上, 仍需检查 close
    if ( is_last_ && next_index_ == eof_index_ ) {
      output_.writer().close();
    }
    return;
  }

  // 裁剪右边界, 超出容量的部分丢弃
  if ( first_index + data.size() > first_unacceptable ) {
    data.resize( first_unacceptable - first_index );
  }

  // 裁剪左边界, 已经写入 ByteStream 的部分丢弃
  if ( first_index < next_index_ ) {
    data.erase( 0, next_index_ - first_index );
    first_index = next_index_;
  }

  // 区间合并逻辑: 确保 map_ 中存储的区间互不重叠, 把 data 融入现有的区间中
  // 向前查找可能重叠的区间, lower_bound函数 —— 不小于给定键(first_index)的第一个元素
  auto it = map_.lower_bound( first_index );
  if ( it != map_.begin() ) {
    auto prev_it = prev( it );
    if ( prev_it->first + prev_it->second.size() >= first_index ) {
      // 如果前一个区间完全包含了当前 data, 直接退出
      if ( prev_it->first + prev_it->second.size() >= first_index + data.size() ) {
        data.clear();
      } else {
        // 部分重叠: 将新 data 拼接到前一个区间, 并以其为起始
        data.insert( 0, prev_it->second.substr( 0, first_index - prev_it->first ) ); // 将前一部分独有的字节补到data
        first_index = prev_it->first;
        map_.erase( prev_it );
      }
    }
  }

  // 向后查找可能重叠的区间
  it = map_.lower_bound( first_index );
  while ( it != map_.end() && first_index + data.size() >= it->first ) {
    if ( first_index + data.size() < it->first + it->second.size() ) {
      // data 只覆盖了后一个区间的头部: 把后一个区间剩余部分接在 data 后面
      data += it->second.substr( first_index + data.size() - it->first );
    }
    // data 完全覆盖了后一个区间, 或者已经合并: 删除后一个区间
    it = map_.erase( it );
  }

  // 插入最终合并后的区间
  if ( !data.empty() ) {
    map_[first_index] = std::move( data );
  }

  // 从 map_ 中提取从 next_index_ 开始的连续块, 尝试写入 ByteStream
  while ( !map_.empty() && map_.begin()->first == next_index_ ) {
    auto& segment = map_.begin()->second;
    uint64_t bytes_pushed = segment.size();
    // 注意: 由于已经裁剪过容量, 正常情况下 push 应该能全吃掉
    output_.writer().push( std::move( segment ) );
    next_index_ += bytes_pushed;
    map_.erase( map_.begin() );
  }

  // 检查是否可以关闭流, 必须满足: 收到了 EOF 标志, 且当前重组进度已达到 EOF 索引
  if ( is_last_ && next_index_ == eof_index_ ) {
    output_.writer().close();
  }
}

uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t count = 0;
  for ( const auto& segment : map_ ) {
    count += segment.second.size();
  }
  return count;
}