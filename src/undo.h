/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 *
 * Copyright (c) 1996-2000 Grace Development Team
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 */

/*
 * Undo/redo of changes to the project (see undo.c)
 */

#ifndef __UNDO_H_
#define __UNDO_H_

/* forget the history (a new project has been loaded) */
void undo_reset(void);

/* record the current state if the project changed since the last call;
   the GUI calls it when idle, so that each user action is one step */
void undo_checkpoint(void);

/* the project may have changed without set_dirtystate() being called
   (e.g. by a command); the next checkpoint compares the states */
void undo_touch(void);

/* go back/forward one step; return RETURN_SUCCESS if the project changed */
int undo(void);
int redo(void);

int undo_possible(void);
int redo_possible(void);

/* number of steps kept and memory limit (in bytes) of the history */
void undo_set_limits(int levels, long bytes);

/* TRUE while a state is being restored */
int undo_restoring(void);

#endif /* __UNDO_H_ */
