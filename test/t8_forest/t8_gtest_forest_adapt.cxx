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
 * \file t8_gtest_forest_adapt.cxx
 * 
 * In this test we check that the adapt function works correctly. 
 * 
 */

#include <gtest/gtest.h>
#include "test/t8_cmesh_generator/t8_cmesh_example_sets.hxx"
#include "test/t8_cmesh_generator/t8_cmesh_parametrized_examples/t8_cmesh_new_bigmesh_param.hxx"
#include "test/t8_cmesh_generator/t8_gtest_cmesh_cartestian_product.hxx"
#include "test/t8_gtest_custom_assertion.hxx"
#include <t8_forest/t8_forest_general.h>
#include <t8_schemes/t8_default/t8_default_cxx.hxx>

/**
 * A class constructing a forest from different cmeshes.
 * 
 */
class t8_forest_adapt: public testing::TestWithParam<cmesh_example_base *> {
 protected:
  void
  SetUp () override
  {
    t8_cmesh_t cmesh = GetParam ()->cmesh_create ();
    t8_cmesh_ref (cmesh);
    scheme = t8_scheme_new_default_cxx ();
    t8_scheme_cxx *other = t8_scheme_new_default_cxx ();
    forest = t8_forest_new_uniform (cmesh, scheme, 0, 1, sc_MPI_COMM_WORLD);
    t8_forest_t forest_tmp;
    int *marker = NULL;
    /* Iteratively refine the forest. */
    for (int ilevel = 1; ilevel <= level; ++ilevel) {
      t8_forest_init (&forest_tmp);
      const t8_locidx_t num_elems = t8_forest_get_local_num_elements (forest);
      marker = T8_ALLOC (int, num_elems);
      for (int imarker = 0; imarker < num_elems; imarker++) {
        marker[imarker] = 1;
      }
      t8_forest_set_level (forest_tmp, ilevel);
      t8_forest_set_adapt_batch (forest_tmp, forest, marker);
      t8_forest_set_partition (forest_tmp, forest, 0);
      T8_ASSERT (marker != NULL);
      t8_forest_commit (forest_tmp);
      forest = forest_tmp;
      T8_FREE (marker);
    }
    forest_compare = t8_forest_new_uniform (cmesh, other, level, 1, sc_MPI_COMM_WORLD);
  }

  void
  TearDown () override
  {
    t8_forest_unref (&forest);
    t8_forest_unref (&forest_compare);
  }
  t8_forest_t forest;
  t8_forest_t forest_compare;
  t8_scheme_cxx *scheme;
  int level = 3;
};

/**
 * Adapt callback to refine every element
 * 
 */
int
refine_everything (t8_forest_t forest, t8_forest_t forest_from, t8_locidx_t which_tree, t8_locidx_t lelement_id,
                   t8_eclass_scheme_c *ts, const int is_family, const int num_elements, t8_element_t *elements[])
{
  return 1;
}

int
refine_second (t8_forest_t forest, t8_forest_t forest_from, t8_locidx_t which_tree, t8_locidx_t lelement_id,
               t8_eclass_scheme_c *ts, const int is_family, const int num_elements, t8_element_t *elements[])
{
  return lelement_id % 2;
}

int
coarsen (t8_forest_t forest, t8_forest_t forest_from, t8_locidx_t which_tree, t8_locidx_t lelement_id,
         t8_eclass_scheme_c *ts, const int is_family, const int num_elements, t8_element_t *elements[])
{
  return is_family ? -1 : 0;
}

/**
 * Iteratively refine a forest and check that it is equal to uniform refined forest. 
 * 
 */
TEST_P (t8_forest_adapt, batch_adapt)
{

  /* Check equality */
  EXPECT_FOREST_LOCAL_EQ (forest, forest_compare);
}

void
build_marker_coarsen (const t8_forest_t forest, int *marker, int coarsen_level)
{
  const t8_locidx_t num_trees = t8_forest_get_num_local_trees (forest);
  t8_locidx_t marker_id = 0;
  for (t8_locidx_t itree = 0; itree < num_trees; ++itree) {
    const t8_eclass_t tree_class = t8_forest_get_tree_class (forest, itree);
    const t8_eclass_scheme_c *tree_scheme = t8_forest_get_eclass_scheme (forest, tree_class);
    const t8_locidx_t num_elems = t8_forest_get_tree_num_elements (forest, itree);
    for (t8_locidx_t ielem = 0; ielem < num_elems; ++ielem) {
      const t8_element_t *elem = t8_forest_get_element_in_tree (forest, itree, ielem);
      const int elem_level = tree_scheme->t8_element_level (elem);
      marker[marker_id] = (elem_level == coarsen_level) ? -1 : 0;
    }
  }
}

TEST_P (t8_forest_adapt, second_elem_refined)
{
  t8_forest_t forest_tmp;
  t8_forest_init (&forest_tmp);
  t8_locidx_t num_elems = t8_forest_get_local_num_elements (forest);
  EXPECT_GT (num_elems, 0);
  int *marker = T8_ALLOC (int, num_elems);
  for (int imarker = 0; imarker < num_elems; imarker++) {
    marker[imarker] = imarker % 2;
    T8_ASSERT (marker[imarker] == 0 || marker[imarker] == 1);
  }
  t8_forest_set_adapt_batch (forest_tmp, forest, marker);
  t8_forest_commit (forest_tmp);
  forest = forest_tmp;
  T8_FREE (marker);

  t8_forest_init (&forest_tmp);
  t8_forest_set_adapt (forest_tmp, forest_compare, refine_second, 0);
  t8_forest_commit (forest_tmp);
  forest_compare = forest_tmp;

  EXPECT_FOREST_LOCAL_EQ (forest, forest_compare);

  t8_forest_init (&forest_tmp);
  num_elems = t8_forest_get_local_num_elements (forest);
  marker = T8_ALLOC (int, num_elems);
  build_marker_coarsen (forest, marker, level);
  t8_forest_set_adapt_batch (forest_tmp, forest, marker);
  t8_forest_commit (forest_tmp);
  forest = forest_tmp;
  T8_FREE (marker);

  t8_forest_init (&forest_tmp);
  t8_forest_set_adapt (forest_tmp, forest_compare, coarsen, 0);
  t8_forest_commit (forest_tmp);
  forest_compare = forest_tmp;

  EXPECT_FOREST_LOCAL_EQ (forest, forest_compare);
}

/**
 * Iteratively refine a forest and check that it is equal to uniform refined forest. 
 * 
 */
#if 0
TEST_P (t8_forest_adapt, compare_uniform)
{

  t8_forest_t forest_tmp;
  /* Iteratively refine the forest. */
  for (int ilevel = 1; ilevel <= level; ++ilevel) {
    t8_forest_init (&forest_tmp);
    t8_forest_set_level (forest_tmp, ilevel);
    t8_forest_set_adapt (forest_tmp, forest, refine_everything, 0);
    t8_forest_set_partition (forest_tmp, forest, 0);
    t8_forest_commit (forest_tmp);
    forest = forest_tmp;
  }

  /* Check equality */
  const t8_locidx_t num_trees = t8_forest_get_num_local_trees (forest);
  for (t8_locidx_t itree = 0; itree < num_trees; ++itree) {
    const t8_eclass_t tree_class = t8_forest_get_tree_class (forest, itree);
    t8_eclass_scheme_c *eclass_scheme = t8_forest_get_eclass_scheme (forest, tree_class);

    const t8_locidx_t num_elements = t8_forest_get_tree_num_elements (forest, itree);
    for (t8_locidx_t ielem = 0; ielem < num_elements; ielem++) {
      const t8_element_t *element = t8_forest_get_element_in_tree (forest, itree, ielem);
      const t8_element_t *other = t8_forest_get_element_in_tree (forest_compare, itree, ielem);
      EXPECT_ELEM_EQ (eclass_scheme, element, other);
    }
  }
}
#endif

INSTANTIATE_TEST_SUITE_P (t8_gtest_forest_adapt, t8_forest_adapt, AllCmeshsParam, pretty_print_base_example);