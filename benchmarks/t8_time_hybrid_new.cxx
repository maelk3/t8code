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
#include <t8_cmesh/t8_cmesh_examples.h>
#include <t8_cmesh_readmshfile.h>
#include <t8_schemes/t8_default/t8_default_cxx.hxx>
#include <sc_flops.h>
#include <sc_statistics.h>
#include <sc_options.h>

void
benchmark_new (const int init_level, const char *file, const int master, const int dim)
{

  t8_eclass_t eclass = T8_ECLASS_PYRAMID;
  sc_flopinfo_t fi, snapshot;
  sc_statistics_t *stats = sc_statistics_new (sc_MPI_COMM_WORLD);

  /* If the master argument is positive, then we read the cmesh
   * only on the master rank and is directly partitioned. */
  t8_cmesh_t cmesh;
  if (strcmp (file, "")) {
    const int partitioned_read = master >= 0;
    t8_cmesh_t gmesh_cmesh = t8_cmesh_from_msh_file (file, partitioned_read, sc_MPI_COMM_WORLD, dim, master, false);
    t8_cmesh_init (&cmesh);
    t8_cmesh_set_derive (cmesh, gmesh_cmesh);
    t8_cmesh_set_partition_uniform (cmesh, 0, t8_scheme_new_default_cxx ());
    t8_cmesh_commit (cmesh, sc_MPI_COMM_WORLD);
  }
  else {
    t8_productionf ("No mesh-file provided, use pyramid bigmesh instead\n");
    cmesh = t8_cmesh_new_bigmesh (eclass, 512, sc_MPI_COMM_WORLD);
  }

  const int num_runs = 2;
  sc_flops_start (&fi);
  for (int irun = 0; irun < num_runs; irun++) {
    t8_forest_t forest;
    t8_forest_init (&forest);
    t8_cmesh_ref (cmesh);
    t8_forest_set_cmesh (forest, cmesh, sc_MPI_COMM_WORLD);
    t8_forest_set_scheme (forest, t8_scheme_new_default_cxx ());
    t8_forest_set_level (forest, init_level);
    SC_FUNC_SNAP (stats, &fi, &snapshot);

    t8_forest_commit (forest);
    SC_FUNC_SHOT (stats, &fi, &snapshot);
    t8_forest_unref (&forest);
  }
  t8_cmesh_unref (&cmesh);

  sc_statistics_compute (stats);
  sc_statistics_print (stats, t8_get_package_id (), SC_LP_STATISTICS, 1, 1);
  sc_statistics_destroy (stats);
}

int
main (int argc, char **argv)
{
  /* init MPI */
  int mpiret = sc_MPI_Init (&argc, &argv);
  SC_CHECK_MPI (mpiret);

  /* init sc, p4est & t8code */
  sc_init (sc_MPI_COMM_WORLD, 1, 1, NULL, SC_LP_ESSENTIAL);
  p4est_init (NULL, SC_LP_ESSENTIAL);
  t8_init (SC_LP_STATISTICS);

  /* usage options */
  int initial_level;
  const char *file;
  int help_me;
  int dim;
  int master;
  sc_options_t *opt = sc_options_new (argv[0]);
  sc_options_add_switch (opt, 'h', "help", &help_me, "Print a help message");
  sc_options_add_int (opt, 'i', "initial_level", &initial_level, 0, "initial level for a uniform mesh");
  sc_options_add_string (opt, 'f', "file", &file, "", "Read cmesh from a msh file");
  sc_options_add_int (opt, 'd', "dim", &dim, 0, "dimension of the mesh");
  sc_options_add_int (opt, 'm', "master", &master, -1,
                      "If specified, the mesh is partitioned and all elements reside on process with rank master.");

  int first_argc = sc_options_parse (t8_get_package_id (), SC_LP_DEFAULT, opt, argc, argv);

  if (first_argc < 0 || first_argc != argc || help_me || initial_level < 0) {
    sc_options_print_usage (t8_get_package_id (), SC_LP_ERROR, opt, NULL);
    sc_options_destroy (opt);
    sc_finalize ();
    mpiret = sc_MPI_Finalize ();
    SC_CHECK_MPI (mpiret);
    return 1;
  }
  else {
    benchmark_new (initial_level, file, master, dim);
  }

  sc_options_destroy (opt);
  sc_finalize ();
  mpiret = sc_MPI_Finalize ();
  SC_CHECK_MPI (mpiret);

  return 0;
}