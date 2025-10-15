#pragma once

#include <iostream>
#include <limits>

namespace NH3D {

#define NH3D_MIN_T(T) std::numeric_limits<T>::min()
#define NH3D_MAX_T(T) std::numeric_limits<T>::max()

#define _NH3D_ENDLOG " - " << __FILE__ << ":" << __LINE__ << "\033[0m" << std::endl

#define NH3D_LOG(msg) std::cout << "[LOG]: " << msg << _NH3D_ENDLOG
#define NH3D_LOG_VK(msg) std::cout << "[VK LOG]: " << msg << _NH3D_ENDLOG
#define NH3D_WARN(msg) std::cerr << "\033[93m[WARNING]: " << msg << _NH3D_ENDLOG
#define NH3D_WARN_VK(msg) std::cerr << "\033[93m[VK WARNING]: " << msg << _NH3D_ENDLOG
#define NH3D_ERROR(msg) std::cerr << "\033[31m[ERROR]: " << msg << _NH3D_ENDLOG
#define NH3D_ERROR_VK(msg) std::cerr << "\033[31m[VK ERROR]: " << msg << _NH3D_ENDLOG

#define _NH3D_FATAL(msg) std::cerr << "\033[31m[FATAL]: " << msg << _NH3D_ENDLOG
#define NH3D_ABORT(reason)   \
    _NH3D_FATAL(reason); \
    std::abort()

#define _NH3D_FATAL_VK(msg) std::cerr << "\033[31m[VK FATAL]: " << msg << _NH3D_ENDLOG
#define NH3D_ABORT_VK(reason)   \
    _NH3D_FATAL_VK(reason); \
    std::abort()

#define NH3D_NO_COPY(T)    \
    T(const T&) = delete; \
    T& operator=(const T&) = delete;
#define NH3D_NO_MOVE(T)     \
    T(const T&&) = delete; \
    T& operator=(const T&&) = delete;
#define NH3D_NO_COPY_MOVE(T) NH3D_NO_COPY(T) NH3D_NO_MOVE(T)

#define NH3D_STATIC_ASSERT static_assert

#ifdef NH3D_DEBUG
#define NH3D_DEBUGLOG(msg) std::cout << "\033[96m[DEBUG]: " << msg << _NH3D_ENDLOG
#define NH3D_DEBUGLOG_VK(msg) std::cout << "\033[96m[VK DEBUG]: " << msg << _NH3D_ENDLOG

#define NH3D_ASSERT(condition, msg) \
    if (!(condition)) {            \
        NH3D_ABORT(msg);                \
    }
#define NH3D_ASSERT_VK(condition, msg) \
    if (!(condition)) {               \
        NH3D_ABORT_VK(msg);                \
    }
#else
#define _NH3D_NOP \
    if (false) { \
    }
#define NH3D_DEBUG(msg) _NH3D_NOP
#define NH3D_DEBUG_VK(msg) _NH3D_NOP

#define NH3D_ASSERT(condition, msg) _NH3D_NOP
#define NH3D_ASSERT_VK(condition, msg) _NH3D_NOP
#endif

} 
