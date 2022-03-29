#include <iostream>
#include <string>
#include <thread>
#include <sstream>
#include <functional>
#include <deque>
#include <mutex>
#include <chrono>
#include <list>
#include <condition_variable>
#include <cstdint>

void server();
void client();

namespace test
{
int main()
{
  try
  {
    int mode;
    std::cout << "server(even), client(odd):> ";
    std::cin >> mode;

    if (mode % 2 == 0)
    {
      server();
    }
    else
    {
      client();
    }
  }
  catch (std::exception& e)
  {
    std::cerr << "exception: " << e.what() << std::endl;
  }

  return 0;
}
}

#ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0A00 // Windows 10
#endif // _WIN32_WINNT

#include <boost/asio.hpp>

namespace asio = boost::asio;
using error_code = boost::system::error_code;
using tcp = asio::ip::tcp;
using socket_t = std::shared_ptr<tcp::socket>;

struct packet_t
{
  static const std::size_t header_size = 4;

  std::uint32_t length; // packet size = 4 + message.length + 1
  std::string message;

  void write(std::ostream& os)
  {
    os.write((const char*)&length, sizeof(length));
    os.write(message.c_str(), message.length() + 1); // include null-character.
  }
  void read(std::istream& is)
  {
    is.read((char*)&length, sizeof(length));
    message.reserve(length);
    is.read(&message[0], length - 4); // length include 4 bytes more.
  }
};

void server()
{
  using namespace std::placeholders;
  using namespace std::literals::chrono_literals;

  std::mutex mu;
  std::deque<std::string> depot;

  asio::io_context ioc;
  asio::executor_work_guard<asio::io_context::executor_type> guard(ioc.get_executor());
  asio::strand<asio::io_context::executor_type> strand(ioc.get_executor());
  std::thread worker([&]() { ioc.run(); });

  std::list<socket_t> sessionlist;

  unsigned short port = 9999;
  // open, set_option, bind, listen
  tcp::acceptor acceptor(ioc, tcp::endpoint(asio::ip::address(), port));

  asio::streambuf write_buffer;
  asio::streambuf read_buffer;
  std::ostream write_stream(&write_buffer);
  std::istream read_stream(&read_buffer);

  std::function<void()> do_accept = nullptr;
  std::function<void(socket_t)> do_session = nullptr;
  std::function<void(socket_t)> do_read_header = nullptr;
  std::function<void(socket_t, std::uint32_t)> do_read = nullptr;
  std::function<void(socket_t)> do_write = nullptr;

  auto on_error = [&](error_code const& ec, const char* where)
  {
    std::stringstream ss;
    ss << where << "::error[" << ec.value() << "]:" << ec.message() << "\n";
    std::cerr << ss.str() << std::flush;
  };
  auto on_accepted = [&](error_code const& ec, socket_t socket)
  {
    std::cout << "on_accepted\n" << std::flush;
    if (!!ec)
    {
      on_error(ec, "accept");
      return;
    }

    do_accept();

    do_session(socket);
  };
  do_accept = [&]()
  {
    std::cout << "do_accepted\n" << std::flush;
    auto socket = std::make_shared<tcp::socket>(ioc);
    sessionlist.push_back(socket);
    acceptor.async_accept(*socket.get(),
                          std::bind(on_accepted, _1, socket));
  };
  do_session = [&](socket_t socket)
  {
    std::cout << "do_session\n" << std::flush;
    do_read_header(socket);
    do_write(socket);
  };
  auto on_read_header = [&](error_code const& ec, std::size_t bytes, socket_t socket)
  {
    std::cout << "on_read_header\n" << std::flush;
    if (!!ec)
    {
      on_error(ec, "read_header");
      return;
    }

    std::uint32_t length;
    read_stream.read((char*)&length, sizeof(length));
    std::stringstream ss;
    ss << "[size]" << length << '\n';
    std::cout << ss.str() << std::flush;

    do_read(socket, length - packet_t::header_size);
  };
  do_read_header = [&](socket_t socket)
  {
    std::cout << "do_read_header\n" << std::flush;
    asio::async_read(*socket.get(),
                     read_buffer,
                     [&](error_code const& ec, std::size_t bytes)
                     {
                       std::stringstream ss;
                       ss << "[buffer]" << read_buffer.size() << '\n';
                       ss << "[bytes]" << bytes << '\n';
                       std::cout << ss.str() << std::flush;

                       if (!!ec)
                       {
                         on_error(ec, "read_header_condition");
                         return std::size_t(0);
                       }

                       return std::size_t(packet_t::header_size - bytes);
                     },
                     std::bind(on_read_header, _1, _2, socket));
  };
  auto on_read = [&](error_code const& ec, std::size_t bytes, socket_t socket, std::uint32_t length)
  {
    std::cout << "on_read\n" << std::flush;
    if (!!ec)
    {
      on_error(ec, "read");
      return;
    }

    std::string message(length, '\0');
    read_stream.read(&message[0], length);
    std::stringstream ss;
    ss << "[packet]" << message << '\n';
    std::cout << ss.str() << std::flush;

    do_read_header(socket);
  };
  do_read = [&](socket_t socket, std::uint32_t length)
  {
    std::cout << "do_read\n" << std::flush;
    asio::async_read(*socket.get(),
                     read_buffer,
                     std::bind([&](error_code const& ec, std::size_t bytes, std::uint32_t length)
                               {
                                 if (!!ec)
                                 {
                                   on_error(ec, "read_condition");
                                   return std::size_t(0);
                                 }
                                 return std::size_t(length - bytes);
                               }, _1, _2, length),
                     std::bind(on_read, _1, _2, socket, length));
  };
  auto on_write = [&](error_code const& ec, std::size_t bytes, socket_t socket)
  {
    if (!!ec)
    {
      on_error(ec, "write");
      return;
    }

    if (bytes > 0)
    {
      std::stringstream ss;
      ss << bytes << " is written.\n";
      std::cout << ss.str() << std::flush;
      write_buffer.consume(bytes);
    }

    do_write(socket);
  };
  do_write = [&](socket_t socket)
  {
    asio::async_write(*socket.get(),
                      write_buffer.data(),
                      std::bind(on_write, _1, _2, socket));
  };

  // start
  do_accept();

  do
  {
    std::string line;
    do
    {
      std::cout << "> ";
      std::getline(std::cin, line);
    } while (line.empty());

    if (line == "quit" || line == "exit")
    {
      break;
    }
    else
    {
      packet_t data{ std::uint32_t(5 + line.length()), line };
      data.write(write_stream);
      //write_stream << line << std::flush;
    }

  } while (true);

  if (acceptor.is_open())
    acceptor.close();

  for (auto& e : sessionlist)
  {
    if (e->is_open())
    {
      e->close();
    }
  }
  sessionlist.clear();

  guard.reset();
  worker.join();
}

void client()
{
  using namespace std::placeholders;
  using namespace std::literals::chrono_literals;

  std::mutex mu;
  std::condition_variable cv;
  std::deque<std::string> depot;

  asio::io_context ioc;
  asio::executor_work_guard<asio::io_context::executor_type> guard(ioc.get_executor());
  asio::strand<asio::io_context::executor_type> strand(ioc.get_executor());
  std::thread worker([&]() { ioc.run(); });

  unsigned short port = 9999;
  // open, set_option, bind, listen
  tcp::endpoint target(asio::ip::address_v4::from_string("127.0.0.1"), port);
  socket_t socket = nullptr;

  asio::streambuf write_buffer;
  asio::streambuf read_buffer;
  std::ostream write_stream(&write_buffer);
  std::istream read_stream(&read_buffer);

  std::function<void()> do_connect = nullptr;
  std::function<void(socket_t)> do_session = nullptr;
  std::function<void(socket_t)> do_read_header = nullptr;
  std::function<void(socket_t, std::uint32_t)> do_read = nullptr;
  std::function<void(socket_t)> do_write = nullptr;

  auto on_error = [&](error_code const& ec, const char* where)
  {
    std::stringstream ss;
    ss << where << "::error[" << ec.value() << "]:" << ec.message() << "\n";
    std::cerr << ss.str() << std::flush;
  };
  auto on_connect = [&](error_code const& ec, socket_t socket)
  {
    std::cout << "on_connect\n" << std::flush;
    if (!!ec)
    {
      on_error(ec, "accept");
      return;
    }

    do_session(socket);
  };
  do_connect = [&]()
  {
    std::cout << "do_connect\n" << std::flush;
    tcp::resolver resolver(ioc);
    auto results = resolver.resolve(target);


    socket = std::make_shared<tcp::socket>(ioc);
    asio::async_connect(*socket.get(),
                        results.begin(),
                        std::bind(on_connect, _1, socket));
  };
  do_session = [&](socket_t socket)
  {
    std::cout << "do_session\n" << std::flush;
    do_read_header(socket);
    do_write(socket);
  };
  auto on_read_header = [&](error_code const& ec, std::size_t bytes, socket_t socket)
  {
    std::cout << "on_read_header\n" << std::flush;
    if (!!ec)
    {
      on_error(ec, "read_header");
      return;
    }

    std::uint32_t length;
    read_stream.read((char*)&length, sizeof(length));
    std::stringstream ss;
    ss << "[size]" << length << '\n';
    std::cout << ss.str() << std::flush;

    do_read(socket, length - packet_t::header_size);
  };
  do_read_header = [&](socket_t socket)
  {
    std::cout << "do_read_header\n" << std::flush;
    asio::async_read(*socket.get(),
                     read_buffer,
                     [&](error_code const& ec, std::size_t bytes)
                     {
                       if (!!ec)
                       {
                         on_error(ec, "read_header_condition");
                         return std::size_t(0);
                       }
                       return std::size_t(packet_t::header_size - bytes);
                     },
                     std::bind(on_read_header, _1, _2, socket));
  };
  auto on_read = [&](error_code const& ec, std::size_t bytes, socket_t socket, std::uint32_t length)
  {
    std::cout << "on_read\n" << std::flush;
    if (!!ec)
    {
      on_error(ec, "read");
      return;
    }

    std::string message(length, '\0');
    read_stream.read(&message[0], length);
    std::stringstream ss;
    ss << "[packet]" << message << '\n';
    std::cout << ss.str() << std::flush;

    do_read_header(socket);
  };
  do_read = [&](socket_t socket, std::uint32_t length)
  {
    std::cout << "do_read\n" << std::flush;
    asio::async_read(*socket.get(),
                     read_buffer,
                     std::bind([&](error_code const& ec, std::size_t bytes, std::uint32_t length)
                               {
                                 if (!!ec)
                                 {
                                   on_error(ec, "read_condition");
                                   return std::size_t(0);
                                 }
                                 return std::size_t(length - bytes);
                               }, _1, _2, length),
                     std::bind(on_read, _1, _2, socket, length));
  };
  auto on_write = [&](error_code const& ec, std::size_t bytes, socket_t socket)
  {
    if (!!ec)
    {
      on_error(ec, "write");
      return;
    }

    if (bytes > 0)
    {
      std::stringstream ss;
      ss << bytes << " is written.\n";
      std::cout << ss.str() << std::flush;

      write_buffer.consume(bytes);
    }

    do_write(socket);
  };
  do_write = [&](socket_t socket)
  {
    asio::async_write(*socket.get(),
                      write_buffer.data(),
                      std::bind(on_write, _1, _2, socket));
  };

  // start
  do_connect();

  do
  {
    std::string line;
    do
    {
      std::cout << "> ";
      std::getline(std::cin, line);
    } while (line.empty());

    if (line == "quit" || line == "exit")
    {
      break;
    }
    else
    {
      packet_t data{ std::uint32_t(5 + line.length()), line };
      data.write(write_stream);
      //write_stream << line << std::flush;
    }

  } while (true);

  if (socket->is_open())
  {
    socket->close();
  }

  socket = nullptr;

  guard.reset();
  worker.join();
}