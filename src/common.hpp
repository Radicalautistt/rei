#ifndef COMMON_HPP
#define COMMON_HPP

#include <alloca.h>
#include <stdlib.h>

#define SCAST static_cast
#define RCAST reinterpret_cast

#define MIN(a, b) (((a) > (b)) ? (b) : (a))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define ARRAY_SIZE(array) sizeof (array) / sizeof *array

#define ALLOCA(Type, count) SCAST <Type*> (alloca (sizeof (Type) * count))
#define MALLOC(Type, count) SCAST <Type*> (malloc (sizeof (Type) * count))

#endif /* COMMON_HPP */
