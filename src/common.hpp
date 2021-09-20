#ifndef COMMON_HPP
#define COMMON_HPP

#include <alloca.h>
#include <stdlib.h>

#define SCAST static_cast
#define RCAST reinterpret_cast

#define ALLOCA(Type, count) SCAST <Type*> (alloca (sizeof (Type) * count))
#define MALLOC(Type, count) SCAST <Type*> (malloc (sizeof (Type) * count))

#endif /* COMMON_HPP */
