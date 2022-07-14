/*
  This file is part of t8code.
  t8code is a C library to manage a collection (a forest) of multiple
  connected adaptive space-trees of general element types in parallel.

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

/* Description:
 * In this example, we construct an adaptive mesh that should either be balanced or transitioned.
 * After that, we iterate through all elements and all faces of the mesh and call the LFN function to compute the neighbor elements.
 * This way, balanced and transitioned meshes can be compared.
 */

#include <t8_schemes/t8_quads_transition/t8_transition/t8_transition_quad_cxx.hxx>
#include <t8_schemes/t8_quads_transition/t8_transition_cxx.hxx>
#include <t8_vec.h>
#include <example/common/t8_example_common.h>

/* In this example, a simple refinement criteria is used to construct an adapted and transitioned forest. 
 * Afterwards, we iterate through all elements and all faces of the this forest in order to test the leaf_face_neighbor function that will determine all neighbor elements. */

typedef struct
{
  double              mid_point[3];
  double              radius;
} t8_basic_sphere_data_t;

/* Compute the distance to a sphere around a mid_point with given radius. */
static double
t8_basic_level_set_sphere (const double x[3], double t, void *data)
{
  t8_basic_sphere_data_t *sdata = (t8_basic_sphere_data_t *) data;
  double             *M = sdata->mid_point;

  return t8_vec_dist (M, x) - sdata->radius;
}

void
t8_print_stats(int global_num_elements, int local_num_elements, int num_quad_elems, int subelement_count, int LFN_call_count, float time_LFN, float time_LFN_per_call)
{
  t8_productionf ("|+++++++++++++++++++++++++ final statistics +++++++++++++++++++++++++|\n");
  t8_productionf ("|    Global #elements:     %i\n", global_num_elements);
  t8_productionf ("|    Local #elements:      %i  (#quads: %i, #subelements: %i)\n", local_num_elements, num_quad_elems, subelement_count);
  t8_productionf ("|    #LFN calls:           %i\n", LFN_call_count);
  t8_productionf ("|    LFN runtime total:    %f\n", time_LFN);
  t8_productionf ("|    LFN runtime per call: %.9f\n", time_LFN_per_call);
  t8_productionf ("|++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++|\n");
}

/* Computing all neighbor elements in forest_adapt */
void
t8_test_LFN (const t8_forest_t forest_adapt)
{
  /* Collecting data of the adapted forest */
  int                 global_num_elements =
    t8_forest_get_global_num_elements (forest_adapt);
  int                 local_num_elements =
    t8_forest_get_local_num_elements (forest_adapt);
  int                 global_num_trees =
    t8_forest_get_num_global_trees (forest_adapt);
  const t8_element_t *current_element;
  t8_locidx_t         ltree_id = 0, forest_is_balanced = 1;
  int                *dual_faces;
  int                 num_neighbors;
  t8_element_t      **neighbor_leafs;
  t8_locidx_t        *element_indices;
  t8_eclass_scheme_c *neigh_scheme;
  t8_eclass_t         eclass;
  t8_eclass_scheme_c *ts;
  double time_LFN = 0;
  int subelement_count = 0;
  int LFN_call_count = 0;

  /* we only allow one tree with id 0 in this testcase and the current element must come from a valid index within the forest (as well as its face index) */
  T8_ASSERT (global_num_trees == 1); /* TODO: enable multiple trees for this example */
  T8_ASSERT (ltree_id == 0);

  eclass = t8_forest_get_tree_class (forest_adapt, ltree_id);
  ts = t8_forest_get_eclass_scheme (forest_adapt, eclass);
  
  t8_debugf("Into element loop with %i local elements\n", local_num_elements);
  
  /* the leaf_face_neighbor function determins neighbor elements of current_element at face face_id in a balanced forest forest_adapt */
  int element_index_in_tree;
  int face_id;
  int neighbor_count;
  for (element_index_in_tree = 0; element_index_in_tree < local_num_elements; ++element_index_in_tree) {

    /* determing the current element according to the given tree id and element id within the tree */
    current_element =
      t8_forest_get_element_in_tree (forest_adapt, ltree_id,
                                    element_index_in_tree);

    if (ts->t8_element_is_subelement (current_element)) subelement_count++;;

#if T8_ENABLE_DEBUG /* print the current element */
    t8_debugf("******************** Current element: ********************\n");
    t8_debugf("Current element has local index %i of %i\n", element_index_in_tree, local_num_elements);
    ts->t8_element_print_element(current_element);
#endif

    for (face_id = 0; face_id < ts->t8_element_num_faces(current_element); ++face_id) {
      LFN_call_count++;
      time_LFN -= sc_MPI_Wtime ();
      t8_forest_leaf_face_neighbors (forest_adapt, ltree_id, current_element,
                                 &neighbor_leafs, face_id, &dual_faces,
                                 &num_neighbors, &element_indices,
                                 &neigh_scheme, forest_is_balanced);
      time_LFN += sc_MPI_Wtime ();

      /* free memory */
      if (num_neighbors > 0) {

#if T8_ENABLE_DEBUG /* print all neighbor elements */
        for (neighbor_count = 0; neighbor_count < num_neighbors; neighbor_count++) {
          t8_debugf ("***** Neighbor %i of %i at face %i: *****\n", neighbor_count+1, num_neighbors, face_id);
          t8_debugf ("Neighbor has local index %i of %i\n", element_indices[neighbor_count], local_num_elements);
          ts->t8_element_print_element(neighbor_leafs[neighbor_count]);
        }
#endif

        neigh_scheme->t8_element_destroy (num_neighbors, neighbor_leafs);

        T8_FREE (element_indices);
        T8_FREE (neighbor_leafs);
        T8_FREE (dual_faces);
      }
      else {
#if T8_ENABLE_DEBUG /* no neighbor in this case */
        t8_debugf ("***** Neighbor at face %i: *****\n", face_id);
        t8_debugf ("There is no neighbor (domain boundary).\n");
        t8_debugf("\n");
#endif
      }

    } /* end of face loop */

  } /* end of element loop */

  t8_print_stats(global_num_elements, local_num_elements, local_num_elements-subelement_count, subelement_count, LFN_call_count, time_LFN, time_LFN/(float) LFN_call_count);
}

/* Initializing and adapting a forest */
static void
t8_refine_transition (t8_eclass_t eclass)
{
  t8_productionf ("Into the t8_refine_transition function");

  t8_forest_t         forest;
  t8_forest_t         forest_adapt;
  t8_cmesh_t          cmesh;
  char                filename[BUFSIZ];

  /* ************************************************* Case Settings ************************************************* */

    /* refinement setting */
    int                 initlevel = 3;    /* initial uniform refinement level */
    int                 adaptlevel = 3;
    int                 minlevel = initlevel;     /* lowest level allowed for coarsening (minlevel <= initlevel) */
    int                 maxlevel = initlevel + adaptlevel;     /* highest level allowed for refining */

    /* adaptation setting */
    int                 do_balance = 0;
    int                 do_transition = 1;

    /* cmesh settings (only one of the following suggestions should be one, the others 0) */
    int                 single_tree = 1;
    int                 multiple_tree = 0, num_x_trees = 2, num_y_trees = 1;
    int                 hybrid_cmesh = 0;

    /* partition setting */
    int                 do_partition = 1;

    /* ghost setting */
    int                 do_ghost = 1;
    int                 ghost_version = 3;

    /* vtk setting */
    int                 do_vtk = 1;

    /* LFN settings */
    int                 do_LFN_test = 1;
    
    /* Monitoring */
    int                 do_LFN_runtime_stats;

  /* *************************************************************************************************************** */

  /* initializing the forests */
  t8_forest_init (&forest);
  t8_forest_init (&forest_adapt);

  /* building the cmesh, using the initlevel */
  if (single_tree) {            /* single quad cmesh */
    cmesh = t8_cmesh_new_hypercube (eclass, sc_MPI_COMM_WORLD, 0, 0, 0);
  }
  else if (multiple_tree) {          /* p4est_connectivity_new_brick (num_x_trees, num_y_trees, 0, 0) -> cmesh of (num_x_trees x num_y_trees) many quads */
    p4est_connectivity_t *brick =
      p4est_connectivity_new_brick (num_x_trees, num_y_trees, 0, 0);
    cmesh = t8_cmesh_new_from_p4est (brick, sc_MPI_COMM_WORLD, 0);
    p4est_connectivity_destroy (brick);
  }
  else if (hybrid_cmesh) {           /* TODO: this does not work at the moment */
    cmesh = t8_cmesh_new_hypercube_hybrid (2, sc_MPI_COMM_WORLD, 0, 0);
  }
  else {
    SC_ABORT ("Specify cmesh.");
  }

  /* building the cmesh, using the initlevel */
  t8_forest_set_cmesh (forest, cmesh, sc_MPI_COMM_WORLD);
  t8_forest_set_scheme (forest, t8_scheme_new_subelement_cxx ());
  t8_forest_set_level (forest, initlevel);

  t8_forest_commit (forest);

  /* user-data (minlevel, maxlevel) */
  t8_example_level_set_struct_t     ls_data;
  t8_basic_sphere_data_t sdata;

  /* Midpoint and radius of a sphere */
  /* shift the midpoiunt of the circle by (shift_x,shift_y) to ensure midpoints on corners of the uniform mesh */
  sdata.mid_point[0] = 0;    // 1.0 / 2.0 + shift_x * 1.0/(1 << (minlevel));
  sdata.mid_point[1] = 0;    // 1.0 / 2.0 + shift_y * 1.0/(1 << (minlevel)); 
  sdata.mid_point[2] = 0;
  sdata.radius = 0.6;

  /* refinement parameter */
  ls_data.band_width = 1;
  ls_data.L = t8_basic_level_set_sphere;
  ls_data.min_level = minlevel;
  ls_data.max_level = maxlevel;
  ls_data.udata = &sdata;

  /* Adapt the mesh according to the user data */
  t8_forest_set_user_data (forest_adapt, &ls_data);
  t8_forest_set_adapt (forest_adapt, forest, t8_common_adapt_level_set, 1);

  if (do_balance) {
    t8_forest_set_balance (forest_adapt, forest, 0);
  }
  
  if (do_transition) {
    t8_forest_set_transition (forest_adapt, NULL);
    ghost_version = 1;
    t8_productionf ("Ghost version written to %d\n", ghost_version);
  }

  if (do_ghost) {
    /* set ghosts after adaptation/balancing/transitioning */
    t8_forest_set_ghost_ext (forest_adapt, do_ghost, T8_GHOST_FACES, ghost_version);
  }

  if (do_partition) {
    t8_forest_set_partition (forest_adapt, forest, 0);
  }

  t8_forest_commit (forest_adapt);

  if (do_vtk) {
    /* print to vtk */
    snprintf (filename, BUFSIZ, "forest_adapt_test_leaf_neighbor_%s",
              t8_eclass_to_string[eclass]);
    t8_forest_write_vtk (forest_adapt, filename);
  }

  if (do_LFN_test) {
    /* determine the neighbor element and printing the element data */
    t8_test_LFN (forest_adapt);
  }

  t8_forest_unref (&forest_adapt);
}

int
main (int argc, char **argv)
{
  int                 mpiret;

  mpiret = sc_MPI_Init (&argc, &argv);
  SC_CHECK_MPI (mpiret);

  sc_init (sc_MPI_COMM_WORLD, 1, 1, NULL, SC_LP_ESSENTIAL);
  t8_init (SC_LP_DEFAULT);

  /* At the moment, subelements are only implemented for T8_ECLASS_QUADS */
  t8_refine_transition (T8_ECLASS_QUAD);

  sc_finalize ();

  mpiret = sc_MPI_Finalize ();
  SC_CHECK_MPI (mpiret);

  return 0;
}
