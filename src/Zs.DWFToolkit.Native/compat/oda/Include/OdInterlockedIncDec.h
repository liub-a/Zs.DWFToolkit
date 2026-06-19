#pragma once

#if defined(_MSC_VER)
  #include <intrin.h>
#endif

inline long OdInterlockedIncrement(long volatile* value)
{
#if defined(_MSC_VER)
    return _InterlockedIncrement(value);
#else
    return __sync_add_and_fetch(value, 1);
#endif
}

inline long OdInterlockedDecrement(long volatile* value)
{
#if defined(_MSC_VER)
    return _InterlockedDecrement(value);
#else
    return __sync_sub_and_fetch(value, 1);
#endif
}

inline int OdInterlockedIncrement(int volatile* value)
{
#if defined(_MSC_VER)
    return static_cast<int>(_InterlockedIncrement(reinterpret_cast<long volatile*>(value)));
#else
    return __sync_add_and_fetch(value, 1);
#endif
}

inline int OdInterlockedDecrement(int volatile* value)
{
#if defined(_MSC_VER)
    return static_cast<int>(_InterlockedDecrement(reinterpret_cast<long volatile*>(value)));
#else
    return __sync_sub_and_fetch(value, 1);
#endif
}
