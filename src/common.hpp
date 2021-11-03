#ifndef COMMON_HPP
#define COMMON_HPP

#include <alloca.h>
#include <stdlib.h>
#include <stdint.h>
#include <x86intrin.h>

#define SCAST static_cast
#define RCAST reinterpret_cast

#define MIN(a, b) (((a) > (b)) ? (b) : (a))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define ARRAY_SIZE(array) sizeof (array) / sizeof *array

#define ALLOCA(Type, count) SCAST <Type*> (alloca (sizeof (Type) * count))
#define MALLOC(Type, count) SCAST <Type*> (malloc (sizeof (Type) * count))

#ifdef NDEBUG
#  ifndef REI_CHECK
#    define REI_CHECK(call) call
#  endif
#else
#  ifndef REI_CHECK
#    define REI_CHECK(call) do {            \
       rei::Result error = call;            \
       if (error != rei::Result::Success) { \
         fprintf (                          \
	   stderr,                          \
	   "%s:%d (%s) %s\n",               \
	   __FILE__,                        \
	   __LINE__,                        \
	   __FUNCTION__,                    \
	   rei::getError (error)            \
	 );                                 \
                                            \
         abort ();                          \
       }                                    \
     } while (false)
#  endif
#endif

#ifndef COUNT_CYCLES
#  define COUNT_CYCLES(routine) do {                                             \
     uint64_t start = __rdtsc ();                                                \
     routine;                                                                    \
     printf ("%s took %llu cycles to comptute\n", #routine, __rdtsc () - start); \
   } while (0)
#endif

namespace rei {

enum class Result : uint8_t {
  Success,
  FileDoesNotExist
};

[[nodiscard]] const char* getError (Result result) noexcept;

};

#endif /* COMMON_HPP */
