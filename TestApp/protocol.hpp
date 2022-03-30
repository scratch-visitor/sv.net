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

#include "base.hpp"
#include "packet.hpp"

namespace sv
{
namespace net
{
namespace protocol
{

namespace asio = boost::asio;
using error_code = boost::system::error_code;
using tcp = asio::ip::tcp;

using namespace std::placeholders;
using namespace std::literals::chrono_literals;

template<class Packet>
struct base
{
  using packet_type = Packet;
  using packet_t = typename packet_type::ptr;
  using self = base<Packet>;
  using ptr = std::shared_ptr<self>;

  template<class...Args>
  static ptr make(Args&&...args)
  {
    return std::make_shared<self>(std::forward<Args>(args)...);
  }
  base(tcp::socket& _socket, id_type _session_id)
    : r_socket(_socket)
    , m_write_buffer()
    , m_read_buffer()
    , m_write_stream(&m_write_buffer)
    , m_read_stream(&m_read_buffer)
    , m_session_id(_session_id)
  {
  }
  virtual ~base()
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
    do_read_header();
  }
  void do_read_header()
  {
    asio::asio_read(r_socket,
                    m_read_buffer,
                    [&](error_code const& ec, std::size_t bytes)
                    {
                      if (!!ec)
                      {
                        on_error(ec, "read_header_condition");
                        return std::size_t(0);
                      }
                      return std::size_t(packet_type::header_size - bytes);
                    },
                    std::bind(&self::on_read_header, _1, _2));
  }
  void on_read_header(error_code const& ec, std::size_t bytes)
  {
    if (!!ec)
    {
      on_error(ec, "read_header");
      return;
    }

    packet_t packet = packet_type::make(m_session_id);
    packet->read_header(m_read_stream);

    do_read_body(packet);
  }
  void do_read_body(packet_t packet)
  {
    asio::async_read(r_socket,
                     m_read_buffer,
                     std::bind([&](error_code const& ec, std::size_t bytes, packet_t packet)
                               {
                                 if (!!ec)
                                 {
                                   on_error(ec, "read_body_condition");
                                   return std::size_t(0);
                                 }
                                 return std::size_t(packet->get_body_size() - bytes);
                               }, _1, _2, packet),
                     std::bind(&self::on_read_body, _1, _2, packet));
  }
  void on_read_body(error_code const& ec, std::size_t bytes, packet_t packet)
  {
    if (!!ec)
    {
      on_error(ec, "read_body");
      return;
    }

    packet->read_body(m_read_stream);
    m_read_depot.push_back(packet);

    do_read();
  }
  void do_write()
  {
    packet_t packet = nullptr;

    // TODO: blocking thread.
    do
    {
      if (m_write_depot.empty()) continue;

      packet = m_write_depot.front();
      m_write_depot.pop_front();
    } while (packet != nullptr);

    do_write_header(packet);
  }
  void do_write_header(packet_t packet)
  {
    packet->write_header(m_write_stream);
    asio::async_write(r_socket,
                      m_write_buffer,
                      [&](error_code const& ec, std::size_t bytes)
                      {
                        if (!!ec)
                        {
                          on_error(ec, "write_header_condition");
                          return std::size_t(0);
                        }
                        return std::size_t(packet_type::header_size - bytes);
                      },
                      std::bind(&self::on_write_header, this, _1, _2, packet));
  }
  void on_write_header(error_code const& ec, std::size_t bytes, packet_t packet)
  {
    if (!!ec)
    {
      on_error(ec, "write_header");
      return;
    }

    if (bytes > 0)
    {
      m_write_buffer.consume(bytes);
    }

    do_write_body(packet);
  }
  void do_write_body(packet_t packet)
  {
    packet->write_body(m_write_stream);
    asio::async_write(r_socket,
                      m_write_buffer,
                      std::bind([&](error_code const& ec, std::size_t bytes, packet_t packet)
                                {
                                  if (!!ec)
                                  {
                                    on_error(ec, "write_body_condition");
                                    return std::size_t(0);
                                  }
                                  return std::size_t(packet->get_body_size() - bytes);
                                }, _1, _2, packet),
                      std::bind(&self::on_write_body, this, _1, _2, packet));
  }
  void on_write_body(error_code const& ec, std::size_t bytes, packet_t packet)
  {
    if (!!ec)
    {
      on_error(ec, "write_body");
      return;
    }

    if (bytes > 0)
    {
      m_write_buffer.consume(bytes);
    }

    do_write();
  }
  void on_error(error_code const& ec, const char* where)
  {
    std::stringstream ss;
    ss << where << "::error[" << ec.value() << "]: " << ec.message() << '\n';
    std::cerr << ss.str() << std::flush;
  }

private:
  tcp::socket& r_socket;
  asio::streambuf m_write_buffer;
  asio::streambuf m_read_buffer;
  std::ostream m_write_stream;
  std::istream m_read_stream;

  std::deque<packet_t> m_write_depot;
  std::deque<packet_t> m_read_depot;

  id_type m_session_id;
};

using basic = base<packet::base>;

} // namespace sv::net::protocol
} // namespace sv::net
} // namespace sv

#endif // __SV_NET_PROTOCOL_HPP__