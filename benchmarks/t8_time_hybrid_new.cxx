/*
This file is part of t8code.
t8code is a C library to manage a collection (a forest) of multiple
connected adaptive space-trees of general element classes in parallel.

Copyright (C) 2024 the developers

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

/**
 * \file    Benchmark program to compare the hybrid new with the uniform new. 
 */

#include <t8_forest/t8_forest_general.h>
#include <sc_flops.h>
#include <sc_statistics.h>
#include <sc_options.h>

int
main (int argc, char **argv)
{
  /* init MPI */
  int mpiret = sc_MPI_Init (&argc, &argv);
  SC_CHECK_MPI (mpiret);

  /* init sc, p4est & t8code */
  sc_init (sc_MPI_COMM_WORLD, 1, 1, NULL, SC_LP_ESSENTIAL);
  p4est_init (NULL, SC_LP_ESSENTIAL);
  t8_init (SC_LP_ESSENTIAL);

  int initial_level;
  int use_old_version_new;
  int mesh;
  sc_options_t *opt = sc_options_new (argv[0]);
  sc_options_add_int (opt, `i`, "initial_level", &initial_level, 0, "initial level for a uniform mesh");
  sc_options_add_switch (opt, 'o', "old_version_new", &use_old_version_new,
                         "Use the new version that is not optimized for hybrid meshes");
  sc_options_add_int (opt, `m`, "mesh", &mesh, "Pick the mesh to use");

  int first_argc = sc_options_parse (t8_get_package_id (), SC_LP_DEFAULT, opt, argc, argv);

  return 0;
}