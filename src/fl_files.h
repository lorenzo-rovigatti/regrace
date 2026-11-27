/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 *
 * Written in 2026 for regrace; see README.md.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 */

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
