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

#pragma pack(push)
#pragma pack(1)
struct header
{
  std::uint32_t tag;
  std::uint32_t version;
  std::uint32_t type;
  std::uint64_t length;
};
#pragma pack(pop)

template<class Body>
struct basic;

struct base : public id_holder<base>
{
  using self = base;
  using ptr = std::shared_ptr<self>;

  static const std::uint32_t sc_tag = 0x12538253;
  static const std::uint32_t sc_header_size = sizeof(header);

  template<class Derived, class...Args>
  static ptr make(Args&&...args)
  {
    return std::dynamic_pointer_cast<self>(std::make_shared<Derived>(std::forward<Args>(args)...));
  }

  virtual void read_header(std::istream&) = 0;
  virtual void read_body(std::istream&) = 0;

  virtual void write_header(std::ostream&) = 0;
  virtual void write_body(std::ostream&) = 0;

  virtual std::size_t get_body_size() const = 0;

  header get_header() const
  {
    return m_header;
  }

protected:
  base(id_type const& _session_id)
    : m_session_id(_session_id)
  {
    m_header.tag = sc_tag;
    m_header.version = 1;
    m_header.type = empty_body_type;
    m_header.length = sc_header_size;
  }
  base(id_type const& _session_id, header const& _header)
    : m_session_id(_session_id)
    , m_header(_header)
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
  using base_ptr = typename base::ptr;

  template<class...Args>
  static base_ptr make(Args&&...args)
  {
    return base::make<self>(std::forward<Args>(args)...);
  }

  basic(value_type const& v, id_type const& _session_id)
    : base(_session_id)
    , m_value(v)
  {
    m_header.type = Body::body_type;
    m_body_size = Body::get_body_size();
    m_header.length += m_body_size;
  }
  basic(id_type const& _session_id)
    : base(_session_id)
  {
  }
  basic(id_type const& _session_id, header const& _header)
    : base(_session_id, _header)
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
    Body::write(os, m_value);
  }

  virtual std::size_t get_body_size() const override
  {
    return m_body_size;
  }

private:
  value_type m_value;

  std::size_t m_body_size;
};

namespace v1
{
struct void_t {};
struct empty_body
{
  using value_type = void_t;

  static const std::uint32_t body_type = empty_body_type;

  static void read_header(std::istream& is, header& pkt)
  {
    is.read(reinterpret_cast<char*>(&pkt), sizeof(header));
  }
  static void write_header(std::ostream& os, header const& pkt)
  {
    os.write(reinterpret_cast<const char*>(&pkt), sizeof(header));
  }
  static void read(std::istream& is, value_type& value, std::size_t length)
  {
  }
  static void write(std::ostream& os, value_type const& value)
  {
  }
  static std::size_t get_body_size(value_type const& _value)
  {
    return 0;
  }
};
struct string_body
{
  using value_type = std::string;

  static const std::uint32_t body_type = string_body_type;

  static void read_header(std::istream& is, header& pkt)
  {
    is.read(reinterpret_cast<char*>(&pkt), sizeof(header));
  }
  static void write_header(std::ostream& os, header const& pkt)
  {
    os.write(reinterpret_cast<const char*>(&pkt), sizeof(header));
  }
  static void read(std::istream& is, value_type& value, std::size_t length)
  {
    value.reserve(length);
    is.read(&value[0], length);
  }
  static void write(std::ostream& os, value_type const& value)
  {
    os.write(value.c_str(), value.length());
  }
  static std::size_t get_body_size(value_type const& _value)
  {
    return std::size_t(_value.length());
  }
};
struct binary_body
{
  using value_type = std::string;

  static const std::uint32_t body_type = binary_body_type;

  static void read_header(std::istream& is, header& pkt)
  {
    is.read(reinterpret_cast<char*>(&pkt), sizeof(header));
  }
  static void write_header(std::ostream& os, header const& pkt)
  {
    os.write(reinterpret_cast<const char*>(&pkt), sizeof(header));
  }
  static void read(std::istream& is, value_type& value, std::size_t length)
  {
    std::string root = R"("R:\")";
    std::string filepath = root + "\\" + value;
    std::fstream file(filepath, std::ios::out | std::ios::binary);
    if (!file.good())
    {
      return;
    }

    while (length > 0)
    {
      auto to_read = (length > 4096ULL) ? 4096ULL : length;

      std::vector<char> buffer(to_read, 0x00);
      is.read(&buffer[0], to_read);

      file.write(&buffer[0], to_read);
      file.flush();
    }

    file.close();
  }
  static void write(std::ostream& os, value_type const& value)
  {
    std::fstream file(value, std::ios::in | std::ios::binary);
    if (!file.good())
    {
      return;
    }

    std::vector<char> buffer(4096, 0x00);
    while (file)
    {
      auto pos = file.tellg();
      file.read(&buffer[0], 4096);
      auto current = file.tellg();
      auto diff = current - pos;
      os.write(&buffer[0], diff);
    }

    file.close();
  }
  static std::size_t get_body_size(value_type const& _value)
  {
    std::fstream file(_value, std::ios::in | std::ios::binary);
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
using empty_body = v1::empty_body;
using string_body = v1::string_body;
using binary_body = v1::binary_body;
} // namespace sv::net::packet::latest

using empty_packet = basic<latest::empty_body>;
using string_packet = basic<latest::string_body>;
using binary_packet = basic<latest::binary_body>;

} // namespace sv::net::packet
} // namespace sv::net
} // namespace sv

#endif // __SV_NET_PACKET_HPP__