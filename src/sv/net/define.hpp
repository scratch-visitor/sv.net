#ifndef __SV_NET_DEFINE_HPP__
#define __SV_NET_DEFINE_HPP__

#ifndef SV_NET_API
#ifdef SV_NET_EXPORTS
#define SV_NET_API __declspec(dllexport)
#else // SV_NET_EXPORTS
#define SV_NET_API __declspec(dllimport)
#pragma comment(lib, "sv.net.lib")
#endif // SV_NET_EXPORTS
#endif // SV_NET_API

#endif // __SV_NET_DEFINE_HPP__