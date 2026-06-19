#pragma once


#include <cstdint>

#ifndef OD_PLATFORM_BASIC_TYPES_DEFINED
#define OD_PLATFORM_BASIC_TYPES_DEFINED
typedef std::int8_t OdInt8;
typedef std::uint8_t OdUInt8;
typedef std::int16_t OdInt16;
typedef std::uint16_t OdUInt16;
typedef std::int32_t OdInt32;
typedef std::uint32_t OdUInt32;
typedef std::int64_t OdInt64;
typedef std::uint64_t OdUInt64;
#endif

// Minimal compatibility header for building the ODA-modified DWF Toolkit
// outside a full ODA SDK checkout. The upstream ODA package expects this
// header from ODA KernelBase. This shim only provides the macros used by
// DWFToolkit-7.7 sources in this repository.

#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
  #ifndef ODA_BIGENDIAN
    #define ODA_BIGENDIAN 1
  #endif
#endif

#ifndef ODA_ASSERT_VAR
  #ifdef NDEBUG
    #define ODA_ASSERT_VAR(code)
  #else
    #define ODA_ASSERT_VAR(code) code
  #endif
#endif
#ifndef ODA_ASSERT_ONCE_X
  #define ODA_ASSERT_ONCE_X(group, expr) ((void)0)
#endif
