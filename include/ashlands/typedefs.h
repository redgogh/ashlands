#ifndef _TYPEDEFS_H_
#define _TYPEDEFS_H_

#ifdef __cplusplus
#include <iterator> // std::size()
#endif

#ifdef __cplusplus
#define ARRAY_SIZE(a) std::size(a)
#else
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#endif

#endif /* _TYPEDEFS_H_ */