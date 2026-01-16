#include "debug.hh"
#include "socket.hh"

#include <cstdlib>
#include <iostream>
#include <span>
#include <string>

using namespace std;

namespace {
void get_URL( const string& host, const string& path )
{
  // 函数接收两个参数: 域名主机(会进行DNS解析) 和 路径
  Address addr( host, "http" );// 服务名"http"默认对应80端口
  TCPSocket sock;
  sock.connect(addr);// 构建了套接字连接

  // 在这里构造一个GET请求
  string msg = "";
  msg += "GET " + path + " HTTP/1.1\r\n";
  msg += "Host: " + host + "\r\n";
  msg += "Connection: close\r\n";
  msg += "\r\n";
  sock.write( msg );

  // 读取请求的返回内容, 套接字读取到EOF（文件末尾）, 则结束
  while(!sock.eof()){
    sock.read( msg );
    cout << msg;
  }

  sock.close();
  // debug( "Function called: get_URL( \"{}\", \"{}\" )", host, path );
  // debug( "get_URL() function not yet implemented" );
}
} // namespace

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort(); // For sticklers: don't try to access argv[0] if argc <= 0.
    }

    auto args = span( argv, argc );

    // The program takes two command-line arguments: the hostname and "path" part of the URL.
    // Print the usage message unless there are these two arguments (plus the program name
    // itself, so arg count = 3 in total).
    if ( argc != 3 ) {
      cerr << "Usage: " << args.front() << " HOST PATH\n";
      cerr << "\tExample: " << args.front() << " stanford.edu /class/cs144\n";
      return EXIT_FAILURE;
    }

    // Get the command-line arguments.
    const string host { args[1] };
    const string path { args[2] };

    // Call the student-written function.
    get_URL( host, path );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
