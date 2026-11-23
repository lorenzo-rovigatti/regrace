/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 * 
 * Home page: http://plasma-gate.weizmann.ac.il/Grace/
 * 
 * Copyright (c) 1991-1995 Paul J Turner, Portland, OR
 * Copyright (c) 1996-2000 Grace Development Team
 * 
 * Maintained by Evgeny Stambulchik
 * 
 * 
 *                           All Rights Reserved
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
 * 
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 *
 * GUI utilities that do not depend on the toolkit: they are written
 * on top of the API of motifinc.h only, and shared by the Motif and
 * the FLTK interfaces.
 *
 */

#include <config.h>
#include <cmath.h>

#include <stdlib.h>
#include <string.h>

#include "guiutils.h"

#include "defines.h"
#include "globals.h"
#include "draw.h"
#include "patterns.h"
#include "jbitmaps.h"
#include "t1fonts.h"
#include "graphs.h"
#include "utils.h"
#include "events.h"
#include "parser.h"
#include "protos.h"

OptionItem *color_option_items = NULL;
int ncolor_option_items = 0;
OptionStructure **color_selectors = NULL;
int ncolor_selectors = 0;

OptionItem *graph_select_items = NULL;
int ngraph_select_items = 0;
ListStructure **graph_selectors = NULL;
int ngraph_selectors = 0;

ListStructure **set_selectors = NULL;
int nset_selectors = 0;

SetChoiceItem *plist = NULL;
int nplist = 0;

static OptionItem *font_option_items;
static OptionItem *settype_option_items;
static BitmapOptionItem *pattern_option_items;
static BitmapOptionItem *lines_option_items;

#define LINES_BM_HEIGHT 15
#define LINES_BM_WIDTH  64

static BitmapOptionItem just_option_items[12] =
{
    {JUST_LEFT  |JUST_BLINE , j_lm_o_bits},
    {JUST_CENTER|JUST_BLINE , j_cm_o_bits},
    {JUST_RIGHT |JUST_BLINE , j_rm_o_bits},
    {JUST_LEFT  |JUST_BOTTOM, j_lb_b_bits},
    {JUST_CENTER|JUST_BOTTOM, j_cb_b_bits},
    {JUST_RIGHT |JUST_BOTTOM, j_rb_b_bits},
    {JUST_LEFT  |JUST_MIDDLE, j_lm_b_bits},
    {JUST_CENTER|JUST_MIDDLE, j_cm_b_bits},
    {JUST_RIGHT |JUST_MIDDLE, j_rm_b_bits},
    {JUST_LEFT  |JUST_TOP   , j_lt_b_bits},
    {JUST_CENTER|JUST_TOP   , j_ct_b_bits},
    {JUST_RIGHT |JUST_TOP   , j_rt_b_bits}
};

static OptionItem fmt_option_items[32] =
{
    {FORMAT_DECIMAL,        "Decimal"             },
    {FORMAT_EXPONENTIAL,    "Exponential"         },
    {FORMAT_GENERAL,        "General"             },
    {FORMAT_POWER,          "Power"               },
    {FORMAT_SCIENTIFIC,     "Scientific"          },
    {FORMAT_ENGINEERING,    "Engineering"         },
    {FORMAT_COMPUTING,      "Computing (K,M,G,...)"},
    {FORMAT_DDMMYY,         "DD-MM-YY"            },
    {FORMAT_MMDDYY,         "MM-DD-YY"            },
    {FORMAT_YYMMDD,         "YY-MM-DD"            },
    {FORMAT_MMYY,           "MM-YY"               },
    {FORMAT_MMDD,           "MM-DD"               },
    {FORMAT_MONTHDAY,       "Month-DD"            },
    {FORMAT_DAYMONTH,       "DD-Month"            },
    {FORMAT_MONTHS,         "Month (abrev.)"      },
    {FORMAT_MONTHSY,        "Month (abrev.)-YY"   },
    {FORMAT_MONTHL,         "Month"               },
    {FORMAT_DAYOFWEEKS,     "Day of week (abrev.)"},
    {FORMAT_DAYOFWEEKL,     "Day of week"         },
    {FORMAT_DAYOFYEAR,      "Day of year"         },
    {FORMAT_HMS,            "HH:MM:SS"            },
    {FORMAT_MMDDHMS,        "MM-DD HH:MM:SS"      },
    {FORMAT_MMDDYYHMS,      "MM-DD-YY HH:MM:SS"   },
    {FORMAT_YYMMDDHMS,      "YY-MM-DD HH:MM:SS"   },
    {FORMAT_DEGREESLON,     "Degrees (lon)"       },
    {FORMAT_DEGREESMMLON,   "DD MM' (lon)"        },
    {FORMAT_DEGREESMMSSLON, "DD MM' SS.s\" (lon)" },
    {FORMAT_MMSSLON,        "MM' SS.s\" (lon)"    },
    {FORMAT_DEGREESLAT,     "Degrees (lat)"       },
    {FORMAT_DEGREESMMLAT,   "DD MM' (lat)"        },
    {FORMAT_DEGREESMMSSLAT, "DD MM' SS.s\" (lat)" },
    {FORMAT_MMSSLAT,        "MM' SS.s\" (lat)"    }
};

static OptionItem as_option_items[4] = 
{
    {AUTOSCALE_NONE, "None"},
    {AUTOSCALE_X,    "X"},
    {AUTOSCALE_Y,    "Y"},
    {AUTOSCALE_XY,   "XY"}
};

extern int ReqUpdateColorSel;

int GetSingleListChoice(ListStructure *listp, int *value)
{
    int n, *values, retval;
 
    n = GetListChoices(listp, &values);
    if (n == 1) {
        *value = values[0];
        retval = RETURN_SUCCESS;
    } else {
        retval = RETURN_FAILURE;
    }
    if (n > 0) {
        xfree(values);
    }
    return retval;
}

double GetSpinChoice(SpinStructure *spinp)
{
    double retval;
    
    xv_evalexpr(spinp->text, &retval);
    if (retval < spinp->min) {
        errmsg("Input value below min limit in GetSpinChoice()");
        retval = spinp->min;
        SetSpinChoice(spinp, retval);
    } else if (retval > spinp->max) {
        errmsg("Input value above max limit in GetSpinChoice()");
        retval = spinp->max;
        SetSpinChoice(spinp, retval);
    }
    
    if (spinp->type == SPIN_TYPE_INT) {
        return rint(retval);
    } else {
        return retval;
    }
}

int init_option_menus(void) {
    int i, j, k, l, n;
    
    n = number_of_fonts();
    font_option_items = xmalloc(n*sizeof(OptionItem));
    if (font_option_items == NULL) {
        errmsg("Malloc error in init_option_menus()");
        return RETURN_FAILURE;
    }
    for (i = 0; i < n; i++) {
        font_option_items[i].value = i;
        font_option_items[i].label = get_fontalias(i);
    }
    
    n = number_of_patterns();
    pattern_option_items = xmalloc(n*sizeof(BitmapOptionItem));
    if (pattern_option_items == NULL) {
        errmsg("Malloc error in init_option_menus()");
        xfree(font_option_items);
        return RETURN_FAILURE;
    }
    for (i = 0; i < n; i++) {
        pattern_option_items[i].value = i;
        if (i == 0) {
            pattern_option_items[i].bitmap = NULL;
        } else {
            pattern_option_items[i].bitmap = pat_bits[i];
        }
    }
    
    n = number_of_linestyles();
    lines_option_items = xmalloc(n*sizeof(BitmapOptionItem));
    if (lines_option_items == NULL) {
        errmsg("Malloc error in init_option_menus()");
        xfree(pattern_option_items);
        xfree(font_option_items);
        return RETURN_FAILURE;
    }
    for (i = 0; i < n; i++) {
        lines_option_items[i].value = i;
        if (i == 0) {
            lines_option_items[i].bitmap = NULL;
            continue;
        }
        
        lines_option_items[i].bitmap = 
              xcalloc(LINES_BM_HEIGHT*LINES_BM_WIDTH/8/SIZEOF_CHAR, SIZEOF_CHAR);
        
        k = LINES_BM_WIDTH*(LINES_BM_HEIGHT/2);
        while (k < LINES_BM_WIDTH*(LINES_BM_HEIGHT/2 + 1)) {
            for (j = 0; j < dash_array_length[i]; j++) {
                for (l = 0; l < dash_array[i][j]; l++) {
                    if (k < LINES_BM_WIDTH*(LINES_BM_HEIGHT/2 + 1)) {
                        if (j % 2 == 0) { 
                            /* black */
                            lines_option_items[i].bitmap[k/8] |= 1 << k % 8;
                        }
                        k++;
                    }
                }
            }
        }
    }

    settype_option_items = xmalloc(NUMBER_OF_SETTYPES*sizeof(OptionItem));
    if (settype_option_items == NULL) {
        errmsg("Malloc error in init_option_menus()");
        return RETURN_FAILURE;
    }
    for (i = 0; i < NUMBER_OF_SETTYPES; i++) {
        settype_option_items[i].value = i;
        settype_option_items[i].label = copy_string(NULL, set_types(i));
        lowtoupper(settype_option_items[i].label);
    }

    return RETURN_SUCCESS;
}

OptionStructure *CreateFontChoice(Widget parent, char *s)
{
    return (CreateOptionChoice(parent,
        s, 0, number_of_fonts(), font_option_items));
}

OptionStructure *CreatePatternChoice(Widget parent, char *s)
{
    return (CreateBitmapOptionChoice(parent, s, 4, number_of_patterns(), 
                                     16, 16, pattern_option_items));
}

OptionStructure *CreateLineStyleChoice(Widget parent, char *s)
{
    return (CreateBitmapOptionChoice(parent, s, 0, number_of_linestyles(), 
                        LINES_BM_WIDTH, LINES_BM_HEIGHT, lines_option_items));
}

OptionStructure *CreateSetTypeChoice(Widget parent, char *s)
{
    return (CreateOptionChoice(parent,
        s, 0, NUMBER_OF_SETTYPES, settype_option_items));
}

OptionStructure *CreateJustChoice(Widget parent, char *s)
{
    return (CreateBitmapOptionChoice(parent, s, 4,
        12, JBITMAP_WIDTH, JBITMAP_HEIGHT, just_option_items));
}

void update_graph_selectors(void)
{
    int i, new_n = number_of_graphs();
    char buf[64];
    OptionItem *p;
    
    for (i = 0; i < ngraph_select_items; i++) {
        xfree(graph_select_items[i].label);
    }
    p = xrealloc(graph_select_items, new_n*sizeof(OptionItem));
    if (p == NULL && new_n != 0) {
        ngraph_select_items = 0;
        return;
    } else {
        graph_select_items = p;
    }

    for (i = 0; i < new_n; i++) {
        graph_select_items[i].value = i;
        sprintf(buf, "(%c) G%d (%d sets)",
            is_graph_hidden(i) ? '-':'+', i, number_of_sets(i));
        graph_select_items[i].label = copy_string(NULL, buf);
    }
    ngraph_select_items = new_n;
    
    for (i = 0; i < ngraph_selectors; i++) {
        UpdateListChoice(graph_selectors[i],
            ngraph_select_items, graph_select_items);
    }
}

void graph_menu_cb(ListStructure *listp, GraphMenuCBtype type)
{
    int err = FALSE;
    int i, n, *values;
    char buf[32];

    n = GetListChoices(listp, &values);
    
    switch (type) {
    case GraphMenuFocusCB:
        if (n == 1) {
            switch_current_graph(values[0]);
        } else {
            err = TRUE;
        }
        break;
    case GraphMenuHideCB:
        if (n > 0) {
            for (i = 0; i < n; i++) {
                set_graph_hidden(values[i], TRUE);
            }
        } else {
            err = TRUE;
        }
        break;
    case GraphMenuShowCB:
        if (n > 0) {
            for (i = 0; i < n; i++) {
                set_graph_hidden(values[i], FALSE);
            }
        } else {
            err = TRUE;
        }
        break;
    case GraphMenuDuplicateCB:
        if (n > 0) {
            for (i = 0; i < n; i++) {
                duplicate_graph(values[i]);
            }
        } else {
            err = TRUE;
        }
        break;
    case GraphMenuKillCB:
        if (n > 0) {
            if (yesno("Kill selected graph(s)?", NULL, NULL, NULL)) {
                for (i = n - 1; i >= 0; i--) {
                    kill_graph(values[i]);
                }
            }
        } else {
            err = TRUE;
        }
        break;
    case GraphMenuCopy12CB:
        if (n == 2) {
            sprintf(buf, "Overwrite G%d?", values[1]);
            if (yesno(buf, NULL, NULL, NULL)) {
                copy_graph(values[0], values[1]);
            }
        } else {
            err = TRUE;
        }
        break;
    case GraphMenuCopy21CB:
        if (n == 2) {
            sprintf(buf, "Overwrite G%d?", values[0]);
            if (yesno(buf, NULL, NULL, NULL)) {
                copy_graph(values[1], values[0]);
            }
        } else {
            err = TRUE;
        }
        break;
    case GraphMenuMove12CB:
        if (n == 2) {
            sprintf(buf, "Replace G%d?", values[1]);
            if (yesno(buf, NULL, NULL, NULL)) {
                move_graph(values[0], values[1]);
            }
        } else {
            err = TRUE;
        }
        break;
    case GraphMenuMove21CB:
        if (n == 2) {
            sprintf(buf, "Replace G%d?", values[0]);
            if (yesno(buf, NULL, NULL, NULL)) {
                move_graph(values[1], values[0]);
            }
        } else {
            err = TRUE;
        }
        break;
    case GraphMenuSwapCB:
        if (n == 2) {
            swap_graph(values[0], values[1]);
        } else {
            err = TRUE;
        }
        break;
    case GraphMenuNewCB:
        set_graph_active(number_of_graphs());
        break;
    default:
        err = TRUE;
        break;
    }

    if (n > 0) {
        xfree(values);
    }

    if (err == FALSE) {
        update_all();
        xdrawgraph();
    }
}

void switch_focus_proc(void *data)
{
    graph_menu_cb((ListStructure *) data, GraphMenuFocusCB);
}

void hide_graph_proc(void *data)
{
    graph_menu_cb((ListStructure *) data, GraphMenuHideCB);
}

void show_graph_proc(void *data)
{
    graph_menu_cb((ListStructure *) data, GraphMenuShowCB);
}

void duplicate_graph_proc(void *data)
{
    graph_menu_cb((ListStructure *) data, GraphMenuDuplicateCB);
}

void kill_graph_proc(void *data)
{
    graph_menu_cb((ListStructure *) data, GraphMenuKillCB);
}

void copy12_graph_proc(void *data)
{
    graph_menu_cb((ListStructure *) data, GraphMenuCopy12CB);
}

void copy21_graph_proc(void *data)
{
    graph_menu_cb((ListStructure *) data, GraphMenuCopy21CB);
}

void move12_graph_proc(void *data)
{
    graph_menu_cb((ListStructure *) data, GraphMenuMove12CB);
}

void move21_graph_proc(void *data)
{
    graph_menu_cb((ListStructure *) data, GraphMenuMove21CB);
}

void swap_graph_proc(void *data)
{
    graph_menu_cb((ListStructure *) data, GraphMenuSwapCB);
}

void create_new_graph_proc(void *data)
{
    graph_menu_cb((ListStructure *) data, GraphMenuNewCB);
}

void set_graph_selectors(int gno)
{
    int i;
    
    for (i = 0; i < ngraph_selectors; i++) {
        SelectListChoice(graph_selectors[i], gno);
    }
}

void UpdateSetChoice(ListStructure *listp, int gno)
{
    int i, j, n = number_of_sets(gno);
    char buf[64];
    OptionItem *set_select_items;
    SetChoiceData *sdata;
    
    sdata = (SetChoiceData *) listp->anydata;
    sdata->gno = gno;
    
    if (n <= 0) {
        UpdateListChoice(listp, 0, NULL);
        return;
    }
    
    set_select_items = xmalloc(n*sizeof(OptionItem));
    if (set_select_items == NULL) {
        return;
    }
    
    for (i = 0, j = 0; i < n; i++) {
        if ((sdata->show_nodata == TRUE || is_set_active(gno, i) == TRUE) &&
            (sdata->show_hidden == TRUE || is_set_hidden(gno, i) != TRUE )) {
            set_select_items[j].value = i;
            sprintf(buf, "(%c) G%d.S%d[%d][%d]",
                is_set_hidden(gno, i) ? '-':'+',
                gno, i, dataset_cols(gno, i), getsetlength(gno, i));
            set_select_items[j].label = copy_string(NULL, buf);
            if (sdata->view_comments == TRUE) {
                set_select_items[j].label =
                    concat_strings(set_select_items[j].label, " \"");
                set_select_items[j].label =
                    concat_strings(set_select_items[j].label,
                    getcomment(gno, i));
                set_select_items[j].label =
                    concat_strings(set_select_items[j].label, "\"");
            }
            j++;
        }
    }
    UpdateListChoice(listp, j, set_select_items);
    
    xfree(set_select_items);
}

void update_set_selectors(int gno)
{
    int i, cg;
    SetChoiceData *sdata;
    
    cg = get_cg();
    update_graph_selectors();
    for (i = 0; i < nset_selectors; i++) {
        sdata = (SetChoiceData *) set_selectors[i]->anydata;
        if (sdata->standalone == TRUE && (gno == cg || gno == ALL_GRAPHS)) {
            UpdateSetChoice(set_selectors[i], cg);
        } else if (sdata->standalone == FALSE && sdata->gno == gno) {
            UpdateSetChoice(set_selectors[i], gno);
        }
    }
}

void set_menu_cb(ListStructure *listp, SetMenuCBtype type)
{
    SetChoiceData *sdata;
    int err = FALSE;
    int gno;
    int i, n, setno, *values;
    char buf[32];

    n = GetListChoices(listp, &values);
    sdata = (SetChoiceData *) listp->anydata;
    gno = sdata->gno;
    
    switch (type) {
    case SetMenuHideCB:
        if (n > 0) {
            for (i = 0; i < n; i++) {
                set_set_hidden(gno, values[i], TRUE);
            }
        } else {
            err = TRUE;
        }
        break;
    case SetMenuShowCB:
        if (n > 0) {
            for (i = 0; i < n; i++) {
                set_set_hidden(gno, values[i], FALSE);
            }
        } else {
            err = TRUE;
        }
        break;
    case SetMenuBringfCB:
        if (n == 1) {
            pushset(gno, values[0], PUSH_SET_TOFRONT);
        } else {
            err = TRUE;
        }
        break;
    case SetMenuSendbCB:
        if (n == 1) {
            pushset(gno, values[0], PUSH_SET_TOBACK);
        } else {
            err = TRUE;
        }
        break;
    case SetMenuDuplicateCB:
        if (n > 0) {
            for (i = 0; i < n; i++) {
                setno = nextset(gno);
                do_copyset(gno, values[i], gno, setno);
            }
        } else {
            err = TRUE;
        }
        break;
    case SetMenuKillCB:
        if (n > 0) {
            if (yesno("Kill selected set(s)?", NULL, NULL, NULL)) {
                for (i = 0; i < n; i++) {
                    killset(gno, values[i]);
                }
            }
        } else {
            err = TRUE;
        }
        break;
    case SetMenuKillDCB:
        if (n > 0) {
            if (yesno("Kill data in selected set(s)?", NULL, NULL, NULL)) {
                for (i = 0; i < n; i++) {
                    killsetdata(gno, values[i]);
                    setcomment(gno, values[i], "");
                }
            }
        } else {
            err = TRUE;
        }
        break;
    case SetMenuCopy12CB:
        if (n == 2) {
            sprintf(buf, "Overwrite S%d?", values[1]);
            if (yesno(buf, NULL, NULL, NULL)) {
                do_copyset(gno, values[0], gno, values[1]);
            }
        } else {
            err = TRUE;
        }
        break;
    case SetMenuCopy21CB:
        if (n == 2) {
            sprintf(buf, "Overwrite S%d?", values[0]);
            if (yesno(buf, NULL, NULL, NULL)) {
                do_copyset(gno, values[1], gno, values[0]);
            }
        } else {
            err = TRUE;
        }
        break;
    case SetMenuMove12CB:
        if (n == 2) {
            sprintf(buf, "Replace S%d?", values[1]);
            if (yesno(buf, NULL, NULL, NULL)) {
                moveset(gno, values[0], gno, values[1]);
            }
        } else {
            err = TRUE;
        }
        break;
    case SetMenuMove21CB:
        if (n == 2) {
            sprintf(buf, "Replace S%d?", values[0]);
            if (yesno(buf, NULL, NULL, NULL)) {
                moveset(gno, values[1], gno, values[0]);
            }
        } else {
            err = TRUE;
        }
        break;
    case SetMenuSwapCB:
        if (n == 2) {
            swapset(gno, values[0], gno, values[1]);
        } else {
            err = TRUE;
        }
        break;
    case SetMenuNewFCB:
            create_leval_frame((void *) gno);
        break;
    case SetMenuNewSCB:
            if ((setno = nextset(gno)) != -1) {
                setcomment(gno, setno, "Editor");
                set_set_hidden(gno, setno, FALSE);
                create_ss_frame(gno, setno);
            } else {
                err = TRUE;
            }
        break;
    case SetMenuNewECB:
            if ((setno = nextset(gno)) != -1) {
                setcomment(gno, setno, "Editor");
                set_set_hidden(gno, setno, FALSE);
                do_ext_editor(gno, setno);
            } else {
                err = TRUE;
            }
        break;
    case SetMenuNewBCB:
            create_eblock_frame(gno);
        break;
    case SetMenuEditSCB:
        if (n == 1) {
            create_ss_frame(gno, values[0]);
        } else {
            err = TRUE;
        }
        break;
    case SetMenuEditECB:
        if (n == 1) {
            do_ext_editor(gno, values[0]);
        } else {
            err = TRUE;
        }
        break;
    case SetMenuPackCB:
        packsets(gno);
        break;
    default:
        err = TRUE;
        break;
    }

    if (n > 0) {
        xfree(values);
    }

    if (err == FALSE) {
        update_all();
        xdrawgraph();
    }
}

void hide_set_proc(void *data)
{
    set_menu_cb((ListStructure *) data, SetMenuHideCB);
}

void show_set_proc(void *data)
{
    set_menu_cb((ListStructure *) data, SetMenuShowCB);
}

void bringf_set_proc(void *data)
{
    set_menu_cb((ListStructure *) data, SetMenuBringfCB);
}

void sendb_set_proc(void *data)
{
    set_menu_cb((ListStructure *) data, SetMenuSendbCB);
}

void duplicate_set_proc(void *data)
{
    set_menu_cb((ListStructure *) data, SetMenuDuplicateCB);
}

void kill_set_proc(void *data)
{
    set_menu_cb((ListStructure *) data, SetMenuKillCB);
}

void killd_set_proc(void *data)
{
    set_menu_cb((ListStructure *) data, SetMenuKillDCB);
}

void copy12_set_proc(void *data)
{
    set_menu_cb((ListStructure *) data, SetMenuCopy12CB);
}

void copy21_set_proc(void *data)
{
    set_menu_cb((ListStructure *) data, SetMenuCopy21CB);
}

void move12_set_proc(void *data)
{
    set_menu_cb((ListStructure *) data, SetMenuMove12CB);
}

void move21_set_proc(void *data)
{
    set_menu_cb((ListStructure *) data, SetMenuMove21CB);
}

void swap_set_proc(void *data)
{
    set_menu_cb((ListStructure *) data, SetMenuSwapCB);
}

void newF_set_proc(void *data)
{
    set_menu_cb((ListStructure *) data, SetMenuNewFCB);
}

void newS_set_proc(void *data)
{
    set_menu_cb((ListStructure *) data, SetMenuNewSCB);
}

void newE_set_proc(void *data)
{
    set_menu_cb((ListStructure *) data, SetMenuNewECB);
}

void newB_set_proc(void *data)
{
    set_menu_cb((ListStructure *) data, SetMenuNewBCB);
}

void editS_set_proc(void *data)
{
    set_menu_cb((ListStructure *) data, SetMenuEditSCB);
}

void editE_set_proc(void *data)
{
    set_menu_cb((ListStructure *) data, SetMenuEditECB);
}

void pack_set_proc(void *data)
{
    set_menu_cb((ListStructure *) data, SetMenuPackCB);
}

void shownd_set_proc(int onoff, void *data)
{
    ListStructure *listp = (ListStructure *) data;
    SetChoiceData *sdata = (SetChoiceData *) listp->anydata;
    
    sdata->show_nodata = onoff;
    UpdateSetChoice(listp, sdata->gno);
}

void showh_set_proc(int onoff, void *data)
{
    ListStructure *listp = (ListStructure *) data;
    SetChoiceData *sdata = (SetChoiceData *) listp->anydata;
    
    sdata->show_hidden = onoff;
    UpdateSetChoice(listp, sdata->gno);
}

void view_comments_set_proc(int onoff, void *data)
{
    ListStructure *listp = (ListStructure *) data;
    SetChoiceData *sdata = (SetChoiceData *) listp->anydata;
    
    sdata->view_comments = onoff;
    UpdateSetChoice(listp, sdata->gno);
}

void update_set_proc(void *data)
{
    ListStructure *listp = (ListStructure *) data;
    SetChoiceData *sdata = (SetChoiceData *) listp->anydata;
    
    UpdateSetChoice(listp, sdata->gno);
}

void update_color_selectors(void)
{
    int i, j;
    CMap_entry *pcmap;
    
    for (i = 0, j = 0; i < number_of_colors(); i++) {
        pcmap = get_cmap_entry(i);
        if (pcmap != NULL && pcmap->ctype == COLOR_MAIN) {
            j++;
        }
    }
    ncolor_option_items = j;

    color_option_items = xrealloc(color_option_items,
                                    ncolor_option_items*sizeof(OptionItem));
    for (i = 0, j = 0; i < number_of_colors(); i++) {
        pcmap = get_cmap_entry(i);
        if (pcmap != NULL && pcmap->ctype == COLOR_MAIN) {
            color_option_items[j].value = i;
            color_option_items[j].label = get_colorname(i);
            j++;
        }
    }
    
    for (i = 0; i < ncolor_selectors; i++) {
        UpdateOptionChoice(color_selectors[i], 
                            ncolor_option_items, color_option_items);
        paint_color_selector(color_selectors[i]);
    }
    
}

OptionStructure *CreateColorChoice(Widget parent, char *s)
{
    OptionStructure *retvalp = NULL;

    ncolor_selectors++;
    color_selectors = xrealloc(color_selectors, 
                                    ncolor_selectors*sizeof(OptionStructure *));
    if (color_selectors == NULL) {
        errmsg("Malloc failed in CreateColorChoice()");
        return retvalp;
    }
    
    retvalp = CreateOptionChoice(parent, s, 4, 
                                ncolor_option_items, color_option_items);

    color_selectors[ncolor_selectors - 1] = retvalp;
    
    paint_color_selector(retvalp);
    
    return retvalp;
}

SpinStructure *CreateLineWidthChoice(Widget parent, char *s)
{
    return CreateSpinChoice(parent, s, 3, SPIN_TYPE_FLOAT, 0.0, MAX_LINEWIDTH, 0.5);
}

OptionStructure *CreateFormatChoice(Widget parent, char *s)
{
    OptionStructure *retval;
    
    retval = CreateOptionChoice(parent, s, 4, 31, fmt_option_items);
    
    return(retval);
}

OptionStructure *CreateASChoice(Widget parent, char *s)
{
    OptionStructure *retval;
    
    retval = CreateOptionChoice(parent, s, 1, 4, as_option_items);
    /* As init value, use this */
    SetOptionChoice(retval, autoscale_onread);
    
    return(retval);
}

Widget *CreatePrecisionChoice(Widget parent, char *s)
{
    Widget *w;
    
    w = CreatePanelChoice(parent, s,
                          11,
                          "0", "1", "2", "3", "4",
                          "5", "6", "7", "8", "9",
                          NULL);

    return(w);
}

Widget CreateAngleChoice(Widget parent, char *s)
{
    return CreateScale(parent, s, 0, 360, 10);
}

int GetAngleChoice(Widget w)
{
    return GetScaleValue(w);
}

void SetAngleChoice(Widget w, int angle)
{
    SetScaleValue(w, angle);
}

Widget CreateCharSizeChoice(Widget parent, char *s)
{
    return CreateScale(parent, s, 0, 1000, 25);
}

double GetCharSizeChoice(Widget w)
{
    return ((double) GetScaleValue(w)/100);
}

void SetCharSizeChoice(Widget w, double size)
{
    int value = (int) rint(size*100);
    SetScaleValue(w, value);
}

TransformStructure *CreateTransformDialogForm(Widget parent,
    char *s, int sel_type)
{
    TransformStructure *retval;
    
    retval = xmalloc(sizeof(TransformStructure));
    
    retval->form = CreateDialogForm(parent, s);
    
    retval->srcdest = CreateSrcDestSelector(retval->form, sel_type);
    AddDialogFormChild(retval->form, retval->srcdest->form);

/*
 *     retval->restr = CreateRestrictionChoice(retval->form, "Source data filtering");
 *     AddDialogFormChild(retval->form, retval->restr->frame);
 */
    
    return retval;
}

int GetTransformDialogSettings(TransformStructure *tdialog, int exclusive,
        int *gsrc, int *gdest,
        int *nssrc, int **svaluessrc, int *nsdest, int **svaluesdest)
{
    int gsrc_ok, gdest_ok;
    
    gsrc_ok = GetSingleListChoice(tdialog->srcdest->src->graph_sel, gsrc);
    gdest_ok = GetSingleListChoice(tdialog->srcdest->dest->graph_sel, gdest);
    if (gsrc_ok == RETURN_FAILURE || gdest_ok == RETURN_FAILURE) {
        errmsg("Please select single source and destination graphs");
	return RETURN_FAILURE;
    }
    
    *nssrc = GetListChoices(tdialog->srcdest->src->set_sel, svaluessrc);
    if (*nssrc == 0) {
        errmsg("No source sets selected");
	return RETURN_FAILURE;
    }    
    *nsdest = GetListChoices(tdialog->srcdest->dest->set_sel, svaluesdest);
    if (*nsdest != 0 && *nssrc != *nsdest) {
        errmsg("Different number of source and destination sets");
        xfree(*svaluessrc);
        xfree(*svaluesdest);
	return RETURN_FAILURE;
    }
    
    /* check for mutually exclusive selections */
    if (exclusive && *gsrc == *gdest && *nsdest != 0) {
        int i;
        for (i = 0; i < *nssrc; i++) {
            if ((*svaluessrc)[i] == (*svaluesdest)[i]) {
                xfree(*svaluessrc);
                xfree(*svaluesdest);
                errmsg("Source and destination set(s) are not mutually exclusive");
	        return RETURN_FAILURE;
            }
        }
    }
    
    return RETURN_SUCCESS;
}

int save_set_list(SetChoiceItem l)
{
    nplist++;
    plist = xrealloc(plist, nplist*sizeof(SetChoiceItem));
    plist[nplist - 1] = l;
    return nplist - 1;
}

void update_save_set_list( SetChoiceItem l, int newgr )
{
    plist[l.indx] = l;
    update_set_list( newgr, plist[l.indx] );
}

void update_set_lists(int gno)
{
    int i;

    if (gno == GRAPH_SELECT_CURRENT) {
        update_set_selectors(get_cg());
        update_ss_editors(get_cg());
    } else {
        update_set_selectors(gno);
        update_ss_editors(gno);
    }

    if (inwin) {
        for (i = 0; i < nplist; i++) {
            if (plist[i].gno == gno || 
                (gno == get_cg() && plist[i].gno == GRAPH_SELECT_CURRENT)) {
                update_set_list(gno, plist[i]);
            }
        }
    }
}

Widget CreateMenuHelpButton(Widget parent, char *label, char mnemonic,
    Widget form, char *ha)
{
    Widget wbut;
    
    wbut = CreateMenuButton(parent, label, mnemonic, HelpCB, ha);
    AddHelpCB(form, ha);
    
    return wbut;
}

void update_all(void)
{
    int gno = get_cg();
    
    if (!inwin) {
        return;
    }
    
    update_set_lists(gno);

    update_set_selectors(ALL_GRAPHS);
    update_ss_editors(ALL_GRAPHS);

    if (ReqUpdateColorSel == TRUE) {
        update_color_selectors();
        update_colors_frame();
        ReqUpdateColorSel = FALSE;
    }

    update_ticks(gno);
    update_props_items();
    update_hotlinks();
    update_prune_frame();
    update_locator_items(gno);
    set_stack_message();
    set_left_footer(NULL);
}

void update_all_cb(void *data)
{
    update_all();
}
