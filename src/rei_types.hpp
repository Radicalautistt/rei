#ifndef REI_TYPES_HPP
#define REI_TYPES_HPP

typedef unsigned char Uint8;
typedef unsigned short int Uint16;
typedef unsigned int Uint32;
typedef unsigned long int Uint64;

typedef signed char Int8;
typedef signed short int Int16;
typedef signed int Int32;
typedef signed long int Int64;

typedef float Float32;
typedef double Float64;

typedef Uint8 Bool;
typedef Uint32 Bool32;

#ifndef True
#  define True 1u
#endif

#ifndef False
#  define False 0u
#endif

#endif /* REI_TYPES_HPP */
