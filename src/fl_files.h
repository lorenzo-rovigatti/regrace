/*
 * files.h uses "template" as a parameter name, which is a reserved
 * word in C++. Include it through this wrapper from C++ sources.
 */
#ifndef __FL_FILES_H_
#define __FL_FILES_H_

#ifdef __cplusplus
extern "C" {
#  define template template_
#  include "files.h"
#  undef template
}
#else
#  include "files.h"
#endif

#endif /* __FL_FILES_H_ */
