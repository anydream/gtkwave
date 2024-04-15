/*
 * Copyright (c) Tony Bybell 2001-2010
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <config.h>
#include "globals.h"
#include <stdio.h>

#include <unistd.h>
#ifndef __MINGW32__
#include <sys/mman.h>
#endif

#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "symbol.h"
#include "vcd.h"
#include "bsearch.h"

#ifdef _WAVE_HAVE_JUDY
#include <Judy.h>
#endif

/*
 * s_selected accessors
 */

// TODO: reenable judy support
// #ifdef _WAVE_HAVE_JUDY
//
// char get_s_selected(GwSymbol *s)
// {
//     int rc = Judy1Test(GLOBALS->s_selected, (Word_t)s, PJE0);
//
//     return (rc);
// }
//
// char set_s_selected(GwSymbol *s, char value)
// {
//     if (value) {
//         Judy1Set((Pvoid_t)&GLOBALS->s_selected, (Word_t)s, PJE0);
//     } else {
//         Judy1Unset((Pvoid_t)&GLOBALS->s_selected, (Word_t)s, PJE0);
//     }
//
//     return (value);
// }
//
// void destroy_s_selected(void)
// {
//     Judy1FreeArray(&GLOBALS->s_selected, PJE0);
//
//     GLOBALS->s_selected = NULL;
// }
//
// #else

char get_s_selected(GwSymbol *s)
{
    return (s->s_selected);
}

char set_s_selected(GwSymbol *s, char value)
{
    return ((s->s_selected = value));
}

void destroy_s_selected(void)
{
    /* nothing */
}

// #endif

/*
 * hash create/destroy
 */
void sym_hash_initialize(void *g)
{
    // TODO: reenable judy support
    // #ifdef _WAVE_HAVE_JUDY
    //     ((struct Global *)g)->sym_judy = NULL;
    // #else
    ((struct Global *)g)->sym_hash = (GwSymbol **)calloc_2(SYMPRIME, sizeof(GwSymbol *));
    // #endif
}

void sym_hash_destroy(void *g)
{
    struct Global *gg = (struct Global *)g;

    // TODO: reenable judy support
    // #ifdef _WAVE_HAVE_JUDY
    //
    //     JudySLFreeArray(&gg->sym_judy, PJE0);
    //     gg->sym_judy = NULL;
    //
    // #else

    if (gg->sym_hash) {
        free_2(gg->sym_hash);
        gg->sym_hash = NULL;
    }

    // #endif
}

/*
 * Generic hash function for symbol names...
 */
int hash(char *s)
{
    char *p;
    char ch;
    unsigned int h = 0, h2 = 0, pos = 0, g;
    for (p = s; *p; p++) {
        ch = *p;
        h2 <<= 3;
        h2 -= ((unsigned int)ch + (pos++)); /* this handles stranded vectors quite well.. */

        h = (h << 4) + ch;
        if ((g = h & 0xf0000000)) {
            h = h ^ (g >> 24);
            h = h ^ g;
        }
    }

    h ^= h2; /* combine the two hashes */

    return h % SYMPRIME;
}

/*
 * add symbol to table.  no duplicate checking
 * is necessary as aet's are "correct."
 */
GwSymbol *symadd(char *name, int hv)
{
    GwSymbol *s = (GwSymbol *)calloc_2(1, sizeof(GwSymbol));

    // TODO: reenable judy support
    // #ifdef _WAVE_HAVE_JUDY
    //     (void)hv;
    //
    //     PPvoid_t PPValue = JudySLIns(&GLOBALS->sym_judy, (uint8_t *)name, PJE0);
    //     *((GwSymbol **)PPValue) = s;
    //
    // #else

    strcpy(s->name = (char *)malloc_2(strlen(name) + 1), name);
    s->sym_next = GLOBALS->sym_hash[hv];
    GLOBALS->sym_hash[hv] = s;

    // #endif
    return (s);
}

GwSymbol *symadd_name_exists(char *name, int hv)
{
    GwSymbol *s = (GwSymbol *)calloc_2(1, sizeof(GwSymbol));

    // TODO: reenable judy support
    // #ifdef _WAVE_HAVE_JUDY
    //     (void)hv;
    //
    //     PPvoid_t PPValue = JudySLIns(&GLOBALS->sym_judy, (uint8_t *)name, PJE0);
    //     *((GwSymbol **)PPValue) = s;
    //
    //     s->name = name; /* redundant for now */
    //
    // #else

    s->name = name;
    s->sym_next = GLOBALS->sym_hash[hv];
    GLOBALS->sym_hash[hv] = s;

    // #endif

    return (s);
}

/*
 * find a slot already in the table...
 */
static GwSymbol *symfind_2(char *s, unsigned int *rows_return)
{
    int hv;
    GwSymbol *temp;

    if (!GLOBALS->facs_are_sorted) {
        // TODO: reenable judy support
        // #ifdef _WAVE_HAVE_JUDY
        //         PPvoid_t PPValue = JudySLGet(GLOBALS->sym_judy, (uint8_t *)s, PJE0);
        //
        //         if (PPValue) {
        //             return (*(GwSymbol **)PPValue);
        //         }
        // #else
        hv = hash(s);
        if (!(GLOBALS->sym_hash) || !(temp = GLOBALS->sym_hash[hv]))
            return (NULL); /* no hash entry, add here wanted to add */

        while (temp) {
            if (!strcmp(temp->name, s)) {
                return (temp); /* in table already */
            }
            if (!temp->sym_next)
                break;
            temp = temp->sym_next;
        }
        // #endif
        return (NULL); /* not found, add here if you want to add*/
    } else /* no sense hashing if the facs table is built */
    {
        GwSymbol *sr;
        DEBUG(printf("BSEARCH: %s\n", s));

        sr = bsearch_facs(s, rows_return);
        if (sr) {
        } else {
            /* this is because . is > in ascii than chars like $ but . was converted to 0x1 on sort
             */
            char *s2;
            int i;
            int mat;

            gboolean has_escaped_names = gw_dump_file_has_escaped_names(GLOBALS->dump_file);
            if (!has_escaped_names) {
                return (sr);
            }

            GwFacs *facs = gw_dump_file_get_facs(GLOBALS->dump_file);
            guint numfacs = gw_facs_get_length(facs);

            if (GLOBALS->facs_have_symbols_state_machine == 0) {
                if (has_escaped_names) {
                    mat = 1;
                } else {
                    mat = 0;

                    for (i = 0; i < numfacs; i++) {
                        char *hfacname = NULL;

                        GwSymbol *fac = gw_facs_get(facs, i);

                        hfacname = fac->name;
                        s2 = hfacname;
                        while (*s2) {
                            if (*s2 < GLOBALS->hier_delimeter) {
                                mat = 1;
                                break;
                            }
                            s2++;
                        }

                        /* if(was_packed) { free_2(hfacname); } ...not needed with
                         * HIER_DEPACK_STATIC */
                        if (mat) {
                            break;
                        }
                    }
                }

                if (mat) {
                    GLOBALS->facs_have_symbols_state_machine = 1;
                } else {
                    GLOBALS->facs_have_symbols_state_machine = 2;
                } /* prevent code below from executing */
            }

            if (GLOBALS->facs_have_symbols_state_machine == 1) {
                mat = 0;
                for (i = 0; i < numfacs; i++) {
                    char *hfacname = NULL;

                    GwSymbol *fac = gw_facs_get(facs, i);

                    hfacname = fac->name;
                    if (!strcmp(hfacname, s)) {
                        mat = 1;
                    }

                    /* if(was_packed) { free_2(hfacname); } ...not needed with HIER_DEPACK_STATIC */
                    if (mat) {
                        sr = fac;
                        break;
                    }
                }
            }
        }

        return (sr);
    }
}

GwSymbol *symfind(char *s, unsigned int *rows_return)
{
    GwSymbol *s_pnt = symfind_2(s, rows_return);

    if (!s_pnt) {
        int len = strlen(s);
        if (len) {
            char ch = s[len - 1];
            if ((ch != ']') && (ch != '}')) {
                char *s2 = g_alloca(len + 4);
                memcpy(s2, s, len);
                strcpy(s2 + len, "[0]"); /* bluespec vs modelsim */

                s_pnt = symfind_2(s2, rows_return);
            }
        }
    }

    return (s_pnt);
}
