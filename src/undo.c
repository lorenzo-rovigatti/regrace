/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 *
 * Copyright (c) 1996-2000 Grace Development Team
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 */

/*
 *
 * Undo/redo of changes to the project
 *
 * The history is a list of snapshots of the whole project. A snapshot
 * has two parts:
 *
 *  - the settings, as written by putparms(), i.e. the same text as the
 *    parameter part of a project file. It describes the complete state
 *    of the project except the data, and is restored the same way a
 *    project is loaded (by feeding it to the parser).
 *
 *  - the data of the sets, copied in memory with full precision. A
 *    column that did not change since the previous snapshot is shared
 *    with it, so steps that only change the appearance cost almost
 *    nothing even with large data sets.
 *
 * Snapshots are taken by undo_checkpoint() when the project was
 * modified (set_dirtystate() was called) since the previous one; the
 * GUI calls it when idle, so that each user action becomes one step.
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "globals.h"
#include "utils.h"
#include "files.h"
#include "graphs.h"
#include "graphutils.h"
#include "parser.h"
#include "protos.h"

#include "undo.h"

extern char print_file[];

/* a column of data, possibly shared by several snapshots */
typedef struct {
    int refcount;
    int len;
    double *v;
} DataBlock;

typedef struct {
    int gno;
    int setno;
    int type;
    int len;
    int ncols;
    DataBlock *cols[MAX_SET_COLS];
    char **s;
} SetSnap;

typedef struct {
    char *params;
    int nsets;
    SetSnap *sets;
    int cg;
    long bytes;         /* memory owned by this snapshot */
} Snapshot;

static Snapshot **history = NULL;
static int nhistory = 0;
static int current = -1;        /* the snapshot of the current state */
static int saved = -1;          /* the snapshot of the saved state */

static int max_levels = 100;
static long max_bytes = 256L*1024*1024;

static unsigned long last_count = 0;
static unsigned long touch_count = 0;
static unsigned long last_clean = 0;
static int restoring = FALSE;

/* ------------------------------------------------------------------ */

static void free_block(DataBlock *b)
{
    if (b && --b->refcount == 0) {
        xfree(b->v);
        xfree(b);
    }
}

static void free_snapshot(Snapshot *sn)
{
    int i, j;

    if (sn == NULL) {
        return;
    }
    for (i = 0; i < sn->nsets; i++) {
        SetSnap *ss = &sn->sets[i];
        for (j = 0; j < ss->ncols; j++) {
            free_block(ss->cols[j]);
        }
        if (ss->s) {
            for (j = 0; j < ss->len; j++) {
                xfree(ss->s[j]);
            }
            xfree(ss->s);
        }
    }
    xfree(sn->sets);
    xfree(sn->params);
    xfree(sn);
}

static SetSnap *find_set(Snapshot *sn, int gno, int setno)
{
    int i;
    if (sn == NULL) {
        return NULL;
    }
    for (i = 0; i < sn->nsets; i++) {
        if (sn->sets[i].gno == gno && sn->sets[i].setno == setno) {
            return &sn->sets[i];
        }
    }
    return NULL;
}

/* a column of the current data, shared with prev if equal */
static DataBlock *snap_column(double *x, int len, DataBlock *prev, long *bytes)
{
    DataBlock *b;

    if (prev && prev->len == len &&
        (len == 0 || memcmp(prev->v, x, len*SIZEOF_DOUBLE) == 0)) {
        prev->refcount++;
        return prev;
    }

    b = xmalloc(sizeof(DataBlock));
    if (b == NULL) {
        return NULL;
    }
    b->refcount = 1;
    b->len = len;
    b->v = xmalloc(MAX2(len, 1)*SIZEOF_DOUBLE);
    if (b->v == NULL) {
        xfree(b);
        return NULL;
    }
    memcpy(b->v, x, len*SIZEOF_DOUBLE);
    *bytes += len*SIZEOF_DOUBLE;

    return b;
}

static Snapshot *take_snapshot(Snapshot *prev)
{
    Snapshot *sn;
    FILE *fp;
    char *buf = NULL;
    size_t size = 0;
    int gno, setno, n, col;

    sn = xcalloc(1, sizeof(Snapshot));
    if (sn == NULL) {
        return NULL;
    }

    /* the settings */
    fp = open_memstream(&buf, &size);
    if (fp == NULL) {
        xfree(sn);
        return NULL;
    }
    putparms(-1, fp, FALSE);
    fclose(fp);
    sn->params = copy_string(NULL, buf);
    free(buf);
    sn->bytes = size;

    sn->cg = get_cg();

    /* the data */
    n = 0;
    for (gno = 0; gno < number_of_graphs(); gno++) {
        for (setno = 0; setno < number_of_sets(gno); setno++) {
            if (is_set_active(gno, setno)) {
                n++;
            }
        }
    }
    sn->sets = xcalloc(MAX2(n, 1), sizeof(SetSnap));
    for (gno = 0; gno < number_of_graphs(); gno++) {
        for (setno = 0; setno < number_of_sets(gno); setno++) {
            SetSnap *ss, *pss;
            char **s;

            if (!is_set_active(gno, setno)) {
                continue;
            }
            ss = &sn->sets[sn->nsets++];
            ss->gno = gno;
            ss->setno = setno;
            ss->type = dataset_type(gno, setno);
            ss->len = getsetlength(gno, setno);
            ss->ncols = dataset_cols(gno, setno);
            pss = find_set(prev, gno, setno);
            for (col = 0; col < ss->ncols; col++) {
                ss->cols[col] = snap_column(getcol(gno, setno, col), ss->len,
                    (pss && col < pss->ncols) ? pss->cols[col] : NULL,
                    &sn->bytes);
                if (ss->cols[col] == NULL) {
                    ss->ncols = col;
                    free_snapshot(sn);
                    errmsg("Not enough memory for the undo history");
                    return NULL;
                }
            }
            if ((s = get_set_strings(gno, setno)) != NULL) {
                int i;
                ss->s = xmalloc(MAX2(ss->len, 1)*sizeof(char *));
                for (i = 0; i < ss->len; i++) {
                    ss->s[i] = copy_string(NULL, s[i]);
                    sn->bytes += s[i] ? strlen(s[i]) + 1 : 0;
                }
            }
        }
    }

    return sn;
}

/* the date of the time stamp changes with any modification: ignore it */
static int skip_line(const char *s)
{
    return strncmp(s, "timestamp def ", 14) == 0;
}

static int same_params(const char *a, const char *b)
{
    while (*a || *b) {
        const char *ea = strchr(a, '\n'), *eb = strchr(b, '\n');
        size_t la = ea ? (size_t) (ea - a) : strlen(a);
        size_t lb = eb ? (size_t) (eb - b) : strlen(b);
        if (!(skip_line(a) && skip_line(b)) &&
            (la != lb || strncmp(a, b, la) != 0)) {
            return FALSE;
        }
        a += la + (ea ? 1:0);
        b += lb + (eb ? 1:0);
    }
    return TRUE;
}

static int same_snapshot(Snapshot *a, Snapshot *b)
{
    int i, j;
    
    if (a == NULL || b == NULL || a->nsets != b->nsets ||
        !same_params(a->params, b->params)) {
        return FALSE;
    }
    for (i = 0; i < a->nsets; i++) {
        SetSnap *sa = &a->sets[i], *sb = &b->sets[i];
        if (sa->gno != sb->gno || sa->setno != sb->setno ||
            sa->type != sb->type || sa->len != sb->len ||
            sa->ncols != sb->ncols) {
            return FALSE;
        }
        for (j = 0; j < sa->ncols; j++) {
            /* equal columns are shared */
            if (sa->cols[j] != sb->cols[j]) {
                return FALSE;
            }
        }
        if ((sa->s == NULL) != (sb->s == NULL)) {
            return FALSE;
        }
        if (sa->s) {
            for (j = 0; j < sa->len; j++) {
                if (compare_strings(sa->s[j], sb->s[j]) == FALSE) {
                    return FALSE;
                }
            }
        }
    }

    return TRUE;
}

/* ------------------------------------------------------------------ */

static void drop_redo(void)
{
    while (nhistory > current + 1) {
        nhistory--;
        free_snapshot(history[nhistory]);
        if (saved == nhistory) {
            saved = -1;
        }
    }
}

static void trim_history(void)
{
    long bytes;
    int i;

    while (nhistory > 2) {
        bytes = 0;
        for (i = 0; i < nhistory; i++) {
            bytes += history[i]->bytes;
        }
        if (nhistory <= max_levels && bytes <= max_bytes) {
            break;
        }
        /* drop the oldest step */
        free_snapshot(history[0]);
        memmove(history, history + 1, (nhistory - 1)*sizeof(Snapshot *));
        nhistory--;
        current--;
        if (saved >= 0) {
            saved--;
        }
    }
}

static void push_snapshot(Snapshot *sn)
{
    drop_redo();
    history = xrealloc(history, (nhistory + 1)*sizeof(Snapshot *));
    history[nhistory++] = sn;
    current = nhistory - 1;
    trim_history();
}

void undo_reset(void)
{
    int i;

    if (restoring) {
        return;
    }
    for (i = 0; i < nhistory; i++) {
        free_snapshot(history[i]);
    }
    xfree(history);
    history = NULL;
    nhistory = 0;
    current = -1;
    saved = -1;
}

void undo_set_limits(int levels, long bytes)
{
    max_levels = MAX2(levels, 1);
    max_bytes = bytes;
    trim_history();
}

void undo_touch(void)
{
    touch_count++;
}

int undo_restoring(void)
{
    return restoring;
}

void undo_checkpoint(void)
{
    Snapshot *sn;
    unsigned long count;

    if (restoring) {
        return;
    }

    count = get_modification_count() + touch_count;
    
    if (nhistory == 0) {
        /* the initial state */
        sn = take_snapshot(NULL);
        if (sn) {
            push_snapshot(sn);
            saved = is_dirtystate() ? -1 : current;
        }
        last_count = count;
        last_clean = get_clean_count();
        return;
    }

    if (count != last_count) {
        last_count = count;
        sn = take_snapshot(history[current]);
        if (sn == NULL) {
            return;
        }
        if (same_snapshot(sn, history[current])) {
            free_snapshot(sn);
        } else {
            push_snapshot(sn);
            /* e.g. commands change the project without marking it */
            if (!is_dirtystate()) {
                set_dirtystate();
                last_count = get_modification_count() + touch_count;
            }
        }
    }
    
    /* the project was saved (in its current state) */
    if (get_clean_count() != last_clean) {
        last_clean = get_clean_count();
        saved = current;
    }
}

int undo_possible(void)
{
    return (current > 0) ? TRUE:FALSE;
}

int redo_possible(void)
{
    return (current >= 0 && current < nhistory - 1) ? TRUE:FALSE;
}

/* put the project in the state of a snapshot */
static int restore_snapshot(Snapshot *sn)
{
    char *docname, *pfile, *params, *line, *next;
    int i, j, save_noask, save_version;

    restoring = TRUE;

    docname = copy_string(NULL, get_docname());
    pfile = copy_string(NULL, print_file);
    save_noask = noask;

    /* as when loading a project: start from scratch... */
    noask = TRUE;
    wipeout();
    noask = save_noask;
    lock_dirtystate(TRUE);

    save_version = get_project_version();
    set_project_version(0);

    /* ... set everything but the data... */
    params = copy_string(NULL, sn->params);
    for (line = params; line && *line; line = next) {
        next = strchr(line, '\n');
        if (next) {
            *next++ = '\0';
        }
        if (*line != '\0' && *line != '#') {
            scanner(line);
        }
    }
    xfree(params);

    /* ... and the data */
    for (i = 0; i < sn->nsets; i++) {
        SetSnap *ss = &sn->sets[i];
        if (activateset(ss->gno, ss->setno) != RETURN_SUCCESS) {
            continue;
        }
        set_dataset_type(ss->gno, ss->setno, ss->type);
        setlength(ss->gno, ss->setno, ss->len);
        for (j = 0; j < ss->ncols; j++) {
            memcpy(getcol(ss->gno, ss->setno, j), ss->cols[j]->v,
                ss->len*SIZEOF_DOUBLE);
        }
        if (ss->s) {
            char **s = xmalloc(MAX2(ss->len, 1)*sizeof(char *));
            for (j = 0; j < ss->len; j++) {
                s[j] = copy_string(NULL, ss->s[j]);
            }
            set_set_strings(ss->gno, ss->setno, ss->len, s);
        }
    }

    postprocess_project(get_project_version());
    set_project_version(save_version);

    if (is_valid_gno(sn->cg)) {
        select_graph(sn->cg);
    }

    set_docname(docname);
    strcpy(print_file, pfile);
    xfree(docname);
    xfree(pfile);

    lock_dirtystate(FALSE);
    restoring = FALSE;

    return RETURN_SUCCESS;
}

static void after_restore(void)
{
    Snapshot *sn;
    
    if (current == saved) {
        clear_dirtystate();
    } else {
        set_dirtystate();
    }
    
    /*
     * Restoring is not exactly idempotent (e.g. fonts not used by the
     * project may get mapped), so the restored state, as it serializes
     * now, becomes the reference for this step; otherwise the next
     * checkpoint could see a change and discard the redo history.
     */
    sn = take_snapshot(history[current]);
    if (sn) {
        free_snapshot(history[current]);
        history[current] = sn;
    }
    
    /* the restore itself is not a new step */
    last_count = get_modification_count() + touch_count;
    last_clean = get_clean_count();
}

int undo(void)
{
    undo_checkpoint();
    if (!undo_possible()) {
        return RETURN_FAILURE;
    }
    current--;
    restore_snapshot(history[current]);
    after_restore();
    return RETURN_SUCCESS;
}

int redo(void)
{
    undo_checkpoint();
    if (!redo_possible()) {
        return RETURN_FAILURE;
    }
    current++;
    restore_snapshot(history[current]);
    after_restore();
    return RETURN_SUCCESS;
}
