#ifndef __SV_NET_PACKET_HPP__
#define __SV_NET_PACKET_HPP__

#include <iostream>
#include <fstream>
#include <cstdint>
#include <memory>
#include <string>
#include <sstream>

namespace sv
{
namespace net
{
namespace packet
{

enum packet_type : std::uint32_t
{
  empty_body_type = 0,
  string_body_type,
  binary_body_type,
};

struct packet_base
{
  std::int32_t version;
  std::int32_t type;
  std::uint64_t length;
  //std::chrono::system_clock::time_point timestamp;

  virtual void write(std::ostream&) = 0;
  virtual void read(std::istream&) = 0;
  virtual void get_header(std::ostream&) = 0;
  virtual void put_header(std::istream&) = 0;
};

using ptr = std::shared_ptr<packet_base>;

template<class Body>
struct basic_packet : public packet_base
{
  using base = packet_base;
  using self = basic_packet<Body>;
  using value_type = typename Body::value_type;
  value_type data;

  template<class...Args>
  static ptr make(Args&&...args)
  {
    return std::make_shared<self>(std::forward<Args>(args)...);
  }

  basic_packet(value_type const& v = value_type())
    : data(v)
  {
  }
  void write(std::ostream& os) override
  {
    Body::write(os, data);
  }
  void read(std::istream& is) override
  {
    Body::read(is, data);
  }
  void get_header(std::ostream& os) override
  {
    version = Body::get_version();
    type = Body::get_type();
    length = Body::get_length(data);

    os
      << "Version: " << version << '\n'
      << "Type: " << type << '\n'
      << "Length: " << length << '\n'
      << '\n';
  }
  void put_header(std::istream& is) override
  {
    std::string line;
    while (std::getline(is, line))
    {
      if (line.empty()) break;

      auto tag = line.substr(0, line.find(':'));
      if (tag == "Version")
      {
        auto value = line.substr(line.find(':') + 1);
        version = std::stoi(value);
      }
      else if (tag == "Type")
      {
        auto value = line.substr(line.find(':') + 1);
        type = std::stoi(value);
      }
      else if (tag == "Length")
      {
        auto value = line.substr(line.find(':') + 1);
        length = std::stoull(value);
      }
      else
      {
        std::stringstream ss;
        ss << "Unknown header: " << line << '\n';
        std::cerr << ss.str();
      }
    }
  }

  template<class Other>
  basic_packet<Body>& operator = (basic_packet<Other> const& other)
  {
    version = other.version;
    type = other.type;
    length = other.length;

    return *this;
  }
};

namespace v1
{
static const int sc_version = 10;
struct string_body
{
  using value_type = std::string;

  static void write(std::ostream& os, value_type const& value)
  {
    os << value;
  }
  static void read(std::istream& is, value_type& value)
  {
    std::getline(is, value);
  }
  static int get_version()
  {
    return sc_version;
  }
  static int get_type()
  {
    return string_body_type;
  }
  static std::uint64_t get_length(value_type const& value)
  {
    return static_cast<std::uint64_t>(value.length());
  }
};

struct binary_body
{
  using value_type = std::string;

  static void write(std::ostream& os, value_type const& value)
  {
    std::fstream file;
    file.open(value, std::ios::binary | std::ios::in);
    if (!file.good())
    {
      std::stringstream ss;
      ss << "opening file is failed: " << value << '\n';
      std::cerr << ss.str();
      return;
    }

    std::size_t size = 1024;
    std::size_t read_size = 0;
    char buffer[1024];

    while (file)
    {
      file.read(buffer, size);
      read_size = file.gcount();
      os.write(buffer, read_size);
    }
    file.close();
  }
  static void read(std::istream& is, value_type& value)
  {
    const std::string directory = "R:\\test\\"; // for test
    std::string filename = directory + value;

    std::fstream file;
    file.open(filename, std::ios::binary | std::ios::out);
    if (!file.good())
    {
      std::stringstream ss;
      ss << "opening file is failed: " << filename << '\n';
      std::cerr << ss.str();
      return;
    }

    std::size_t size = 1024;
    std::size_t read_size = 0;
    char buffer[1024];

    while (is)
    {
      is.read(buffer, size);
      read_size = is.gcount();
      file.write(buffer, read_size);
    }

    file.close();
  }
  static int get_version()
  {
    return sc_version;
  }
  static int get_type()
  {
    return binary_body_type;
  }
  static std::uint64_t get_length(value_type const& value)
  {
    std::fstream file;
    std::uint64_t size = 0;
    file.open(value, std::ios::binary | std::ios::in);
    if (!file.good())
    {
      std::stringstream ss;
      ss << "opening file is failed: " << value << '\n';
      std::cerr << ss.str();
      return size;
    }

    file.seekg(0, std::ios::end);
    size = file.tellg();

    file.close();

    return size;
  }
};
} // namespace sv::net::packet::v1

namespace latest
{
using string_body = v1::string_body;
using binary_body = v1::binary_body;
} // namespace sv::net::packet::latest

using string_packet = basic_packet<latest::string_body>;
using binary_packet = basic_packet<latest::binary_body>;

} // namespace sv::net::packet
} // namespace sv::net
} // namespace sv

#endif // __SV_NET_PACKET_HPP__