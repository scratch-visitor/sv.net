#ifndef __SV_NET_NODE_HPP__
#define __SV_NET_NODE_HPP__

#include <iostream>
#include <unordered_map>
#include <string>
#include <thread>
#include <memory>
#include <sstream>

#ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0A00 // for Windows 10
#endif // _WIN32_WINNT

#include <boost/asio.hpp>

#include "base.hpp"

namespace sv
{
namespace net
{
namespace node
{

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using error_code = boost::system::error_code;

using namespace std::placeholders;
using namespace std::literals::chrono_literals;

////////////////////////////////////////////////////////////////////////////////
// basic_server
////////////////////////////////////////////////////////////////////////////////
template<class _Acceptor>
struct basic_server
{
  using self = basic_server<_Acceptor>;
  using ptr = std::shared_ptr<self>;

  template<class...Args>
  static ptr make(Args&&...args)
  {
    return std::make_shared<self>(std::forward<Args>(args)...);
  }
  basic_server()
    : m_ioc()
    , m_work_guard(m_ioc.get_executor())
    , m_worker([&]() { m_ioc.run(); })
    , m_acceptor(nullptr)
  {
  }
  virtual ~basic_server()
  {
    m_acceptor = nullptr;

    m_work_guard.reset();

    if (m_worker.joinable())
      m_worker.join();
  }
  void execute(unsigned short _port)
  {
    m_acceptor = _Acceptor::make(m_ioc, _port);
    m_acceptor->execute();
  }

private:
  asio::io_context m_ioc;
  asio::executor_work_guard<asio::io_context::executor_type> m_work_guard;
  std::thread m_worker;

  typename _Acceptor::ptr m_acceptor;
};

////////////////////////////////////////////////////////////////////////////////
// basic_acceptor
////////////////////////////////////////////////////////////////////////////////
template<class _Session>
struct basic_acceptor
{
  using self = basic_acceptor<_Session>;
  using ptr = std::shared_ptr<self>;

  template<class...Args>
  static ptr make(Args&&...args)
  {
    return std::make_shared<self>(std::forward<Args>(args)...);
  }

  basic_acceptor(asio::io_context& _ioc, unsigned short _port)
    : r_ioc(_ioc)
    , m_acceptor(r_ioc, tcp::endpoint(asio::ip::address(), _port))
  {
  }
  virtual ~basic_acceptor()
  {
    if (m_acceptor.is_open())
    {
      m_acceptor.close();
    }

    for (auto& e : m_session_depot)
    {
      if (e->socket().is_open())
      {
        e->socket().close();
      }
    }
    m_session_depot.clear();
  }
  void execute()
  {
    do_accept();
  }

private:
  void do_accept()
  {
    auto session = _Session::make(r_ioc);
    m_session_depot.push_back(session);
    m_acceptor.async_accept(session->socket(),
                            std::bind(&self::on_accepted, this, _1, session));
  }
  void on_accepted(error_code const& ec, typename _Session::ptr session)
  {
    if (!!ec)
    {
      on_error(ec, "accept");
      return;
    }

    std::stringstream ss;
    ss << "accepted from " << session->socket().remote_endpoint() << "\n";
    std::cout << ss.str() << std::flush;

    do_accept();

    session->execute();
  }
  void on_error(error_code const& ec, const char* where)
  {
    std::stringstream ss;
    ss << where << "::error[" << ec.value() << "]: " << ec.message() << '\n';
    std::cerr << ss.str() << std::flush;
  }

private:
  asio::io_context& r_ioc;

  tcp::acceptor m_acceptor;
  std::list<typename _Session::ptr> m_session_depot;
};

////////////////////////////////////////////////////////////////////////////////
// basic_client
////////////////////////////////////////////////////////////////////////////////
template<class _Connector>
struct basic_client
{
  using self = basic_client<_Connector>;
  using ptr = std::shared_ptr<self>;

  template<class...Args>
  static ptr make(Args&&...args)
  {
    return std::make_shared<self>(std::forward<Args>(args)...);
  }
public:
  basic_client()
    : m_ioc()
    , m_work_guard(m_ioc.get_executor())
    , m_worker([&]() { m_ioc.run(); })
    , m_connector(nullptr)
  {
  }
  virtual ~basic_client()
  {
    m_connector = nullptr;

    m_work_guard.reset();

    if (m_worker.joinable())
      m_worker.join();
  }
  void execute(std::string const& _target, unsigned short _port)
  {
    m_connector = _Connector::make(m_ioc, _target, _port);

    m_connector->execute();
  }

private:
  asio::io_context m_ioc;
  asio::executor_work_guard<asio::io_context::executor_type> m_work_guard;
  std::thread m_worker;

  typename _Connector::ptr m_connector;
};

////////////////////////////////////////////////////////////////////////////////
// basic_connector
////////////////////////////////////////////////////////////////////////////////
template<class _Session>
struct basic_connector
{
  using self = basic_connector<_Session>;
  using ptr = std::shared_ptr<self>;

  template<class...Args>
  static ptr make(Args&&...args)
  {
    return std::make_shared<self>(std::forward<Args>(args)...);
  }
public:
  basic_connector(asio::io_context& _ioc, std::string const& _target, unsigned short _port)
    : r_ioc(_ioc)
    , m_resolver(r_ioc)
    , m_target(_target)
    , m_port(_port)
  {
  }
  virtual ~basic_connector()
  {
  }
  void execute()
  {
    do_resolve();
  }
private:
  void do_resolve()
  {
    tcp::endpoint target(asio::ip::address::from_string(m_target), m_port);
    m_resolver.async_resolve(target,
                             std::bind(&self::on_resolved, this, _1, _2));
  }
  void on_resolved(error_code const& ec, tcp::resolver::results_type const& results)
  {
    if (!!ec)
    {
      on_error(ec, "resolve");
      return;
    }

    auto session = _Session::make(r_ioc);

    std::stringstream ss;
    ss << "try to connect to [";
    for (auto const& ep : results)
    {
      ss << ep.endpoint() << ' ';
    }
    ss << "]\n";
    std::cout << ss.str() << std::flush;

    asio::async_connect(session->socket(),
                        results,
                        std::bind(&self::on_connected, this, _1, _2, session));
  }
  void on_connected(error_code const& ec, tcp::endpoint const& ep, typename _Session::ptr session)
  {
    if (!!ec)
    {
      on_error(ec, "connect");
      return;
    }

    std::stringstream ss;
    ss << "connected to " << session->socket().remote_endpoint() << '(' << session->socket().local_endpoint() << ")\n";
    std::cout << ss.str();

    session->execute();
  }
  void on_error(error_code const& ec, const char* where)
  {
    std::stringstream ss;
    ss << where << "::error[" << ec.value() << "]: " << ec.message() << '\n';
    std::cerr << ss.str() << std::flush;
  }

private:
  asio::io_context& r_ioc;
  tcp::resolver m_resolver;

  std::string m_target;
  unsigned short m_port;
};

////////////////////////////////////////////////////////////////////////////////
// basic_session
////////////////////////////////////////////////////////////////////////////////
template<class _Protocol>
struct basic_session : public id_holder<basic_session<_Protocol>>
{
  using base = id_holder<basic_session<_Protocol>>;
  using self = basic_session<_Protocol>;
  using ptr = std::shared_ptr<self>;

  template<class...Args>
  static basic_session::ptr make(Args&&...args)
  {
    return std::make_shared<self>(std::forward<Args>(args)...);
  }
public:
  basic_session(asio::io_context& _ioc)
    : m_socket(_ioc)
    , m_protocol(nullptr)
  {
  }
  virtual ~basic_session()
  {
    if (m_socket.is_open())
    {
      m_socket.close();
    }

    m_protocol = nullptr;
  }
  tcp::socket& socket()
  {
    return m_socket;
  }
  const tcp::socket& socket() const
  {
    return m_socket;
  }
  void execute()
  {
    m_protocol = _Protocol::make(m_socket, base::id());

    do_read();
    do_write();
  }
private:
  void do_read()
  {
    m_protocol->read();
  }
  void do_write()
  {
    m_protocol->write();
  }
private:
  tcp::socket m_socket;
  typename _Protocol::ptr m_protocol;
};

template<class Proto>
using basic_tcp_server = basic_server<basic_acceptor<basic_session<Proto>>>;
template<class Proto>
using basic_tcp_client = basic_client<basic_connector<basic_session<Proto>>>;

} // namespace sv::net::node
} // namespace sv::net
} // namespace sv

#endif // __SV_NET_NODE_HPP__