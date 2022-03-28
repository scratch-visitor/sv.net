#ifndef __SV_NET_PROTOCOL_HPP__
#define __SV_NET_PROTOCOL_HPP__

#include <iostream>
#include <string>
#include <memory>
#include <deque>
#include <sstream>

#ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0A00 // for Windows 10
#endif // _WIN32_WINNT

#include <boost/asio.hpp>

#include "packet.hpp"

namespace sv
{
namespace net
{
namespace protocol
{

namespace asio = boost::asio;
namespace system = boost::system;
using tcp = asio::ip::tcp;

template<bool is_server>
struct basic_protocol;

////////////////////////////////////////////////////////////////////////////////
// basic_protocol<true>, server_side
////////////////////////////////////////////////////////////////////////////////
template<>
struct basic_protocol<true>
{
  using self = basic_protocol<true>;
  using ptr = std::shared_ptr<self>;

  template<class...Args>
  static ptr make(Args&&...args)
  {
    return std::make_shared<self>(std::forward<Args>(args)...);
  }
public:
  basic_protocol(tcp::socket& _socket)
    : r_socket(_socket)
    , m_write_buffer()
    , m_read_buffer()
    , m_os(&m_write_buffer)
    , m_is(&m_read_buffer)
  {
  }
  virtual ~basic_protocol()
  {
  }
  void read()
  {
    do_read();
  }
  void write()
  {
    
  }
private:
  void do_read()
  {
    read_header();
  }
  void read_header()
  {
    std::cout << "try to read header" << std::endl;
    asio::async_read_until(
      r_socket,
      m_read_buffer,
      "\n\n",
      [this](system::error_code const& ec, std::size_t bytes)
      {
        on_read_header(ec, bytes);
      });
  }
  void on_read_header(system::error_code const& ec, std::size_t bytes_transferred)
  {
    std::cout << "read_header is done." << std::endl;

    if (!!ec)
    {
      on_error(ec);
      return;
    }

    //m_read_buffer.commit(bytes_transferred);

    {
      std::stringstream ss;
      ss << "header read: " << bytes_transferred << '\n';
      std::cout << ss.str();
    }

    m_header.put_header(m_is);
    //m_read_buffer.consume(bytes_transferred);

    {
      std::stringstream ss;
      ss
        << "version: " << m_header.version << '\n'
        << "type: " << m_header.type << '\n'
        << "length: " << m_header.length << '\n';
      std::cout << ss.str();
    }

    if (m_header.type == packet::string_body_type)
    {
      std::cout << "try to read string body" << std::endl;
      m_string_body = m_header;
      //m_read_buffer.prepare(m_string_body.length);
      asio::async_read(
        r_socket,
        m_read_buffer,
        [this](system::error_code const& ec, std::size_t bytes)
        {
          if (!!ec)
          {
            on_error(ec);
            return 0ULL;
          }
          return m_string_body.length - m_read_buffer.size();
        },
        [this](system::error_code const& ec, std::size_t bytes) { on_read_string(ec, bytes); }
      );
    }
    else if (m_header.type == packet::binary_body_type)
    {
      m_binary_body = m_header;
      m_binary_body.data = "test.binary"; // for test

      std::cout << "try to read binary body" << std::endl;
      asio::async_read(
        r_socket,
        m_read_buffer,
        [this](system::error_code const& ec, std::size_t bytes) { return m_binary_body.length - bytes; },
        [this](system::error_code const& ec, std::size_t bytes) { on_read_binary(ec, bytes); }
      );
    }
    else
    {
      std::stringstream ss;
      ss << "Unknown body type: " << m_header.type << '\n';
      std::cerr << ss.str();
    }
  }
  void on_read_string(system::error_code const& ec, std::size_t bytes_transferred)
  {
    std::cout << "reading string body is done." << std::endl;
    if (!!ec)
    {
      on_error(ec);
      return;
    }

    {
      std::stringstream ss;
      ss << "string body: " << bytes_transferred << '\n';
      std::cout << ss.str();
    }

    m_string_body.read(m_is);

    {
      std::stringstream ss;
      ss << "body: " << m_string_body.data << '\n';
      std::cout << ss.str();
    }

    do_read();
  }
  void on_read_binary(system::error_code const& ec, std::size_t bytes_transferred)
  {
    std::cout << "reading binary body is done." << std::endl;
    if (!!ec)
    {
      on_error(ec);
      return;
    }

    std::stringstream ss;
    ss << "binary body: " << bytes_transferred << '\n';
    std::cout << ss.str();

    m_binary_body.read(m_is);

    do_read();
  }
  void on_error(system::error_code const& ec)
  {
    std::stringstream ss;
    ss << "protocol::error[" << ec.value() << "]: " << ec.message() << '\n';
    std::cerr << ss.str();
  }

private:
  tcp::socket& r_socket;
  asio::streambuf m_write_buffer;
  asio::streambuf m_read_buffer;
  std::ostream m_os;
  std::istream m_is;

  packet::string_packet m_header;
  packet::string_packet m_string_body;
  packet::binary_packet m_binary_body;
};

////////////////////////////////////////////////////////////////////////////////
// basic_protocol<false>, client_side
////////////////////////////////////////////////////////////////////////////////
template<>
struct basic_protocol<false>
{
  using self = basic_protocol<false>;
  using ptr = std::shared_ptr<self>;

  template<class...Args>
  static ptr make(Args&&...args)
  {
    return std::make_shared<self>(std::forward<Args>(args)...);
  }
public:
  basic_protocol(tcp::socket& _socket)
    : r_socket(_socket)
    , m_write_buffer()
    , m_read_buffer()
    , m_os(&m_write_buffer)
    , m_is(&m_read_buffer)
  {
  }
  virtual ~basic_protocol()
  {

  }
  void read()
  {
    do_read();
  }
  void write()
  {
    do_write();
  }
private:
  void do_read()
  {
    read_header();
  }
  void read_header()
  {

  }
  void on_read_header(system::error_code const& ec, std::size_t bytes_transferred)
  {

  }
  void on_read_string(system::error_code const& ec, std::size_t bytes_transferred)
  {

  }
  void on_read_binary(system::error_code const& ec, std::size_t bytes_transferred)
  {

  }
  void do_write()
  {
    // for test
    packet::ptr test = packet::string_packet::make(std::string("hello scratch-visitor"));
    m_depot.push_back(test);

    if (m_depot.empty()) return;

    packet::ptr item = m_depot.front();
    m_depot.pop_front();

    item->get_header(m_os);

    auto size = m_write_buffer.size();
    std::cout << "write to size: " << size << std::endl;
    write_header(item);
  }
  void write_header(packet::ptr item)
  {
    std::cout << "try to write header." << std::endl;
    asio::async_write(
      r_socket,
      m_write_buffer,
      std::bind([&](system::error_code const& ec, std::size_t bytes, packet::ptr item)
      {
        on_write_header(ec, bytes, item);
      }, std::placeholders::_1, std::placeholders::_2, item));
  }
  void on_write_header(system::error_code const& ec, std::size_t bytes_transferred, packet::ptr item)
  {
    std::cout << "writing header is done." << std::endl;
    if (!!ec)
    {
      on_error(ec);
      return;
    }

    item->write(m_os);

    std::cout << "try to write body." << std::endl;
    asio::async_write(
      r_socket,
      m_write_buffer,
      std::bind([&](system::error_code const& ec, std::size_t bytes, packet::ptr item)
      {
        on_write(ec, bytes, item);
      }, std::placeholders::_1, std::placeholders::_2, item));
  }
  void on_write(system::error_code const& ec, std::size_t bytes_transferred, packet::ptr item)
  {
    std::cout << "writing body is done." << std::endl;
    if (!!ec)
    {
      on_error(ec);
      return;
    }

    std::stringstream ss;
    ss << bytes_transferred << " bytes transferred.\n";
    std::cout << ss.str();
  }
  void on_error(system::error_code const& ec)
  {
    std::stringstream ss;
    ss << "protocol::error[" << ec.value() << "]: " << ec.message() << '\n';
    std::cerr << ss.str();
  }

private:
  tcp::socket& r_socket;
  asio::streambuf m_write_buffer;
  asio::streambuf m_read_buffer;
  std::ostream m_os;
  std::istream m_is;

  packet::string_packet m_header;
  packet::string_packet m_string_body;
  packet::binary_packet m_binary_body;

  std::deque<packet::ptr> m_depot;
};

using server_side = basic_protocol<true>;
using client_side = basic_protocol<false>;

} // namespace sv::net::protocol
} // namespace sv::net
} // namespace sv

#endif // __SV_NET_PROTOCOL_HPP__