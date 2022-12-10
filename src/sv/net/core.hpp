#ifndef __SV_NET_CORE_HPP__
#define __SV_NET_CORE_HPP__
#pragma once

#include "sv/net/define.hpp"

#include <cstdint>

namespace sv
{

using id_type = std::uint64_t;

template<class T>
struct id_holder
{
  id_type id() const noexcept { return id_; }
protected:
  id_holder()
    : id_(s_id_++)
  {}

private:
  id_type id_;
  static id_type s_id_;
};
template<class T>
id_type id_holder<T>::s_id_ = 0ULL;

namespace net
{

class SV_NET_API node : public id_holder<node>
{
protected:
  node();

public:
  virtual ~node();
};

} // namespace sv::net
} // namespace sv

#endif // __SV_NET_CORE_HPP__