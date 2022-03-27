#ifndef __SV_UTIL_HPP__
#define __SV_UTIL_HPP__

#include <vector>
#include <string>

namespace sv
{
namespace util
{

std::vector<std::string>
tokenize(std::string const& _src, std::string const& _delimiter = " \t")
{
  std::vector<std::string> result;
  bool need_to_insert = true;
  for (auto const& c : _src)
  {
    bool is_delimiter = (_delimiter.find(c) != std::string::npos);
    if (is_delimiter)
    {
      need_to_insert = true;
    }
    else
    {
      if (need_to_insert)
      {
        result.push_back(std::string(1, c));
        need_to_insert = false;
      }
      else
      {
        result.back().push_back(c);
      }
    }
  }

  return result;
}

} // namespace sv::util
} // namespace sv

#endif // __SV_UTIL_HPP__