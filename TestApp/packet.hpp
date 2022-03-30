#ifndef __SV_NET_PACKET_HPP__
#define __SV_NET_PACKET_HPP__

#include <iostream>
#include <fstream>
#include <cstdint>
#include <memory>
#include <string>
#include <sstream>
#include <unordered_map>

#include "base.hpp"

namespace sv
{
namespace net
{
namespace packet
{

enum type : std::uint32_t
{
  empty_body_type = 0,
  string_body_type,
  binary_body_type,
};

struct header
{
  std::uint32_t tag;
  std::uint32_t version;
  std::uint32_t type;
  std::uint64_t length;
};

template<class Body>
struct basic;

struct base : public id_holder<base>
{
  using self = base;
  using ptr = std::shared_ptr<self>;

  static const std::uint32_t sc_tag = 0x12538253;
  static const std::uint32_t header_size = sizeof(header);

  template<class Derived, class...Args>
  static ptr make(Args&&...args)
  {
    return std::make_shared<Derived>(std::forward<Args>(args)...);
  }

  virtual void read_header(std::istream&) = 0;
  virtual void read_body(std::istream&) = 0;

  virtual void write_header(std::ostream&) = 0;
  virtual void write_body(std::ostream&) = 0;

  virtual std::size_t get_body_size() const = 0;

protected:
  base(id_type const& _session_id)
    : m_session_id(_session_id)
  {
  }

protected:
  header m_header;
  id_type m_session_id;
};

template<class Body>
struct basic : public base
{
  using body_type = Body;

  using self = basic<body_type>;
  using ptr = std::shared_ptr<base>;
  using value_type = typename Body::value_type;

  basic(value_type const& v, id_type const& _session_id)
    : base(_session_id)
    , m_value(v)
  {
  }
  basic(id_type const& _session_id)
    : base(_session_id)
  {
  }
  virtual void read_header(std::istream& is) override
  {
    Body::read_header(is, m_header);
  }
  virtual void read_body(std::istream& is) override
  {
    Body::read(is, m_value, m_header.length);
  }

  virtual void write_header(std::ostream& os) override
  {
    Body::write_header(os, m_header);
  }
  virtual void write_body(std::ostream& os) override
  {
    Body::write();
  }

  virtual std::size_t get_body_size() const override
  {
    return std::size_t(m_header.length);
  }

private:
  value_type m_value;

  body_type m_body;
};

namespace v1
{
struct void_t {};
struct empty_body
{
  std::uint32_t tag;
  std::uint32_t version;
  std::uint32_t type;
  std::uint64_t length;

  using value_type = void_t;
  static void read(std::istream& is, value_type& value, std::size_t length)
  {}
  static std::size_t get_body_size(value_type const& _value)
  {
    return 0;
  }
};
struct string_body
{
  std::uint32_t tag;
  std::uint32_t version;
  std::uint32_t type;
  std::uint64_t length;

  using value_type = std::string;
  static void read(std::istream& is, value_type& value, std::size_t length)
  {
    value.reserve(length);
    is.read(&value[0], length);
  }
  static void write(std::ostream& os)
  {

  }
  std::size_t get_body_size(value_type const& _value)
  {
    return std::size_t(_value.length() + 1); // include null-character.
  }
  static void read_header(std::istream& is, header& pkt)
  {
    is.read(reinterpret_cast<char*>(&pkt.tag), sizeof(header::tag));
    is.read(reinterpret_cast<char*>(&pkt.version), sizeof(header::version));
    is.read(reinterpret_cast<char*>(&pkt.type), sizeof(header::type));
    is.read(reinterpret_cast<char*>(&pkt.length), sizeof(header::length));
  }
  static void write_header(std::ostream& os, header const& pkt)
  {
    os.write(reinterpret_cast<const char*>(&pkt.tag), sizeof(header::tag));
    os.write(reinterpret_cast<const char*>(&pkt.version), sizeof(header::version));
    os.write(reinterpret_cast<const char*>(&pkt.type), sizeof(header::type));
    os.write(reinterpret_cast<const char*>(&pkt.length), sizeof(header::length));
  }
};
struct binary_body
{
  std::uint32_t tag;
  std::uint32_t version;
  std::uint32_t type;
  std::uint64_t length;

  using value_type = std::string;
  static void read(std::istream& is)
  {
  }
  std::size_t get_body_size(value_type const& _value)
  {
    std::fstream file(_value, std::ios::in);
    if (!file.good())
    {
      return std::size_t(0);
    }
    file.seekg(0, std::ios::end);
    return std::size_t(file.tellg());
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