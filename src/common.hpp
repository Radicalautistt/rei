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

#ifndef REI_LOG_INFO
#  define REI_LOG_INFO(format, ...) \
     rei::logger (rei::LogLevelInfo, format, __VA_ARGS__)
#endif

#ifndef REI_LOG_ERROR
#  define REI_LOG_ERROR(format, ...) \
     rei::logger (rei::LogLevelError, format, __VA_ARGS__)
#endif

#ifndef REI_LOG_WARN
#  define REI_LOG_WARN(format, ...) \
     rei::logger (rei::LogLevelWarning, format, __VA_ARGS__)
#endif

#ifndef REI_LOGS_INFO
#  define REI_LOGS_INFO(string) REI_LOG_INFO ("%s", string)
#endif

#ifndef REI_LOGS_ERROR
#  define REI_LOGS_ERROR(string) REI_LOG_ERROR ("%s", string)
#endif

#ifndef REI_LOGS_WARN
#  define REI_LOGS_WARN(string) REI_LOG_WARN ("%s", string)
#endif

// General-purpose macros
#ifndef REI_SWAP
#  define REI_SWAP(a, b) do { \
     auto temp = *a;          \
     *a = *b;                 \
     *b = temp;               \
   } while (0)
#endif

#ifndef REI_MIN
#  define REI_MIN(a, b) (((a) > (b)) ? (b) : (a))
#endif

#ifndef REI_MAX
#  define REI_MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef REI_CLAMP
#  define REI_CLAMP(value, min, max) (REI_MAX (min, REI_MIN (value, max)))
#endif

#ifndef REI_ARRAY_SIZE
#  define REI_ARRAY_SIZE(array) (sizeof (array) / sizeof *array)
#endif

#ifndef REI_ALLOCA
#  define REI_ALLOCA(Type, count) (Type*) alloca (sizeof (Type) * count)
#endif

#ifndef REI_MALLOC
#  define REI_MALLOC(Type, count) (Type*) malloc (sizeof (Type) * count)
#endif

#ifndef REI_OFFSET_OF
#  define REI_OFFSET_OF(structure, member) ((size_t) &(((structure*) nullptr)->member))
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
         REI_LOG_ERROR (                                             \
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
         REI_LOG_ERROR (                                                                   \
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

#ifndef REI_COUNT_CYCLES
#  define REI_COUNT_CYCLES(routine) do {                                                        \
     u64 start = __rdtsc ();                                                                    \
     routine;                                                                                   \
     u64 end = __rdtsc () - start;                                                              \
     REI_LOG_INFO (ANSI_YELLOW "%s" ANSI_GREEN " took %llu cycles to comptute", #routine, end); \
   } while (0)
#endif

// Count of frames in flight
#ifndef REI_FRAMES_COUNT
#  define REI_FRAMES_COUNT 2u
#endif

namespace rei {

enum LogLevel : u8 {
  LogLevelInfo,
  LogLevelError,
  LogLevelWarning
};

enum class Result : u8 {
  Success,
  FileIsEmpty,
  FileDoesNotExist
};

struct Vertex {
  f32 x, y, z;
  f32 nx, ny, nz;
  f32 u, v;
};

struct File {
  size_t size;
  void* contents;
};

struct Timer {
  static timeval start;

  static void init () noexcept;
  [[nodiscard]] static f32 getCurrentTime () noexcept;
};

void logger (LogLevel level, const char* format, ...);

[[nodiscard]] const char* getError (Result result) noexcept;
Result readFile (const char* relativePath, b8 binary, File* output);
void writeFile (const char* relativePath, b8 binary, void* data, size_t size);

};

#endif /* COMMON_HPP */
