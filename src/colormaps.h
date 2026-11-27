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
 * Palettes, colour maps and colouring by value (see colormaps.c)
 */

#ifndef __COLORMAPS_H_
#define __COLORMAPS_H_

/* palettes: sets of named colours for the main colours 2, 3, ... */
int number_of_palettes(void);
char *palette_name(int i);
int apply_palette(int i);

/* colour maps: continuous scales */
int number_of_colormaps(void);
char *colormap_name(int i);

/* define (or find) the n colours sampling a colour map; their indices
   in the colour table are returned in indices[] */
int colormap_colors(int map, int n, int reverse, int *indices);

/* the range of the finite (positive, if logscale) values of a column */
int get_column_range(int gno, int setno, int col, int logscale,
    double *vmin, double *vmax);

typedef struct {
    int col;            /* column with the values */
    int map;            /* colour map */
    int ncolors;        /* number of colours */
    int reverse;        /* reversed colour map */
    double vmin, vmax;  /* range of values mapped to the colour map */
    int logscale;       /* logarithmic mapping */
    int new_set;        /* put the result in a new set, else convert the set */
    int hide_source;    /* hide the source set (with new_set) */
    int colorbar;       /* add a colour bar */
} ColorByValue;

/* colour the symbols of a set according to the values of a column;
   returns the set with the result (or -1) */
int color_by_value(int gno, int setno, ColorByValue *cbv);

#endif /* __COLORMAPS_H_ */
