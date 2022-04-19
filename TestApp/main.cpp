#include <iostream>
#include <string>

#include "util.h"
#include "node.hpp"
#include "packet.hpp"
#include "protocol.hpp"

namespace sv
{
namespace app
{

class demo
{
  using server_t = sv::net::node::basic_tcp_server<sv::net::protocol::basic>;
  using client_t = sv::net::node::basic_tcp_client<sv::net::protocol::basic>;

public:
  demo()
    : m_server(nullptr)
    , m_client(nullptr)
  {
  }
  ~demo()
  {
  }
  void run()
  {
    do
    {
      std::string line;
      do
      {
        std::cout << "command:> ";
        std::getline(std::cin, line);
      } while (line.empty());

      auto tokens = sv::util::tokenize(line);
      // exit | quit
      if (tokens.size() == 1 && (tokens[0] == "exit" || tokens[0] == "quit"))
      {
        std::cout << "terminating program...";
        break;
      }
      // client [target] [port]
      else if (tokens.size() == 3 && tokens[0] == "client")
      {
        on_client(tokens[1], static_cast<unsigned short>(std::stoi(tokens[2])));
      }
      // server [port]
      else if (tokens.size() == 2 && tokens[0] == "server")
      {
        on_server(static_cast<unsigned short>(std::stoi(tokens[1])));
      }
    } while (true);
  }
private:
  void on_client(std::string const& _target, unsigned short _port)
  {
    m_client = client_t::make();
    m_client->execute(_target, _port);
  }
  void on_server(unsigned short _port)
  {
    m_server = server_t::make();
    m_server->execute(_port);
  }
private:
  server_t::ptr m_server;
  client_t::ptr m_client;
};

} // namespace sv::app
} // namespace sv

int main()
{
  try
  {
    sv::app::demo sample;
    sample.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "exception: " << e.what() << std::endl;
  }

  std::cout << "finished." << std::endl;
  return 0;
}