#ifndef COMMON_HPP
#define COMMON_HPP

#include <alloca.h>
#include <stdlib.h>
#include <x86intrin.h>

#include "rei_types.hpp"

// List of ANSI color to use with the logger
#ifndef ANSI_RESET
#  define ANSI_RESET "\033[;0m"
#endif

#ifndef ANSI_RED
#  define ANSI_RED "\033[1;31m"
#endif

#ifndef ANSI_BLUE
#  define ANSI_BLUE "\033[1;34m"
#endif

#ifndef ANSI_CYAN
#  define ANSI_CYAN "\033[1;36m"
#endif

#ifndef ANSI_BLACK
#  define ANSI_BLACK "\033[1;30m"
#endif

#ifndef ANSI_WHITE
#  define ANSI_WHITE "\033[1;37m"
#endif

#ifndef ANSI_GREEN
#  define ANSI_GREEN "\033[1;32m"
#endif

#ifndef ANSI_YELLOW
#  define ANSI_YELLOW "\033[1;33m"
#endif

#ifndef ANSI_PURPLE
#  define ANSI_PURPLE "\033[1;35m"
#endif

#ifndef LOG_INFO
#  define LOG_INFO(format, ...) \
     rei::logger (rei::LogLevelInfo, format, __VA_ARGS__)
#endif

#ifndef LOG_ERROR
#  define LOG_ERROR(format, ...) \
     rei::logger (rei::LogLevelError, format, __VA_ARGS__)
#endif

#ifndef LOG_WARNING
#  define LOG_WARNING(format, ...) \
     rei::logger (rei::LogLevelWarning, format, __VA_ARGS__)
#endif

#ifndef LOGS_INFO
#  define LOGS_INFO(string) LOG_INFO ("%s", string)
#endif

#ifndef LOGS_ERROR
#  define LOGS_ERROR(string) LOG_ERROR ("%s", string)
#endif

#ifndef LOGS_WARNING
#  define LOGS_WARNING(string) LOG_WARNING ("%s", string)
#endif

// General-purpose macros
#ifndef SWAP
#  define SWAP(a, b) do { \
     auto temp = *a;      \
     *a = *b;             \
     *b = temp;           \
   } while (0)
#endif

#ifndef MIN
#  define MIN(a, b) (((a) > (b)) ? (b) : (a))
#endif

#ifndef MAX
#  define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(array) sizeof (array) / sizeof *array
#endif

#ifndef ALLOCA
#  define ALLOCA(Type, count) (Type*) alloca (sizeof (Type) * count)
#endif

#ifndef MALLOC
#  define MALLOC(Type, count) (Type*) malloc (sizeof (Type) * count)
#endif

#ifndef CLAMP
#  define CLAMP(value, min, max) (((value) > (max)) ? (max) : (((value) < (min)) ? (min) : (value)))
#endif

#ifdef NDEBUG
#  ifndef REI_ASSERT
#    define REI_ASSERT(condition)
#  endif
#else
#  ifndef REI_ASSERT
#    define REI_ASSERT(condition)                                    \
       if (condition) {                                              \
         (void) 0;                                                   \
       } else {                                                      \
         LOG_ERROR (                                                 \
           "%s:%d Assertion " ANSI_YELLOW "[%s]" ANSI_RED " failed", \
           __FILE__,                                                 \
           __LINE__,                                                 \
           #condition                                                \
         );	                                                     \
                                                                     \
         __builtin_trap ();                                          \
       }
#  endif
#endif

#ifdef NDEBUG
#  ifndef REI_CHECK
#    define REI_CHECK(call) call
#  endif
#else
#  ifndef REI_CHECK
#    define REI_CHECK(call) do {                                                           \
       rei::Result result = call;                                                          \
       if (result != rei::Result::Success) {                                               \
         LOG_ERROR (                                                                       \
	   "%s:%d Rei error " ANSI_YELLOW "[%s]" ANSI_RED " occured in " ANSI_YELLOW "%s", \
	   __FILE__,                                                                       \
	   __LINE__,                                                                       \
	   rei::getError (result),                                                         \
	   __FUNCTION__                                                                    \
	 );                                                                                \
                                                                                           \
         abort ();                                                                         \
       }                                                                                   \
     } while (0)
#  endif
#endif

#ifndef COUNT_CYCLES
#  define COUNT_CYCLES(routine) do {                                                        \
     Uint64 start = __rdtsc ();                                                             \
     routine;                                                                               \
     Uint64 end = __rdtsc () - start;                                                       \
     LOG_INFO (ANSI_YELLOW "%s" ANSI_GREEN " took %llu cycles to comptute", #routine, end); \
   } while (0)
#endif

// Count of frames in flight
#ifndef FRAMES_COUNT
#  define FRAMES_COUNT 2u
#endif

namespace rei {

enum LogLevel : Uint8 {
  LogLevelInfo,
  LogLevelError,
  LogLevelWarning
};

enum class Result : Uint8 {
  Success,
  FileIsEmpty,
  FileDoesNotExist
};

struct Vertex {
  Float32 x, y, z;
  Float32 nx, ny, nz;
  Float32 u, v;
};

struct File {
  size_t size;
  void* contents;
};

struct Timer {
  static timeval start;

  static void init () noexcept;
  [[nodiscard]] static Float32 getCurrentTime () noexcept;
};

void logger (LogLevel level, const char* format, ...);

[[nodiscard]] const char* getError (Result result) noexcept;
Result readFile (const char* relativePath, Bool binary, File* output);

};

#endif /* COMMON_HPP */
