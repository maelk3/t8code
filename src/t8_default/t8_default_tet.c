/*
  This file is part of t8code.
  t8code is a C library to manage a collection (a forest) of multiple
  connected adaptive space-trees of general element types in parallel.

  Copyright (C) 2010 The University of Texas System
  Written by Carsten Burstedde, Lucas C. Wilcox, and Tobin Isaac

  t8code is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  t8code is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with t8code; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include "t8_default_common.h"
#include "t8_default_tet.h"

typedef int8_t	t8_default_tet_type_t;
typedef int8_t t8_default_cube_id;

struct t8_default_tet_id{
    t8_default_tet_type_t	type;
    t8_tcoord_t	anchor_coordinates[3];
};

int				t8_tet_cid_type_to_parenttype[8][6]={
                    {0,1,2,3,4,5},
                    {0,1,1,1,0,0},
                    {2,2,2,3,3,3},
                    {1,1,2,2,2,1},
                    {5,5,4,4,4,5},
                    {0,0,0,5,5,5},
                    {4,3,3,3,4,4},
                    {0,1,2,3,4,5}
};

static              size_t
t8_default_tet_size (void)
{
  return sizeof (t8_tet_t);
}

t8_type_scheme_t   *
t8_default_scheme_new_tet (void)
{
  t8_type_scheme_t   *ts;

  ts = T8_ALLOC (t8_type_scheme_t, 1);

  ts->elem_size = t8_default_tet_size;

  ts->elem_new = t8_default_mempool_alloc;
  ts->elem_destroy = t8_default_mempool_free;
  ts->ts_destroy = t8_default_scheme_mempool_destroy;
  ts->ts_context = sc_mempool_new (sizeof (t8_tet_t));

  return ts;
}

t8_default_tet_id_t *t8_default_tet_id_new(t8_default_tet_type_t type,t8_tcoord_t anchor_coordinates[3]){
    t8_default_tet_id_t *tet_id;
    int i;

    tet_id=T8_ALLOC(t8_default_tet_id_t,1);
    tet_id->type=type;
    for(i=0;i<3;i++){
        tet_id->anchor_coordinates[i]=anchor_coordinates[i];
    }
    return tet_id;
}

void t8_default_tet_id_destroy(t8_default_tet_id_t *tid){
    T8_FREE(tid);
}

t8_default_cube_id t8_default_tet_compute_cubeid(const t8_default_tet_id_t *tid){
    t8_default_cube_id cid=0;

    cid=tid->anchor_coordinates[0]&1+tid->anchor_coordinates[1]&2+tid->anchor_coordinates[2]&4;
    return cid;
}

t8_default_tet_id_t *t8_default_tet_parent_tetid(const t8_default_tet_id_t *tid,const int8_t level){
    t8_default_cube_id cid;
    t8_default_tet_type_t parent_type;
    t8_tcoord_t parent_coord[3],h;
    int i;

    /* Compute type of parent */
    cid = t8_default_tet_compute_cubeid(tid);
    parent_type = t8_tet_cid_type_to_parenttype[cid][tid->type];
    /* Compute anchor coordinates of parent */
    h = T8_TET_ROOT_LEN(level);
    for(i=0;i<3;i++){
        parent_coord[i] = tid->anchor_coordinates[i]& ~h;
    }

    return t8_default_tet_id_new(parent_type,parent_coord);
}
