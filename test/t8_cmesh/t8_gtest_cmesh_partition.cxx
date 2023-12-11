/*
  This file is part of t8code.
  t8code is a C library to manage a collection (a forest) of multiple
  connected adaptive space-trees of general element classes in parallel.

  Copyright (C) 2015 the developers

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

#include <gtest/gtest.h>
#include <t8_cmesh.h>
#include <t8_schemes/t8_default/t8_default_cxx.hxx>
#include "t8_cmesh/t8_cmesh_trees.h"
#include "t8_cmesh/t8_cmesh_partition.h"
#include <t8_cmesh/t8_cmesh_testcases.h>
#include <t8_geometry/t8_geometry_with_vertices.hxx>
#include <test/t8_gtest_macros.hxx>
#include <t8_vec.h>

/* We create a cmesh, partition it and repartition it several times.
 * At the end we result in the same partition as at the beginning and we
 * compare this cmesh with the initial one. If they are equal the test is
 * passed.
 */

class t8_cmesh_partition_class: public testing::TestWithParam<int> {
 protected:
  void
  SetUp () override
  {
    cmesh_id = GetParam ();

    if (cmesh_id == 89 || (237 <= cmesh_id && cmesh_id <= 256)) {
      GTEST_SKIP ();
    }

    cmesh_original = t8_test_create_cmesh (cmesh_id);
  }

  int cmesh_id;
  t8_cmesh_t cmesh_original;
};

static void
test_cmesh_committed (t8_cmesh_t cmesh)
{
  ASSERT_TRUE (t8_cmesh_is_committed (cmesh)) << "Cmesh commit failed.";
  ASSERT_TRUE (t8_cmesh_trees_is_face_consistent (cmesh, cmesh->trees)) << "Cmesh face consistency failed.";
}

static void
test_cmesh_partition_compare_vertices (t8_cmesh_t partitioned, t8_cmesh_t original)
{
  const t8_locidx_t num_local_trees_part = t8_cmesh_get_num_local_trees (partitioned);
  t8_geometry_handler_t *handler_part = partitioned->geometry_handler;
  t8_geometry_handler_t *handler_orig = original->geometry_handler;
  EXPECT_TRUE (t8_geom_handler_is_committed (handler_part));
  EXPECT_TRUE (t8_geom_handler_is_committed (handler_orig));
  /* The original cmesh has to be replicated on all processes. */
  ASSERT_FALSE (t8_cmesh_is_partitioned (original));
  for (t8_locidx_t itree = 0; itree < num_local_trees_part; itree++) {
    t8_eclass_t eclass_part = t8_cmesh_get_tree_class (partitioned, itree);
    const t8_gloidx_t itree_global = t8_cmesh_get_global_id (partitioned, itree);
    t8_eclass_t eclass_orig = t8_cmesh_get_tree_class (original, itree_global);
    EXPECT_EQ (eclass_orig, eclass_part);
    if (t8_geom_handler_get_num_geometries (handler_part) > 0) {
      const t8_geometry_type_t geo_type = t8_geometry_get_type (partitioned, itree_global);
      if (geo_type == T8_GEOMETRY_TYPE_ZERO || geo_type == T8_GEOMETRY_TYPE_UNDEFINED) {
        /* Can't compare the vertices if there aren't any. */
        continue;
      }

      t8_geom_handler_update_tree (handler_part, partitioned, itree_global);
      const double *vertices_part = handler_part->active_geometry->t8_geom_get_active_tree_vertices ();
      t8_geom_handler_update_tree (handler_orig, original, itree_global);
      const double *vertices_orig = handler_orig->active_geometry->t8_geom_get_active_tree_vertices ();
      const int num_vertices = t8_eclass_num_vertices[eclass_part];
      ASSERT_TRUE (vertices_orig != NULL);
      ASSERT_TRUE (vertices_part != NULL);
      for (int ivertex = 0; ivertex < num_vertices; ivertex++) {
        EXPECT_NEAR (t8_vec_dist (vertices_orig + 3 * ivertex, vertices_part + 3 * ivertex), 0, 10 * T8_PRECISION_EPS);
      }
    }
  }
}

TEST_P (t8_cmesh_partition_class, test_cmesh_partition_concentrate)
{

  const int level = 11;
  int mpisize;
  int mpiret;
  int mpirank;
  t8_cmesh_t cmesh_partition;
  t8_cmesh_t cmesh_partition_new1;
  t8_cmesh_t cmesh_partition_new2;
  t8_shmem_array_t offset_concentrate;

  test_cmesh_committed (cmesh_original);

  t8_cmesh_ref (cmesh_original);
  t8_cmesh_t cmesh_tmp = cmesh_original;

  mpiret = sc_MPI_Comm_size (sc_MPI_COMM_WORLD, &mpisize);
  SC_CHECK_MPI (mpiret);
  /* Set up the partitioned cmesh */
  for (int i = 0; i < 2; i++) {
    t8_cmesh_init (&cmesh_partition);
    t8_cmesh_set_derive (cmesh_partition, cmesh_tmp);
    /* Uniform partition according to level */
    t8_cmesh_set_partition_uniform (cmesh_partition, level, t8_scheme_new_default_cxx ());
    t8_cmesh_commit (cmesh_partition, sc_MPI_COMM_WORLD);

    test_cmesh_committed (cmesh_partition);
    cmesh_tmp = cmesh_partition;
    test_cmesh_partition_compare_vertices (cmesh_partition, cmesh_original);
  }

  t8_shmem_array_t part_table = t8_cmesh_get_partition_table (cmesh_partition);

  mpiret = sc_MPI_Comm_rank (sc_MPI_COMM_WORLD, &mpirank);
  SC_CHECK_MPI (mpiret);

  t8_gloidx_t first_tree = t8_shmem_array_get_gloidx (part_table, mpirank);
  t8_debugf ("[D] first tree: %li\n", first_tree);
  /* Since we want to repartition the cmesh_partition in each step,
   * we need to ref it. This ensures that we can still work with it after
   * another cmesh is derived from it. 
   */
  t8_cmesh_ref (cmesh_partition);

  cmesh_partition_new1 = cmesh_partition;

  /* We repartition the cmesh to be concentrated on each rank once */
  for (int irank = 0; irank < mpisize; irank++) {
    t8_cmesh_init (&cmesh_partition_new2);
    t8_cmesh_set_derive (cmesh_partition_new2, cmesh_partition_new1);
    /* Create an offset array where each tree resides on irank */
    offset_concentrate = t8_cmesh_offset_concentrate (0, sc_MPI_COMM_WORLD, t8_cmesh_get_num_trees (cmesh_partition));
    /* Set the new cmesh to be partitioned according to that offset */
    t8_cmesh_set_partition_offsets (cmesh_partition_new2, offset_concentrate);
    /* Commit the cmesh and test if successful */
    t8_cmesh_commit (cmesh_partition_new2, sc_MPI_COMM_WORLD);
    test_cmesh_committed (cmesh_partition_new2);

    /* Switch the rolls of the cmeshes */
    cmesh_partition_new1 = cmesh_partition_new2;
    cmesh_partition_new2 = NULL;
  }
  t8_debugf ("[D] num_local_trees: %i\n", t8_cmesh_get_num_local_trees (cmesh_partition_new1));
  /* We partition the resulting cmesh according to a uniform level refinement.
   * This cmesh should now be equal to the initial cmesh. 
   */
  t8_cmesh_ref (cmesh_partition);
  for (int i = 0; i < 2; i++) {
    t8_cmesh_init (&cmesh_partition_new2);
    t8_cmesh_set_derive (cmesh_partition_new2, cmesh_partition);
    t8_cmesh_set_partition_uniform (cmesh_partition_new2, level, t8_scheme_new_default_cxx ());
    t8_cmesh_commit (cmesh_partition_new2, sc_MPI_COMM_WORLD);
    cmesh_partition = cmesh_partition_new2;
  }

  part_table = t8_cmesh_get_partition_table (cmesh_partition_new1);

  cmesh_partition->geometry_handler->active_tree = first_tree;
  first_tree = t8_shmem_array_get_gloidx (part_table, mpirank);
  t8_debugf ("[D] cmesh-partition-new1 first tree: %li\n", first_tree);

  test_cmesh_partition_compare_vertices (cmesh_partition_new2, cmesh_original);
  ASSERT_TRUE (t8_cmesh_is_equal (cmesh_partition_new2, cmesh_partition)) << "Cmesh equality check failed.";
  t8_cmesh_destroy (&cmesh_partition_new2);
  //t8_cmesh_unref (&cmesh_partition);
  t8_cmesh_unref (&cmesh_original);
}

/* Test all cmeshes over all different inputs we get through their id */
INSTANTIATE_TEST_SUITE_P (t8_gtest_cmesh_partition, t8_cmesh_partition_class, AllCmeshs);
