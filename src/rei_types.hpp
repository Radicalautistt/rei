#ifndef REI_TYPES_HPP
#define REI_TYPES_HPP

typedef unsigned char u8;
typedef unsigned short int u16;
typedef unsigned int u32;
typedef unsigned long int u64;

typedef signed char i8;
typedef signed short int i16;
typedef signed int i32;
typedef signed long int i64;

typedef float f32;
typedef double f64;

typedef u8 b8;
typedef u32 b32;

#ifndef REI_TRUE
#  define REI_TRUE 1u
#endif

#ifndef REI_FALSE
#  define REI_FALSE 0u
#endif

#endif /* REI_TYPES_HPP */
