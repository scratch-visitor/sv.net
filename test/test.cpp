#include <iostream>
#include <sv/net.hpp>

int main(int argc, const char** argv)
{
  std::cout << "sv.net version: " << sv::net::version() << std::endl;

  return 0;
}