/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#include <stdlib.h>
#include <cmath>
#include "atom_vec_CAC.h"
#include "atom.h"
#include "comm.h"
#include "force.h"
#include "domain.h"
#include "modify.h"
#include "fix.h"
#include "memory.h"
#include "error.h"
#include <string.h>

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

AtomVecCAC::AtomVecCAC(LAMMPS *lmp) : AtomVec(lmp)
{
  molecular = 0;
  mass_type = 1;
  element_type_count = 0;
  scale_count=0;
  comm_x_only = comm_f_only = 0;
  size_forward = 3;
  size_reverse = 3;
  size_border = 6;
  size_velocity = 3;
  size_data_atom = 5;
  size_data_vel = 4;
  xcol_data = 3;
  forceclearflag = 1;
  atom->CAC_flag=1;
 atom->oneflag=0;
  search_range_max = 0;
   initial_size=0;
 
}


/* ----------------------------------------------------------------------
   process user input
   n = 0 grows arrays by a chunk
   n > 0 allocates arrays to size n
------------------------------------------------------------------------- */

void AtomVecCAC::process_args(int narg, char **arg)
{
  if (narg < 1) error->all(FLERR,"Invalid atom_style CAC command");
  if (narg > 3) error->all(FLERR,"Invalid atom_style CAC command");

 nodes_per_element=force->numeric(FLERR,arg[0]);
 maxpoly = force->numeric(FLERR, arg[1]);
 atom->nodes_per_element=nodes_per_element;
 atom-> words_per_node = 6;
 atom->maxpoly = maxpoly;
 if(narg==3){
	 if (strcmp(arg[2], "one") == 0) atom->oneflag = 1;
	 else error->all(FLERR,"Invalid argument in atom_style CAC command");
 }


  size_forward = 12*nodes_per_element*maxpoly +8+ maxpoly;
  size_reverse = 3; // 3 + drho + de
  size_border = 12*nodes_per_element*maxpoly +11+ maxpoly;
  size_velocity = 12*nodes_per_element*maxpoly +11+ maxpoly;
  size_data_atom = 3*nodes_per_element*maxpoly +10+ maxpoly;
  size_data_vel = 12*nodes_per_element*maxpoly +9+ maxpoly;
  xcol_data = 3;

  comm->maxexchange_atom=size_border;
  
  if(element_type_count==0){
		element_type_count = 2; //increase if new types added
		 memory->grow(atom->nodes_per_element_list, element_type_count, "atom:nodes_per_element_list");
		//define number of nodes for existing element types
		atom->nodes_per_element_list[0] = 1;
		atom->nodes_per_element_list[1] = 8;
	}	
}

/* ----------------------------------------------------------------------
  initialize arrays
------------------------------------------------------------------------- */
void AtomVecCAC::init()
{
  	
    deform_vremap = domain->deform_vremap;
  	deform_groupbit = domain->deform_groupbit;
  	h_rate = domain->h_rate;

  	if (lmp->kokkos != NULL && !kokkosable)
    error->all(FLERR,"KOKKOS package requires a kokkos enabled atom_style");
    
	
}

/* ----------------------------------------------------------------------
   grow atom arrays
   n = 0 grows arrays by a chunk
   n > 0 allocates arrays to size n
------------------------------------------------------------------------- */



void AtomVecCAC::grow(int n)
{
  if (n == 0) grow_nmax();
  else nmax = n;
  atom->nmax = nmax;
  if (nmax < 0)
    error->one(FLERR,"Per-processor system is too big");

  tag = memory->grow(atom->tag,nmax,"atom:tag");
  type = memory->grow(atom->type,nmax,"atom:type");
  mask = memory->grow(atom->mask,nmax,"atom:mask");
  image = memory->grow(atom->image,nmax,"atom:image");
  x = memory->grow(atom->x,nmax,3,"atom:x");
  v = memory->grow(atom->v,nmax,3,"atom:v");
  f = memory->grow(atom->f, nmax*comm->nthreads, 3, "atom:f");
  poly_count = memory->grow(atom->poly_count, nmax, "atom:type_count");
  element_type= memory->grow(atom->element_type, nmax, "atom:element_type");
  element_scale = memory->grow(atom->element_scale, nmax,3, "atom:element_scales");
  node_types = memory->grow(atom->node_types, nmax, maxpoly, "atom:node_types");
  nodal_positions = memory->grow(atom->nodal_positions, nmax, nodes_per_element, maxpoly,3, "atom:nodal_positions");
  initial_nodal_positions = memory->grow(atom->initial_nodal_positions, nmax, nodes_per_element, maxpoly, 3, "atom:nodal_positions");
  nodal_velocities = memory->grow(atom->nodal_velocities, nmax, nodes_per_element, maxpoly, 3, "atom:nodal_velocities");
  nodal_forces = memory->grow(atom->nodal_forces, nmax, nodes_per_element, maxpoly, 3, "atom:nodal_forces");
  nodal_gradients = memory->grow(atom->nodal_gradients, nmax, nodes_per_element, maxpoly, 3, "atom:I_nodal_positions");
  if (atom->nextra_grow)
    for (int iextra = 0; iextra < atom->nextra_grow; iextra++)
      modify->fix[atom->extra_grow[iextra]]->grow_arrays(nmax);
}

/* ----------------------------------------------------------------------
   reset local array ptrs
------------------------------------------------------------------------- */

void AtomVecCAC::grow_reset()
{
  tag = atom->tag; type = atom->type;
  mask = atom->mask; image = atom->image;
  x = atom->x; v = atom->v; f = atom->f;
    nodal_positions = atom->nodal_positions;
	initial_nodal_positions = atom->initial_nodal_positions;
  nodal_velocities = atom->nodal_velocities;
  nodal_forces = atom->nodal_forces;
  nodal_gradients =atom->nodal_gradients;
  poly_count = atom->poly_count;
  element_type = atom->element_type;
  element_scale = atom->element_scale;
  node_types = atom->node_types;

}

/* ----------------------------------------------------------------------
   copy atom I info to atom J
------------------------------------------------------------------------- */

void AtomVecCAC::copy(int i, int j, int delflag)
{
  int *nodes_count_list = atom->nodes_per_element_list;	
  tag[j] = tag[i];
  type[j] = type[i];
  mask[j] = mask[i];
  image[j] = image[i];
  x[j][0] = x[i][0];
  x[j][1] = x[i][1];
  x[j][2] = x[i][2];
  v[j][0] = v[i][0];
  v[j][1] = v[i][1];
  v[j][2] = v[i][2];
  element_type[j] = element_type[i];
  element_scale[j][0] = element_scale[i][0];
  element_scale[j][1] = element_scale[i][1];
  element_scale[j][2] = element_scale[i][2];
  poly_count[j] = poly_count[i];
  for (int type_map = 0; type_map < poly_count[j]; type_map++) {
	  node_types[j][type_map] = node_types[i][type_map];
  }

  for(int nodecount=0; nodecount< nodes_count_list[element_type[j]]; nodecount++ ){
	  for (int poly_index = 0; poly_index < poly_count[j]; poly_index++)
		  {
			  nodal_positions[j][nodecount][poly_index][0] = nodal_positions[i][nodecount][poly_index][0];
			  nodal_positions[j][nodecount][poly_index][1] = nodal_positions[i][nodecount][poly_index][1];
			  nodal_positions[j][nodecount][poly_index][2] = nodal_positions[i][nodecount][poly_index][2];
			  initial_nodal_positions[j][nodecount][poly_index][0] = initial_nodal_positions[i][nodecount][poly_index][0];
			  initial_nodal_positions[j][nodecount][poly_index][1] = initial_nodal_positions[i][nodecount][poly_index][1];
			  initial_nodal_positions[j][nodecount][poly_index][2] = initial_nodal_positions[i][nodecount][poly_index][2];
			  nodal_gradients[j][nodecount][poly_index][0] = nodal_gradients[i][nodecount][poly_index][0];
			  nodal_gradients[j][nodecount][poly_index][1] = nodal_gradients[i][nodecount][poly_index][1];
			  nodal_gradients[j][nodecount][poly_index][2] = nodal_gradients[i][nodecount][poly_index][2];
			  nodal_velocities[j][nodecount][poly_index][0] = nodal_velocities[i][nodecount][poly_index][0];
			  nodal_velocities[j][nodecount][poly_index][1] = nodal_velocities[i][nodecount][poly_index][1];
			  nodal_velocities[j][nodecount][poly_index][2] = nodal_velocities[i][nodecount][poly_index][2];
		  }
	  }

  if (atom->nextra_grow)
    for (int iextra = 0; iextra < atom->nextra_grow; iextra++)
      modify->fix[atom->extra_grow[iextra]]->copy_arrays(i,j,delflag);
}

/* ---------------------------------------------------------------------- */

int AtomVecCAC::pack_comm(int n, int *list, double *buf,
                             int pbc_flag, int *pbc)
{
  int i,j,m;
  double dx,dy,dz;
  int *nodes_count_list = atom->nodes_per_element_list;
  m = 0;
  if (pbc_flag == 0) {
    for (i = 0; i < n; i++) {
      j = list[i];
      buf[m++] = x[j][0];
      buf[m++] = x[j][1];
      buf[m++] = x[j][2];

	  buf[m++] = element_type[j];

	  buf[m++] = element_scale[j][0];
	  buf[m++] = element_scale[j][1];
	  buf[m++] = element_scale[j][2];

	  buf[m++] = poly_count[j];
	  for (int type_map = 0; type_map < poly_count[j]; type_map++) {
		  buf[m++] = node_types[j][type_map];
	  }

	  for (int nodecount = 0; nodecount< nodes_count_list[element_type[j]]; nodecount++){
		  for (int poly_index = 0; poly_index < poly_count[j]; poly_index++)
			  {
				  buf[m++] = nodal_positions[j][nodecount][poly_index][0];
				  buf[m++] = nodal_positions[j][nodecount][poly_index][1];
				  buf[m++] = nodal_positions[j][nodecount][poly_index][2];
				  buf[m++] = initial_nodal_positions[j][nodecount][poly_index][0];
				  buf[m++] = initial_nodal_positions[j][nodecount][poly_index][1];
				  buf[m++] = initial_nodal_positions[j][nodecount][poly_index][2];
				  buf[m++] = nodal_gradients[j][nodecount][poly_index][0];
				  buf[m++] = nodal_gradients[j][nodecount][poly_index][1];
				  buf[m++] = nodal_gradients[j][nodecount][poly_index][2];
				  buf[m++] = nodal_velocities[j][nodecount][poly_index][0];
				  buf[m++] = nodal_velocities[j][nodecount][poly_index][1];
				  buf[m++] = nodal_velocities[j][nodecount][poly_index][2];
			  }
		  }
    }
  } else {
    if (domain->triclinic == 0) {
      dx = pbc[0]*domain->xprd;
      dy = pbc[1]*domain->yprd;
      dz = pbc[2]*domain->zprd;
    } else {
      dx = pbc[0]*domain->xprd + pbc[5]*domain->xy + pbc[4]*domain->xz;
      dy = pbc[1]*domain->yprd + pbc[3]*domain->yz;
      dz = pbc[2]*domain->zprd;
    }
    for (i = 0; i < n; i++) {
      j = list[i];
      buf[m++] = x[j][0] + dx;
      buf[m++] = x[j][1] + dy;
      buf[m++] = x[j][2] + dz;

	  buf[m++] = element_type[j];
	  buf[m++] = element_scale[j][0];
	  buf[m++] = element_scale[j][1];
	  buf[m++] = element_scale[j][2];
	  buf[m++] = poly_count[j];
	  for (int type_map = 0; type_map < poly_count[j]; type_map++) {
		  buf[m++] = node_types[j][type_map];
	  }

	  for (int nodecount = 0; nodecount< nodes_count_list[element_type[j]]; nodecount++) {
		  for (int poly_index = 0; poly_index < poly_count[j]; poly_index++)
		  {
			  buf[m++] = nodal_positions[j][nodecount][poly_index][0]+dx;
			  buf[m++] = nodal_positions[j][nodecount][poly_index][1] + dy;
			  buf[m++] = nodal_positions[j][nodecount][poly_index][2] + dz;
			  buf[m++] = initial_nodal_positions[j][nodecount][poly_index][0] + dx;
			  buf[m++] = initial_nodal_positions[j][nodecount][poly_index][1] + dy;
			  buf[m++] = initial_nodal_positions[j][nodecount][poly_index][2] + dz;
			  buf[m++] = nodal_gradients[j][nodecount][poly_index][0];
			  buf[m++] = nodal_gradients[j][nodecount][poly_index][1];
			  buf[m++] = nodal_gradients[j][nodecount][poly_index][2];
			  buf[m++] = nodal_velocities[j][nodecount][poly_index][0];
			  buf[m++] = nodal_velocities[j][nodecount][poly_index][1];
			  buf[m++] = nodal_velocities[j][nodecount][poly_index][2];
		  }
	  }

    }
  }
  return m;
}

/* ---------------------------------------------------------------------- */

int AtomVecCAC::pack_comm_vel(int n, int *list, double *buf,
                                 int pbc_flag, int *pbc)
{
  int i,j,m;
  double dx,dy,dz,dvx,dvy,dvz;
 	int *nodes_count_list = atom->nodes_per_element_list;
  m = 0;
  if (pbc_flag == 0) {
	  for (i = 0; i < n; i++) {
		  j = list[i];
		  buf[m++] = x[j][0];
		  buf[m++] = x[j][1];
		  buf[m++] = x[j][2];
		  buf[m++] = v[j][0];
		  buf[m++] = v[j][1];
		  buf[m++] = v[j][2];
		  buf[m++] = element_type[j];
		  buf[m++] = element_scale[j][0];
		  buf[m++] = element_scale[j][1];
		  buf[m++] = element_scale[j][2];
		  buf[m++] = poly_count[j];
		  for (int type_map = 0; type_map < poly_count[j]; type_map++) {
			  buf[m++] = node_types[j][type_map];
		  }

		  for (int nodecount = 0; nodecount < nodes_count_list[element_type[j]]; nodecount++) {
			  for (int poly_index = 0; poly_index < poly_count[j]; poly_index++)
			  {
				  buf[m++] = nodal_positions[j][nodecount][poly_index][0];
				  buf[m++] = nodal_positions[j][nodecount][poly_index][1];
				  buf[m++] = nodal_positions[j][nodecount][poly_index][2];
				  buf[m++] = initial_nodal_positions[j][nodecount][poly_index][0];
				  buf[m++] = initial_nodal_positions[j][nodecount][poly_index][1];
				  buf[m++] = initial_nodal_positions[j][nodecount][poly_index][2];
				  buf[m++] = nodal_gradients[j][nodecount][poly_index][0];
				  buf[m++] = nodal_gradients[j][nodecount][poly_index][1];
				  buf[m++] = nodal_gradients[j][nodecount][poly_index][2];
				  buf[m++] = nodal_velocities[j][nodecount][poly_index][0];
				  buf[m++] = nodal_velocities[j][nodecount][poly_index][1];
				  buf[m++] = nodal_velocities[j][nodecount][poly_index][2];
			  }
		  }
	  }
  }
  else {
    if (domain->triclinic == 0) {
      dx = pbc[0]*domain->xprd;
      dy = pbc[1]*domain->yprd;
      dz = pbc[2]*domain->zprd;
    } else {
      dx = pbc[0]*domain->xprd + pbc[5]*domain->xy + pbc[4]*domain->xz;
      dy = pbc[1]*domain->yprd + pbc[3]*domain->yz;
      dz = pbc[2]*domain->zprd;
    }
    if (!deform_vremap) {
      for (i = 0; i < n; i++) {
        j = list[i];
        buf[m++] = x[j][0] + dx;
        buf[m++] = x[j][1] + dy;
        buf[m++] = x[j][2] + dz;
        buf[m++] = v[j][0];
        buf[m++] = v[j][1];
        buf[m++] = v[j][2];
		buf[m++] = element_type[j];
		buf[m++] = element_scale[j][0];
		buf[m++] = element_scale[j][1];
		buf[m++] = element_scale[j][2];
		buf[m++] = poly_count[j];
		for (int type_map = 0; type_map < poly_count[j]; type_map++) {
			buf[m++] = node_types[j][type_map];
		}

		for (int nodecount = 0; nodecount< nodes_count_list[element_type[j]]; nodecount++) {
			for (int poly_index = 0; poly_index < poly_count[j]; poly_index++)
			{
				buf[m++] = nodal_positions[j][nodecount][poly_index][0] + dx;
				buf[m++] = nodal_positions[j][nodecount][poly_index][1] + dy;
				buf[m++] = nodal_positions[j][nodecount][poly_index][2] + dz;
				buf[m++] = initial_nodal_positions[j][nodecount][poly_index][0] + dx;
				buf[m++] = initial_nodal_positions[j][nodecount][poly_index][1] + dy;
				buf[m++] = initial_nodal_positions[j][nodecount][poly_index][2] + dz;
				buf[m++] = nodal_gradients[j][nodecount][poly_index][0];
				buf[m++] = nodal_gradients[j][nodecount][poly_index][1];
				buf[m++] = nodal_gradients[j][nodecount][poly_index][2];
				buf[m++] = nodal_velocities[j][nodecount][poly_index][0];
				buf[m++] = nodal_velocities[j][nodecount][poly_index][1];
				buf[m++] = nodal_velocities[j][nodecount][poly_index][2];
			}
		}
      }
    } else {
      dvx = pbc[0]*h_rate[0] + pbc[5]*h_rate[5] + pbc[4]*h_rate[4];
      dvy = pbc[1]*h_rate[1] + pbc[3]*h_rate[3];
      dvz = pbc[2]*h_rate[2];
      for (i = 0; i < n; i++) {
        j = list[i];
        buf[m++] = x[j][0] + dx;
        buf[m++] = x[j][1] + dy;
        buf[m++] = x[j][2] + dz;

        if (mask[i] & deform_groupbit) {
          buf[m++] = v[j][0] + dvx;
          buf[m++] = v[j][1] + dvy;
          buf[m++] = v[j][2] + dvz;
		  buf[m++] = element_type[j];
		  buf[m++] = element_scale[j][0];
		  buf[m++] = element_scale[j][1];
		  buf[m++] = element_scale[j][2];
		  buf[m++] = poly_count[j];
		  for (int type_map = 0; type_map < poly_count[j]; type_map++) {
			  buf[m++] = node_types[j][type_map];
		  }

		  for (int nodecount = 0; nodecount< nodes_count_list[element_type[j]]; nodecount++) {
			  for (int poly_index = 0; poly_index < poly_count[j]; poly_index++)
			  {
				  buf[m++] = nodal_positions[j][nodecount][poly_index][0] + dx;
				  buf[m++] = nodal_positions[j][nodecount][poly_index][1] + dy;
				  buf[m++] = nodal_positions[j][nodecount][poly_index][2] + dz;
				  buf[m++] = initial_nodal_positions[j][nodecount][poly_index][0] + dx;
				  buf[m++] = initial_nodal_positions[j][nodecount][poly_index][1] + dy;
				  buf[m++] = initial_nodal_positions[j][nodecount][poly_index][2] + dz;
				  buf[m++] = nodal_gradients[j][nodecount][poly_index][0];
				  buf[m++] = nodal_gradients[j][nodecount][poly_index][1];
				  buf[m++] = nodal_gradients[j][nodecount][poly_index][2];
				  buf[m++] = nodal_velocities[j][nodecount][poly_index][0] + dvx;
				  buf[m++] = nodal_velocities[j][nodecount][poly_index][1] + dvy;
				  buf[m++] = nodal_velocities[j][nodecount][poly_index][2] + dvz;
			  }
		  }
        } else {
          buf[m++] = v[j][0];
          buf[m++] = v[j][1];
          buf[m++] = v[j][2];
		  buf[m++] = element_type[j];
		  buf[m++] = element_scale[j][0];
		  buf[m++] = element_scale[j][1];
		  buf[m++] = element_scale[j][2];
		  buf[m++] = poly_count[j];
		  for (int type_map = 0; type_map < maxpoly; type_map++) {
			  buf[m++] = node_types[j][type_map];
		  }

		  for (int nodecount = 0; nodecount< nodes_count_list[element_type[j]]; nodecount++) {
			  for (int poly_index = 0; poly_index < poly_count[j]; poly_index++)
			  {
				  buf[m++] = nodal_positions[j][nodecount][poly_index][0] + dx;
				  buf[m++] = nodal_positions[j][nodecount][poly_index][1] + dy;
				  buf[m++] = nodal_positions[j][nodecount][poly_index][2] + dz;
				  buf[m++] = initial_nodal_positions[j][nodecount][poly_index][0] + dx;
				  buf[m++] = initial_nodal_positions[j][nodecount][poly_index][1] + dy;
				  buf[m++] = initial_nodal_positions[j][nodecount][poly_index][2] + dz;
				  buf[m++] = nodal_gradients[j][nodecount][poly_index][0];
				  buf[m++] = nodal_gradients[j][nodecount][poly_index][1];
				  buf[m++] = nodal_gradients[j][nodecount][poly_index][2];
				  buf[m++] = nodal_velocities[j][nodecount][poly_index][0];
				  buf[m++] = nodal_velocities[j][nodecount][poly_index][1];
				  buf[m++] = nodal_velocities[j][nodecount][poly_index][2];
			  }
		  }
        }
      }
    }
  }
  return m;
}

/* ---------------------------------------------------------------------- */

void AtomVecCAC::unpack_comm(int n, int first, double *buf)
{
  int i,m,last;
  int *nodes_count_list = atom->nodes_per_element_list;
  m = 0;
  last = first + n;
  for (i = first; i < last; i++) {
    x[i][0] = buf[m++];
    x[i][1] = buf[m++];
    x[i][2] = buf[m++];
	 element_type[i]=buf[m++];
	 element_scale[i][0] = buf[m++];
	 element_scale[i][1] = buf[m++];
	 element_scale[i][2] = buf[m++];
	 poly_count[i] = buf[m++];
	for (int type_map = 0; type_map < poly_count[i]; type_map++) {
		 node_types[i][type_map]= buf[m++];
	}
	for (int nodecount = 0; nodecount < nodes_count_list[element_type[i]]; nodecount++) {
		for (int poly_index = 0; poly_index < poly_count[i]; poly_index++)
		{
			nodal_positions[i][nodecount][poly_index][0] = buf[m++];
			nodal_positions[i][nodecount][poly_index][1] = buf[m++];
			nodal_positions[i][nodecount][poly_index][2] = buf[m++];
			initial_nodal_positions[i][nodecount][poly_index][0] = buf[m++];
			initial_nodal_positions[i][nodecount][poly_index][1] = buf[m++];
			initial_nodal_positions[i][nodecount][poly_index][2] = buf[m++];
			nodal_gradients[i][nodecount][poly_index][0] = buf[m++];
			nodal_gradients[i][nodecount][poly_index][1] = buf[m++];
			nodal_gradients[i][nodecount][poly_index][2] = buf[m++];
			nodal_velocities[i][nodecount][poly_index][0] = buf[m++];
			nodal_velocities[i][nodecount][poly_index][1] = buf[m++];
			nodal_velocities[i][nodecount][poly_index][2] = buf[m++];
		}
	}
  }
}

/* ---------------------------------------------------------------------- */

void AtomVecCAC::unpack_comm_vel(int n, int first, double *buf)
{
  int i,m,last;
  int *nodes_count_list = atom->nodes_per_element_list;
  m = 0;
  last = first + n;
  for (i = first; i < last; i++) {
    x[i][0] = buf[m++];
    x[i][1] = buf[m++];
    x[i][2] = buf[m++];
    v[i][0] = buf[m++];
    v[i][1] = buf[m++];
    v[i][2] = buf[m++];
	element_type[i] = buf[m++];
	element_scale[i][0] = buf[m++];
	element_scale[i][1] = buf[m++];
	element_scale[i][2] = buf[m++];
	poly_count[i] = buf[m++];
	for (int type_map = 0; type_map < poly_count[i]; type_map++) {
		node_types[i][type_map] = buf[m++];
	}
	for (int nodecount = 0; nodecount < nodes_count_list[element_type[i]]; nodecount++) {
		for (int poly_index = 0; poly_index < poly_count[i]; poly_index++)
		{
			nodal_positions[i][nodecount][poly_index][0] = buf[m++];
			nodal_positions[i][nodecount][poly_index][1] = buf[m++];
			nodal_positions[i][nodecount][poly_index][2] = buf[m++];
			initial_nodal_positions[i][nodecount][poly_index][0] = buf[m++];
			initial_nodal_positions[i][nodecount][poly_index][1] = buf[m++];
			initial_nodal_positions[i][nodecount][poly_index][2] = buf[m++];
			nodal_gradients[i][nodecount][poly_index][0] = buf[m++];
			nodal_gradients[i][nodecount][poly_index][1] = buf[m++];
			nodal_gradients[i][nodecount][poly_index][2] = buf[m++];
			nodal_velocities[i][nodecount][poly_index][0] = buf[m++];
			nodal_velocities[i][nodecount][poly_index][1] = buf[m++];
			nodal_velocities[i][nodecount][poly_index][2] = buf[m++];
		}
	}
  }
}

/* ---------------------------------------------------------------------- */

int AtomVecCAC::pack_reverse(int n, int first, double *buf)
{
  int i,m,last;

  m = 0;
  last = first + n;
  for (i = first; i < last; i++) {
    buf[m++] = f[i][0];
    buf[m++] = f[i][1];
    buf[m++] = f[i][2];
  }
  return m;
}

/* ---------------------------------------------------------------------- */

void AtomVecCAC::unpack_reverse(int n, int *list, double *buf)
{
  int i,j,m;

  m = 0;
  for (i = 0; i < n; i++) {
    j = list[i];
    f[j][0] += buf[m++];
    f[j][1] += buf[m++];
    f[j][2] += buf[m++];
  }
}

/* ---------------------------------------------------------------------- */

int AtomVecCAC::pack_border(int n, int *list, double *buf,
                               int pbc_flag, int *pbc)
{
  int i,j,m;
  double dx,dy,dz;
  double lamda_temp[3];
  double nodal_temp[3];
  int *nodes_count_list = atom->nodes_per_element_list;
  m = 0;
  if (pbc_flag == 0) {
    for (i = 0; i < n; i++) {
      j = list[i];
      buf[m++] = x[j][0];
      buf[m++] = x[j][1];
      buf[m++] = x[j][2];
      buf[m++] = ubuf(tag[j]).d;
      buf[m++] = ubuf(type[j]).d;
      buf[m++] = ubuf(mask[j]).d;
	  buf[m++] = element_type[j];
	  buf[m++] = element_scale[j][0];
	  buf[m++] = element_scale[j][1];
	  buf[m++] = element_scale[j][2];
	  buf[m++] = poly_count[j];
	  for (int type_map = 0; type_map < poly_count[j]; type_map++) {
		  buf[m++] = node_types[j][type_map];
	  }

	  for (int nodecount = 0; nodecount< nodes_count_list[element_type[j]]; nodecount++) {
		  for (int poly_index = 0; poly_index < poly_count[j]; poly_index++)
		  {
			  buf[m++] = nodal_positions[j][nodecount][poly_index][0];
			  buf[m++] = nodal_positions[j][nodecount][poly_index][1];
			  buf[m++] = nodal_positions[j][nodecount][poly_index][2];
			  buf[m++] = initial_nodal_positions[j][nodecount][poly_index][0];
			  buf[m++] = initial_nodal_positions[j][nodecount][poly_index][1];
			  buf[m++] = initial_nodal_positions[j][nodecount][poly_index][2];
			  buf[m++] = nodal_gradients[j][nodecount][poly_index][0];
			  buf[m++] = nodal_gradients[j][nodecount][poly_index][1];
			  buf[m++] = nodal_gradients[j][nodecount][poly_index][2];
			  buf[m++] = nodal_velocities[j][nodecount][poly_index][0];
			  buf[m++] = nodal_velocities[j][nodecount][poly_index][1];
			  buf[m++] = nodal_velocities[j][nodecount][poly_index][2];
		  }
	  }
    }
  } else {
    if (domain->triclinic == 0) {
      dx = pbc[0]*domain->xprd;
      dy = pbc[1]*domain->yprd;
      dz = pbc[2]*domain->zprd;
    } else {
      dx = pbc[0];
      dy = pbc[1];
      dz = pbc[2];
    }
    for (i = 0; i < n; i++) {
      j = list[i];
      buf[m++] = x[j][0] + dx;
      buf[m++] = x[j][1] + dy;
      buf[m++] = x[j][2] + dz;
      buf[m++] = ubuf(tag[j]).d;
      buf[m++] = ubuf(type[j]).d;
      buf[m++] = ubuf(mask[j]).d;
	  buf[m++] = element_type[j];
	  buf[m++] = element_scale[j][0];
	  buf[m++] = element_scale[j][1];
	  buf[m++] = element_scale[j][2];
	  buf[m++] = poly_count[j];
	  for (int type_map = 0; type_map < poly_count[j]; type_map++) {
		  buf[m++] = node_types[j][type_map];
	  }

	  for (int nodecount = 0; nodecount< nodes_count_list[element_type[j]]; nodecount++) {
		  for (int poly_index = 0; poly_index < poly_count[j]; poly_index++)
		  {
			  nodal_temp[0] = nodal_positions[j][nodecount][poly_index][0];
			  nodal_temp[1] = nodal_positions[j][nodecount][poly_index][1];
			  nodal_temp[2] = nodal_positions[j][nodecount][poly_index][2];
			  if (domain->triclinic != 0) {
				  domain->x2lamda(nodal_temp, lamda_temp);
				  lamda_temp[0] += dx;
				  lamda_temp[1] += dy;
				  lamda_temp[2] += dz;
				  domain->lamda2x(lamda_temp, nodal_temp);
				  buf[m++] = nodal_temp[0];
				  buf[m++] = nodal_temp[1];
				  buf[m++] = nodal_temp[2];
			  }
			  else {
				  buf[m++] = nodal_temp[0]+dx;
				  buf[m++] = nodal_temp[1]+dy;
				  buf[m++] = nodal_temp[2]+dz;
			  }

			  nodal_temp[0] = initial_nodal_positions[j][nodecount][poly_index][0];
			  nodal_temp[1] = initial_nodal_positions[j][nodecount][poly_index][1];
			  nodal_temp[2] = initial_nodal_positions[j][nodecount][poly_index][2];
			  if (domain->triclinic != 0) {
				  domain->x2lamda(nodal_temp, lamda_temp);
				  lamda_temp[0] += dx;
				  lamda_temp[1] += dy;
				  lamda_temp[2] += dz;
				  domain->lamda2x(lamda_temp, nodal_temp);
				  buf[m++] = nodal_temp[0];
				  buf[m++] = nodal_temp[1];
				  buf[m++] = nodal_temp[2];
			  }
			  else {
				  buf[m++] = nodal_temp[0] + dx;
				  buf[m++] = nodal_temp[1] + dy;
				  buf[m++] = nodal_temp[2] + dz;
			  }
			  buf[m++] = nodal_gradients[j][nodecount][poly_index][0];
			  buf[m++] = nodal_gradients[j][nodecount][poly_index][1];
			  buf[m++] = nodal_gradients[j][nodecount][poly_index][2];
			  buf[m++] = nodal_velocities[j][nodecount][poly_index][0];
			  buf[m++] = nodal_velocities[j][nodecount][poly_index][1];
			  buf[m++] = nodal_velocities[j][nodecount][poly_index][2];
		  }
	  }
    }
  }

  if (atom->nextra_border)
    for (int iextra = 0; iextra < atom->nextra_border; iextra++)
      m += modify->fix[atom->extra_border[iextra]]->pack_border(n,list,&buf[m]);

  return m;
}

/* ---------------------------------------------------------------------- */

int AtomVecCAC::pack_border_vel(int n, int *list, double *buf,
                                   int pbc_flag, int *pbc)
{
  int i,j,m;
  double dx,dy,dz,dvx,dvy,dvz;
  double lamda_temp[3];
  double nodal_temp[3];
  int *nodes_count_list = atom->nodes_per_element_list;
  m = 0;
  if (pbc_flag == 0) {
    for (i = 0; i < n; i++) {
      j = list[i];
      buf[m++] = x[j][0];
      buf[m++] = x[j][1];
      buf[m++] = x[j][2];
      buf[m++] = ubuf(tag[j]).d;
      buf[m++] = ubuf(type[j]).d;
      buf[m++] = ubuf(mask[j]).d;
      buf[m++] = v[j][0];
      buf[m++] = v[j][1];
      buf[m++] = v[j][2];
	  buf[m++] = element_type[j];
	  buf[m++] = element_scale[j][0];
	  buf[m++] = element_scale[j][1];
	  buf[m++] = element_scale[j][2];
	  buf[m++] = poly_count[j];
	  for (int type_map = 0; type_map < maxpoly; type_map++) {
		  buf[m++] = node_types[j][type_map];
	  }

	  for (int nodecount = 0; nodecount< nodes_count_list[element_type[j]]; nodecount++) {
		  for (int poly_index = 0; poly_index < poly_count[j]; poly_index++)
		  {
			  buf[m++] = nodal_positions[j][nodecount][poly_index][0];
			  buf[m++] = nodal_positions[j][nodecount][poly_index][1];
			  buf[m++] = nodal_positions[j][nodecount][poly_index][2];
			  buf[m++] = initial_nodal_positions[j][nodecount][poly_index][0];
			  buf[m++] = initial_nodal_positions[j][nodecount][poly_index][1];
			  buf[m++] = initial_nodal_positions[j][nodecount][poly_index][2];
			  buf[m++] = nodal_gradients[j][nodecount][poly_index][0];
			  buf[m++] = nodal_gradients[j][nodecount][poly_index][1];
			  buf[m++] = nodal_gradients[j][nodecount][poly_index][2];
			  buf[m++] = nodal_velocities[j][nodecount][poly_index][0];
			  buf[m++] = nodal_velocities[j][nodecount][poly_index][1];
			  buf[m++] = nodal_velocities[j][nodecount][poly_index][2];
		  }
	  }
    }
  } else {
    if (domain->triclinic == 0) {
      dx = pbc[0]*domain->xprd;
      dy = pbc[1]*domain->yprd;
      dz = pbc[2]*domain->zprd;
    } else {
      dx = pbc[0];
      dy = pbc[1];
      dz = pbc[2];
    }
    if (!deform_vremap) {
      for (i = 0; i < n; i++) {
        j = list[i];
        buf[m++] = x[j][0] + dx;
        buf[m++] = x[j][1] + dy;
        buf[m++] = x[j][2] + dz;
        buf[m++] = ubuf(tag[j]).d;
        buf[m++] = ubuf(type[j]).d;
        buf[m++] = ubuf(mask[j]).d;
        buf[m++] = v[j][0];
        buf[m++] = v[j][1];
        buf[m++] = v[j][2];
		buf[m++] = element_type[j];
		buf[m++] = element_scale[j][0];
		buf[m++] = element_scale[j][1];
		buf[m++] = element_scale[j][2];
		buf[m++] = poly_count[j];
		for (int type_map = 0; type_map < poly_count[j]; type_map++) {
			buf[m++] = node_types[j][type_map];
		}

		for (int nodecount = 0; nodecount< nodes_count_list[element_type[j]]; nodecount++) {
			for (int poly_index = 0; poly_index < poly_count[j]; poly_index++)
			{
				nodal_temp[0] = nodal_positions[j][nodecount][poly_index][0];
				nodal_temp[1] = nodal_positions[j][nodecount][poly_index][1];
				nodal_temp[2] = nodal_positions[j][nodecount][poly_index][2];
				if (domain->triclinic != 0) {
					domain->x2lamda(nodal_temp, lamda_temp);
					lamda_temp[0] += dx;
					lamda_temp[1] += dy;
					lamda_temp[2] += dz;
					domain->lamda2x(lamda_temp, nodal_temp);
					buf[m++] = nodal_temp[0];
					buf[m++] = nodal_temp[1];
					buf[m++] = nodal_temp[2];
				}
				else {
					buf[m++] = nodal_temp[0] + dx;
					buf[m++] = nodal_temp[1] + dy;
					buf[m++] = nodal_temp[2] + dz;
				}

				nodal_temp[0] = initial_nodal_positions[j][nodecount][poly_index][0];
				nodal_temp[1] = initial_nodal_positions[j][nodecount][poly_index][1];
				nodal_temp[2] = initial_nodal_positions[j][nodecount][poly_index][2];
				if (domain->triclinic != 0) {
					domain->x2lamda(nodal_temp, lamda_temp);
					lamda_temp[0] += dx;
					lamda_temp[1] += dy;
					lamda_temp[2] += dz;
					domain->lamda2x(lamda_temp, nodal_temp);
					buf[m++] = nodal_temp[0];
					buf[m++] = nodal_temp[1];
					buf[m++] = nodal_temp[2];
				}
				else {
					buf[m++] = nodal_temp[0] + dx;
					buf[m++] = nodal_temp[1] + dy;
					buf[m++] = nodal_temp[2] + dz;
				}
				buf[m++] = nodal_gradients[j][nodecount][poly_index][0];
				buf[m++] = nodal_gradients[j][nodecount][poly_index][1];
				buf[m++] = nodal_gradients[j][nodecount][poly_index][2];
				buf[m++] = nodal_velocities[j][nodecount][poly_index][0];
				buf[m++] = nodal_velocities[j][nodecount][poly_index][1];
				buf[m++] = nodal_velocities[j][nodecount][poly_index][2];
			}
		}
      }
    } else {
      dvx = pbc[0]*h_rate[0] + pbc[5]*h_rate[5] + pbc[4]*h_rate[4];
      dvy = pbc[1]*h_rate[1] + pbc[3]*h_rate[3];
      dvz = pbc[2]*h_rate[2];
      for (i = 0; i < n; i++) {
        j = list[i];
        buf[m++] = x[j][0] + dx;
        buf[m++] = x[j][1] + dy;
        buf[m++] = x[j][2] + dz;
        buf[m++] = ubuf(tag[j]).d;
        buf[m++] = ubuf(type[j]).d;
        buf[m++] = ubuf(mask[j]).d;
        if (mask[i] & deform_groupbit) {
          buf[m++] = v[j][0] + dvx;
          buf[m++] = v[j][1] + dvy;
          buf[m++] = v[j][2] + dvz;
		  buf[m++] = element_type[j];
		  buf[m++] = element_scale[j][0];
		  buf[m++] = element_scale[j][1];
		  buf[m++] = element_scale[j][2];
		  buf[m++] = poly_count[j];
		  for (int type_map = 0; type_map < poly_count[j]; type_map++) {
			  buf[m++] = node_types[j][type_map];
		  }

		  for (int nodecount = 0; nodecount< nodes_count_list[element_type[j]]; nodecount++) {
			  for (int poly_index = 0; poly_index < poly_count[j]; poly_index++)
			  {
				  nodal_temp[0] = nodal_positions[j][nodecount][poly_index][0];
				  nodal_temp[1] = nodal_positions[j][nodecount][poly_index][1];
				  nodal_temp[2] = nodal_positions[j][nodecount][poly_index][2];
				  if (domain->triclinic != 0) {
					  domain->x2lamda(nodal_temp, lamda_temp);
					  lamda_temp[0] += dx;
					  lamda_temp[1] += dy;
					  lamda_temp[2] += dz;
					  domain->lamda2x(lamda_temp, nodal_temp);
					  buf[m++] = nodal_temp[0];
					  buf[m++] = nodal_temp[1];
					  buf[m++] = nodal_temp[2];
				  }
				  else {
					  buf[m++] = nodal_temp[0] + dx;
					  buf[m++] = nodal_temp[1] + dy;
					  buf[m++] = nodal_temp[2] + dz;
				  }

				  nodal_temp[0] = initial_nodal_positions[j][nodecount][poly_index][0];
				  nodal_temp[1] = initial_nodal_positions[j][nodecount][poly_index][1];
				  nodal_temp[2] = initial_nodal_positions[j][nodecount][poly_index][2];
				  if (domain->triclinic != 0) {
					  domain->x2lamda(nodal_temp, lamda_temp);
					  lamda_temp[0] += dx;
					  lamda_temp[1] += dy;
					  lamda_temp[2] += dz;
					  domain->lamda2x(lamda_temp, nodal_temp);
					  buf[m++] = nodal_temp[0];
					  buf[m++] = nodal_temp[1];
					  buf[m++] = nodal_temp[2];
				  }
				  else {
					  buf[m++] = nodal_temp[0] + dx;
					  buf[m++] = nodal_temp[1] + dy;
					  buf[m++] = nodal_temp[2] + dz;
				  }
				  buf[m++] = nodal_gradients[j][nodecount][poly_index][0];
				  buf[m++] = nodal_gradients[j][nodecount][poly_index][1];
				  buf[m++] = nodal_gradients[j][nodecount][poly_index][2];
				  buf[m++] = nodal_velocities[j][nodecount][poly_index][0] + dvx;
				  buf[m++] = nodal_velocities[j][nodecount][poly_index][1] + dvy;
				  buf[m++] = nodal_velocities[j][nodecount][poly_index][2] + dvz;
			  }
		  }
        } else {
          buf[m++] = v[j][0];
          buf[m++] = v[j][1];
          buf[m++] = v[j][2];
		  buf[m++] = element_type[j];
		  buf[m++] = element_scale[j][0];
		  buf[m++] = element_scale[j][1];
		  buf[m++] = element_scale[j][2];
		  buf[m++] = poly_count[j];
		  for (int type_map = 0; type_map < poly_count[j]; type_map++) {
			  buf[m++] = node_types[j][type_map];
		  }

		  for (int nodecount = 0; nodecount< nodes_count_list[element_type[j]]; nodecount++) {
			  for (int poly_index = 0; poly_index < poly_count[j]; poly_index++)
			  {
				  nodal_temp[0] = nodal_positions[j][nodecount][poly_index][0];
				  nodal_temp[1] = nodal_positions[j][nodecount][poly_index][1];
				  nodal_temp[2] = nodal_positions[j][nodecount][poly_index][2];
				  if (domain->triclinic != 0) {
					  domain->x2lamda(nodal_temp, lamda_temp);
					  lamda_temp[0] += dx;
					  lamda_temp[1] += dy;
					  lamda_temp[2] += dz;
					  domain->lamda2x(lamda_temp, nodal_temp);
					  buf[m++] = nodal_temp[0];
					  buf[m++] = nodal_temp[1];
					  buf[m++] = nodal_temp[2];
				  }
				  else {
					  buf[m++] = nodal_temp[0] + dx;
					  buf[m++] = nodal_temp[1] + dy;
					  buf[m++] = nodal_temp[2] + dz;
				  }

				  nodal_temp[0] = initial_nodal_positions[j][nodecount][poly_index][0];
				  nodal_temp[1] = initial_nodal_positions[j][nodecount][poly_index][1];
				  nodal_temp[2] = initial_nodal_positions[j][nodecount][poly_index][2];
				  if (domain->triclinic != 0) {
					  domain->x2lamda(nodal_temp, lamda_temp);
					  lamda_temp[0] += dx;
					  lamda_temp[1] += dy;
					  lamda_temp[2] += dz;
					  domain->lamda2x(lamda_temp, nodal_temp);
					  buf[m++] = nodal_temp[0];
					  buf[m++] = nodal_temp[1];
					  buf[m++] = nodal_temp[2];
				  }
				  else {
					  buf[m++] = nodal_temp[0] + dx;
					  buf[m++] = nodal_temp[1] + dy;
					  buf[m++] = nodal_temp[2] + dz;
				  }
				  buf[m++] = nodal_gradients[j][nodecount][poly_index][0];
				  buf[m++] = nodal_gradients[j][nodecount][poly_index][1];
				  buf[m++] = nodal_gradients[j][nodecount][poly_index][2];
				  buf[m++] = nodal_velocities[j][nodecount][poly_index][0];
				  buf[m++] = nodal_velocities[j][nodecount][poly_index][1];
				  buf[m++] = nodal_velocities[j][nodecount][poly_index][2];
			  }
		  }
        }
      }
    }
  }

  if (atom->nextra_border)
    for (int iextra = 0; iextra < atom->nextra_border; iextra++)
      m += modify->fix[atom->extra_border[iextra]]->pack_border(n,list,&buf[m]);

  return m;
}

/* ---------------------------------------------------------------------- */

void AtomVecCAC::unpack_border(int n, int first, double *buf)
{
  int i,m,last;
  int *nodes_count_list = atom->nodes_per_element_list;
  m = 0;
  last = first + n;
  for (i = first; i < last; i++) {
    if (i == nmax) grow(0);
    x[i][0] = buf[m++];
    x[i][1] = buf[m++];
    x[i][2] = buf[m++];
    tag[i] = (tagint) ubuf(buf[m++]).i;
    type[i] = (int) ubuf(buf[m++]).i;
    mask[i] = (int) ubuf(buf[m++]).i;
	element_type[i] = buf[m++];
	element_scale[i][0] = buf[m++];
	element_scale[i][1] = buf[m++];
	element_scale[i][2] = buf[m++];
	poly_count[i] = buf[m++];
	for (int type_map = 0; type_map < poly_count[i]; type_map++) {
		node_types[i][type_map] = buf[m++];
	}
	for (int nodecount = 0; nodecount < nodes_count_list[element_type[i]]; nodecount++) {
		for (int poly_index = 0; poly_index < poly_count[i]; poly_index++)
		{
			nodal_positions[i][nodecount][poly_index][0] = buf[m++];
			nodal_positions[i][nodecount][poly_index][1] = buf[m++];
			nodal_positions[i][nodecount][poly_index][2] = buf[m++];
			initial_nodal_positions[i][nodecount][poly_index][0] = buf[m++];
			initial_nodal_positions[i][nodecount][poly_index][1] = buf[m++];
			initial_nodal_positions[i][nodecount][poly_index][2] = buf[m++];
			nodal_gradients[i][nodecount][poly_index][0] = buf[m++];
			nodal_gradients[i][nodecount][poly_index][1] = buf[m++];
			nodal_gradients[i][nodecount][poly_index][2] = buf[m++];
			nodal_velocities[i][nodecount][poly_index][0] = buf[m++];
			nodal_velocities[i][nodecount][poly_index][1] = buf[m++];
			nodal_velocities[i][nodecount][poly_index][2] = buf[m++];
		}
	}
  }

  if (atom->nextra_border)
    for (int iextra = 0; iextra < atom->nextra_border; iextra++)
      m += modify->fix[atom->extra_border[iextra]]->
        unpack_border(n,first,&buf[m]);
}

/* ---------------------------------------------------------------------- */

void AtomVecCAC::unpack_border_vel(int n, int first, double *buf)
{
  int i,m,last;
  int *nodes_count_list = atom->nodes_per_element_list;
  m = 0;
  last = first + n;
  for (i = first; i < last; i++) {
    if (i == nmax) grow(0);
    x[i][0] = buf[m++];
    x[i][1] = buf[m++];
    x[i][2] = buf[m++];
    tag[i] = (tagint) ubuf(buf[m++]).i;
    type[i] = (int) ubuf(buf[m++]).i;
    mask[i] = (int) ubuf(buf[m++]).i;
    v[i][0] = buf[m++];
    v[i][1] = buf[m++];
    v[i][2] = buf[m++];
	element_type[i] = buf[m++];
	element_scale[i][0] = buf[m++];
	element_scale[i][1] = buf[m++];
	element_scale[i][2] = buf[m++];
	poly_count[i] = buf[m++];
	for (int type_map = 0; type_map < poly_count[i]; type_map++) {
		node_types[i][type_map] = buf[m++];
	}
	for (int nodecount = 0; nodecount < nodes_count_list[element_type[i]]; nodecount++) {
		for (int poly_index = 0; poly_index < poly_count[i]; poly_index++)
		{
			nodal_positions[i][nodecount][poly_index][0] = buf[m++];
			nodal_positions[i][nodecount][poly_index][1] = buf[m++];
			nodal_positions[i][nodecount][poly_index][2] = buf[m++];
			initial_nodal_positions[i][nodecount][poly_index][0] = buf[m++];
			initial_nodal_positions[i][nodecount][poly_index][1] = buf[m++];
			initial_nodal_positions[i][nodecount][poly_index][2] = buf[m++];
			nodal_gradients[i][nodecount][poly_index][0] = buf[m++];
			nodal_gradients[i][nodecount][poly_index][1] = buf[m++];
			nodal_gradients[i][nodecount][poly_index][2] = buf[m++];
			nodal_velocities[i][nodecount][poly_index][0] = buf[m++];
			nodal_velocities[i][nodecount][poly_index][1] = buf[m++];
			nodal_velocities[i][nodecount][poly_index][2] = buf[m++];
		}
	}
  }

  if (atom->nextra_border)
    for (int iextra = 0; iextra < atom->nextra_border; iextra++)
      m += modify->fix[atom->extra_border[iextra]]->
        unpack_border(n,first,&buf[m]);
}

/* ----------------------------------------------------------------------
   pack data for atom I for sending to another proc
   xyz must be 1st 3 values, so comm::exchange() can test on them
------------------------------------------------------------------------- */

int AtomVecCAC::pack_exchange(int i, double *buf)
{
  int m = 1;
	int *nodes_count_list = atom->nodes_per_element_list;
  buf[m++] = x[i][0];
  buf[m++] = x[i][1];
  buf[m++] = x[i][2];
  buf[m++] = v[i][0];
  buf[m++] = v[i][1];
  buf[m++] = v[i][2];
  buf[m++] = ubuf(tag[i]).d;
  buf[m++] = ubuf(type[i]).d;
  buf[m++] = ubuf(mask[i]).d;
  buf[m++] = ubuf(image[i]).d;
  buf[m++] = element_type[i];
  buf[m++] = element_scale[i][0];
  buf[m++] = element_scale[i][1];
  buf[m++] = element_scale[i][2];
  buf[m++] = poly_count[i];
  for (int type_map = 0; type_map < poly_count[i]; type_map++) {
	  buf[m++] = node_types[i][type_map];
  }

  for (int nodecount = 0; nodecount< nodes_count_list[element_type[i]]; nodecount++) {
	  for (int poly_index = 0; poly_index < poly_count[i]; poly_index++)
	  {
		  buf[m++] = nodal_positions[i][nodecount][poly_index][0];
		  buf[m++] = nodal_positions[i][nodecount][poly_index][1];
		  buf[m++] = nodal_positions[i][nodecount][poly_index][2];
		  buf[m++] = initial_nodal_positions[i][nodecount][poly_index][0];
		  buf[m++] = initial_nodal_positions[i][nodecount][poly_index][1];
		  buf[m++] = initial_nodal_positions[i][nodecount][poly_index][2];
		  buf[m++] = nodal_gradients[i][nodecount][poly_index][0];
		  buf[m++] = nodal_gradients[i][nodecount][poly_index][1];
		  buf[m++] = nodal_gradients[i][nodecount][poly_index][2];
		  buf[m++] = nodal_velocities[i][nodecount][poly_index][0];
		  buf[m++] = nodal_velocities[i][nodecount][poly_index][1];
		  buf[m++] = nodal_velocities[i][nodecount][poly_index][2];
	  }
  }

  if (atom->nextra_grow)
    for (int iextra = 0; iextra < atom->nextra_grow; iextra++)
      m += modify->fix[atom->extra_grow[iextra]]->pack_exchange(i,&buf[m]);

  buf[0] = m;
  return m;
}

/* ---------------------------------------------------------------------- */

int AtomVecCAC::unpack_exchange(double *buf)
{
  int nlocal = atom->nlocal;
	int *nodes_count_list = atom->nodes_per_element_list;
  if (nlocal == nmax) grow(0);

  int m = 1;
  x[nlocal][0] = buf[m++];
  x[nlocal][1] = buf[m++];
  x[nlocal][2] = buf[m++];
  v[nlocal][0] = buf[m++];
  v[nlocal][1] = buf[m++];
  v[nlocal][2] = buf[m++];
  tag[nlocal] = (tagint) ubuf(buf[m++]).i;
  type[nlocal] = (int) ubuf(buf[m++]).i;
  mask[nlocal] = (int) ubuf(buf[m++]).i;
  image[nlocal] = (imageint) ubuf(buf[m++]).i;
  element_type[nlocal] = buf[m++];
  element_scale[nlocal][0] = buf[m++];
  element_scale[nlocal][1] = buf[m++];
  element_scale[nlocal][2] = buf[m++];
  poly_count[nlocal] = buf[m++];
  for (int type_map = 0; type_map < poly_count[nlocal]; type_map++) {
	  node_types[nlocal][type_map] = buf[m++];
  }
  for (int nodecount = 0; nodecount < nodes_count_list[element_type[nlocal]]; nodecount++) {
	  for (int poly_index = 0; poly_index < poly_count[nlocal]; poly_index++)
	  {
		  nodal_positions[nlocal][nodecount][poly_index][0] = buf[m++];
		  nodal_positions[nlocal][nodecount][poly_index][1] = buf[m++];
		  nodal_positions[nlocal][nodecount][poly_index][2] = buf[m++];
		  initial_nodal_positions[nlocal][nodecount][poly_index][0] = buf[m++];
		  initial_nodal_positions[nlocal][nodecount][poly_index][1] = buf[m++];
		  initial_nodal_positions[nlocal][nodecount][poly_index][2] = buf[m++];
		  nodal_gradients[nlocal][nodecount][poly_index][0] = buf[m++];
		  nodal_gradients[nlocal][nodecount][poly_index][1] = buf[m++];
		  nodal_gradients[nlocal][nodecount][poly_index][2] = buf[m++];
		  nodal_velocities[nlocal][nodecount][poly_index][0] = buf[m++];
		  nodal_velocities[nlocal][nodecount][poly_index][1] = buf[m++];
		  nodal_velocities[nlocal][nodecount][poly_index][2] = buf[m++];
	  }
  }

  if (atom->nextra_grow)
    for (int iextra = 0; iextra < atom->nextra_grow; iextra++)
      m += modify->fix[atom->extra_grow[iextra]]->
        unpack_exchange(nlocal,&buf[m]);

  atom->nlocal++;
  return m;
}

/* ----------------------------------------------------------------------
   size of restart data for all atoms owned by this proc
   include extra data stored by fixes
------------------------------------------------------------------------- */

int AtomVecCAC::size_restart()
{
  int i;
  int current_node_count; 
	int *nodes_count_list = atom->nodes_per_element_list;
  int nlocal = atom->nlocal;
  int n=0;
  for (i=0; i < nlocal; i++){
  current_node_count=nodes_count_list[element_type[i]];
   n += (16+12*current_node_count*poly_count[i]+poly_count[i]);
  }
  

  if (atom->nextra_restart)
    for (int iextra = 0; iextra < atom->nextra_restart; iextra++)
      for (i = 0; i < nlocal; i++)
        n += modify->fix[atom->extra_restart[iextra]]->size_restart(i);

  return n;
}

/* ----------------------------------------------------------------------
   pack atom I's data for restart file including extra quantities
   xyz must be 1st 3 values, so that read_restart can test on them
   molecular types may be negative, but write as positive
------------------------------------------------------------------------- */

int AtomVecCAC::pack_restart(int i, double *buf)
{
  int m = 1;
  int current_node_count; 
	int *nodes_count_list = atom->nodes_per_element_list;
  buf[m++] = x[i][0];
  buf[m++] = x[i][1];
  buf[m++] = x[i][2];
  buf[m++] = ubuf(tag[i]).d;
  buf[m++] = ubuf(type[i]).d;
  buf[m++] = ubuf(mask[i]).d;
  buf[m++] = ubuf(image[i]).d;
  buf[m++] = v[i][0];
  buf[m++] = v[i][1];
  buf[m++] = v[i][2];
  buf[m++] = ubuf(element_type[i]).d;
  buf[m++] = ubuf(element_scale[i][0]).d;
  buf[m++] = ubuf(element_scale[i][1]).d;
  buf[m++] = ubuf(element_scale[i][2]).d;
  buf[m++] = ubuf(poly_count[i]).d;
  current_node_count=nodes_count_list[element_type[i]];
  for (int type_map = 0; type_map < poly_count[i]; type_map++) {
	  buf[m++] = node_types[i][type_map];
  }

  for (int nodecount = 0; nodecount< current_node_count; nodecount++) {
	  for (int poly_index = 0; poly_index < poly_count[i]; poly_index++)
	  {
		  buf[m++] = nodal_positions[i][nodecount][poly_index][0];
		  buf[m++] = nodal_positions[i][nodecount][poly_index][1];
		  buf[m++] = nodal_positions[i][nodecount][poly_index][2];
		  buf[m++] = initial_nodal_positions[i][nodecount][poly_index][0];
		  buf[m++] = initial_nodal_positions[i][nodecount][poly_index][1];
		  buf[m++] = initial_nodal_positions[i][nodecount][poly_index][2];
		  buf[m++] = nodal_gradients[i][nodecount][poly_index][0];
		  buf[m++] = nodal_gradients[i][nodecount][poly_index][1];
		  buf[m++] = nodal_gradients[i][nodecount][poly_index][2];
		  buf[m++] = nodal_velocities[i][nodecount][poly_index][0];
		  buf[m++] = nodal_velocities[i][nodecount][poly_index][1];
		  buf[m++] = nodal_velocities[i][nodecount][poly_index][2];
	  }
  }
  if (atom->nextra_restart)
    for (int iextra = 0; iextra < atom->nextra_restart; iextra++)
      m += modify->fix[atom->extra_restart[iextra]]->pack_restart(i,&buf[m]);

  buf[0] = m;
  return m;
}

/* ----------------------------------------------------------------------
   unpack data for one atom from restart file including extra quantities
------------------------------------------------------------------------- */

int AtomVecCAC::unpack_restart(double *buf)
{
  int nlocal = atom->nlocal;
  int current_node_count;
  int *nodes_count_list = atom->nodes_per_element_list;
  scale_search_range=atom->scale_search_range;
  scale_list=atom->scale_list;
  scale_count=atom->scale_count;
  initial_size=atom->initial_size;
  if (nlocal == nmax) {
    grow(0);
    if (atom->nextra_store)
      memory->grow(atom->extra,nmax,atom->nextra_store,"atom:extra");
  }
  	
  int m = 1;
  x[nlocal][0] = buf[m++];
  x[nlocal][1] = buf[m++];
  x[nlocal][2] = buf[m++];
  tag[nlocal] = (tagint) ubuf(buf[m++]).i;
  type[nlocal] = (int) ubuf(buf[m++]).i;
  mask[nlocal] = (int) ubuf(buf[m++]).i;
  image[nlocal] = (imageint) ubuf(buf[m++]).i;
  v[nlocal][0] = buf[m++];
  v[nlocal][1] = buf[m++];
  v[nlocal][2] = buf[m++];
  element_type[nlocal] = (int) ubuf(buf[m++]).i;
  element_scale[nlocal][0] = (int) ubuf(buf[m++]).i;
  element_scale[nlocal][1] = (int) ubuf(buf[m++]).i;
  element_scale[nlocal][2] = (int) ubuf(buf[m++]).i;
  poly_count[nlocal] = (int) ubuf(buf[m++]).i;
  current_node_count=nodes_count_list[element_type[nlocal]];

  for (int type_map = 0; type_map < poly_count[nlocal]; type_map++) {
	  node_types[nlocal][type_map] = buf[m++];
  }
  for (int nodecount = 0; nodecount < current_node_count; nodecount++) {
	  for (int poly_index = 0; poly_index < poly_count[nlocal]; poly_index++)
	  {
		  nodal_positions[nlocal][nodecount][poly_index][0] = buf[m++];
		  nodal_positions[nlocal][nodecount][poly_index][1] = buf[m++];
		  nodal_positions[nlocal][nodecount][poly_index][2] = buf[m++];
		  initial_nodal_positions[nlocal][nodecount][poly_index][0] = buf[m++];
		  initial_nodal_positions[nlocal][nodecount][poly_index][1] = buf[m++];
		  initial_nodal_positions[nlocal][nodecount][poly_index][2] = buf[m++];
		  nodal_gradients[nlocal][nodecount][poly_index][0] = buf[m++];
		  nodal_gradients[nlocal][nodecount][poly_index][1] = buf[m++];
		  nodal_gradients[nlocal][nodecount][poly_index][2] = buf[m++];
		  nodal_velocities[nlocal][nodecount][poly_index][0] = buf[m++];
		  nodal_velocities[nlocal][nodecount][poly_index][1] = buf[m++];
		  nodal_velocities[nlocal][nodecount][poly_index][2] = buf[m++];
	  }
  }
  double max_distancesq;
  double current_distancesq;
  double search_radius;
  double dx,dy,dz;
 
  int search_range_delta_ratio = 4;
 int grow_size = 10;
  int error_scale=1.10; //essentially a fudge factor since  the search algorithm is not so rigorous
  int expand=0;
  int match[3];
//edit scale search ranges and scale counts if necessary
	if(!atom->oneflag){
	
		/*
	if(scale_count==0&&element_type[nlocal]!=1){
		
		
		scale_count=1;
		scale_list[scale_count]=element_scale[nlocal][0];
		if(element_scale[nlocal][0]!=element_scale[nlocal][1]){
			scale_count++;
			scale_list[scale_count]=element_scale[nlocal][1];
		}
		if(element_scale[nlocal][0]!=element_scale[nlocal][2]&&element_scale[nlocal][1]!=element_scale[nlocal][2]){
			scale_count++;
			scale_list[scale_count]=element_scale[nlocal][2];
		}

				 max_distancesq=0;
	
		//compute search radius using maximum distance between nodes an element;
		//not the most rigorous approach; feel free to improve :).
	
		for(int ipoly=0; ipoly < poly_count[nlocal]; ipoly++)
		for(int i=0; i<current_node_count; i++){
			for (int j=i+1; j<current_node_count; j++){
				dx=nodal_positions[nlocal][i][ipoly][0]-nodal_positions[nlocal][j][ipoly][0];
				dy=nodal_positions[nlocal][i][ipoly][1]-nodal_positions[nlocal][j][ipoly][1];
				dz=nodal_positions[nlocal][i][ipoly][2]-nodal_positions[nlocal][j][ipoly][2];
				current_distancesq=dx*dx+dy*dy+dz*dz;
				if(current_distancesq>max_distancesq) max_distancesq=current_distancesq;
			}
		}
		search_radius=sqrt(max_distancesq);
		search_radius*=error_scale;
		scale_search_range[scale_count]=search_radius;
		if(scale_count>1)
		scale_search_range[scale_count-1]=search_radius;
		if(scale_count>2)
		scale_search_range[scale_count-2]=search_radius;
	}
	*/

	match[0]=match[1]=match[2]=0;
	for(int scalecheck=0; scalecheck<scale_count; scalecheck++){
	if(element_scale[nlocal][0]==scale_list[scalecheck]){
		match[0]=1;
	}
	if(element_scale[nlocal][1]==scale_list[scalecheck]){
		match[1]=1;
	}
	if(element_scale[nlocal][2]==scale_list[scalecheck]){
		match[2]=1;
	}
	}	
	if(!match[0]){
		expand++;
	}
	if(!match[1]&&element_scale[nlocal][0]!=element_scale[nlocal][1]){
		expand++;
	}
	if(!match[2]&&element_scale[nlocal][0]!=element_scale[nlocal][2]&&element_scale[nlocal][1]!=element_scale[nlocal][2]){
		expand++;
	}
	int element_max_scale;
	element_max_scale=element_scale[nlocal][0];
	if(element_max_scale<element_scale[nlocal][1])
	element_max_scale=element_scale[nlocal][1];
	if(element_max_scale<element_scale[nlocal][2])
	element_max_scale=element_scale[nlocal][2];

//expand the scale list and ranges if need be

	if(expand)
	{
		 max_distancesq=0;
	
		//compute search radius using maximum distance between nodes of an element;
		//not the most rigorous approach; feel free to improve :).
	
		for(int ipoly=0; ipoly < poly_count[nlocal]; ipoly++)
		for(int i=0; i<current_node_count; i++){
			for (int j=i+1; j<current_node_count; j++){
				dx=nodal_positions[nlocal][i][ipoly][0]-nodal_positions[nlocal][j][ipoly][0];
				dy=nodal_positions[nlocal][i][ipoly][1]-nodal_positions[nlocal][j][ipoly][1];
				dz=nodal_positions[nlocal][i][ipoly][2]-nodal_positions[nlocal][j][ipoly][2];
				current_distancesq=dx*dx+dy*dy+dz*dz;
				if(current_distancesq>max_distancesq) max_distancesq=current_distancesq;
			}
		}
		search_radius=sqrt(max_distancesq);
		search_radius*=error_scale;
		
		if(scale_count+expand>initial_size){
		initial_size+=grow_size;	
		scale_search_range= memory->grow(atom->scale_search_range, initial_size, "atom:element_type");
		scale_list= memory->grow(atom->scale_list, initial_size, "atom:scale_search_range");
		for(int i=scale_count; i<initial_size; i++){
			scale_search_range[i]=0;
			scale_list[i]=0;
			}
		}
		scale_count+=expand;
		scale_search_range[scale_count-1]=search_radius;
		scale_list[scale_count-1]=element_scale[nlocal][0];
		if(expand>1){
		scale_search_range[scale_count-2]=search_radius;
		scale_list[scale_count-2]=element_scale[nlocal][0];
		}
		if(expand>2){
		scale_search_range[scale_count-3]=search_radius;
		scale_list[scale_count-3]=element_scale[nlocal][0];
		}
		
		
		
	}
	//compute maximum search range
	for(int i=1; i<scale_count; i++) {
		if(scale_search_range[i]>atom->max_search_range) atom->max_search_range=scale_search_range[i];
	}
	}
	atom->scale_count=scale_count;


  double **extra = atom->extra;
  if (atom->nextra_store) {
    int size = static_cast<int> (buf[0]) - m;
    for (int i = 0; i < size; i++) extra[nlocal][i] = buf[m++];
  }

  atom->nlocal++;
  return m;
}

/* ----------------------------------------------------------------------
   create one atom of itype at coord
   set other values to defaults
------------------------------------------------------------------------- */

void AtomVecCAC::create_atom(int itype, double *coord)
{
  int nlocal = atom->nlocal;
  if (nlocal == nmax) grow(0);

  tag[nlocal] = 0;
  type[nlocal] = itype;
  x[nlocal][0] = coord[0];
  x[nlocal][1] = coord[1];
  x[nlocal][2] = coord[2];
  mask[nlocal] = 1;
  image[nlocal] = ((imageint) IMGMAX << IMG2BITS) |
    ((imageint) IMGMAX << IMGBITS) | IMGMAX;
  v[nlocal][0] = 0.0;
  v[nlocal][1] = 0.0;
  v[nlocal][2] = 0.0;
  element_type[nlocal] = 0;

  poly_count[nlocal] =1;
  for (int type_map = 0; type_map < poly_count[nlocal]; type_map++) {
	  node_types[nlocal][type_map] = type_map+1;
  }
  for (int nodecount = 0; nodecount < nodes_per_element; nodecount++) {
	  for (int poly_index = 0; poly_index < poly_count[nlocal]; poly_index++)
	  {
		  nodal_positions[nlocal][nodecount][poly_index][0] = coord[0];
		  nodal_positions[nlocal][nodecount][poly_index][1] = coord[1];
		  nodal_positions[nlocal][nodecount][poly_index][2] = coord[2];
		  initial_nodal_positions[nlocal][nodecount][poly_index][0] = coord[0];
		  initial_nodal_positions[nlocal][nodecount][poly_index][1] = coord[1];
		  initial_nodal_positions[nlocal][nodecount][poly_index][2] = coord[2];
		  nodal_gradients[nlocal][nodecount][poly_index][0] = 0;
		  nodal_gradients[nlocal][nodecount][poly_index][1] = 0;
		  nodal_gradients[nlocal][nodecount][poly_index][2] = 0;
		  nodal_velocities[nlocal][nodecount][poly_index][0] = 0;
		  nodal_velocities[nlocal][nodecount][poly_index][1] = 0;
		  nodal_velocities[nlocal][nodecount][poly_index][2] = 0;
	  }
  }

  atom->nlocal++;
}

/* ----------------------------------------------------------------------
   unpack one element from CAC_Elements section of data file
   initialize other Element quantities
------------------------------------------------------------------------- */

void AtomVecCAC::data_atom(double *coord, imageint imagetmp, char **values)
{
	int nlocal = atom->nlocal;
	int node_index,node_type,poly_index;
	int typefound;
	if (nlocal == nmax) grow(0);
	int nodetotal, npoly;
	int tmp;
	int types_filled = 0;
	int *nodes_count_list = atom->nodes_per_element_list;
	scale_search_range=atom->scale_search_range;
    scale_list=atom->scale_list;
    scale_count=atom->scale_count;
    initial_size=atom->initial_size;

	poly_index = 0;
	tag[nlocal] = ATOTAGINT(values[0]);
	char* element_type_read;
	element_type_read = values[1];
	type[nlocal] = 1;

	npoly = atoi(values[2]);
	if (npoly > maxpoly)
		error->one(FLERR, "poly count declared in data file was greater than maxpoly in input file");
		//add a block for new element types; this does not have to be done in unpack restart
		//by convention atoms are element type 0; this has nothing to do with the mass type.
		// you should only be setting the name of your element in the read comparison check
		// and the corresponding numerical id for this type
	if (strcmp(element_type_read, "Eight_Node") == 0) {//add a control block for new types of elements
		element_type[nlocal] = 1;
		nodetotal = nodes_count_list[element_type[nlocal]];
		poly_count[nlocal] = npoly;
		element_scale[nlocal][0] = atoi(values[3]);
		element_scale[nlocal][1] = atoi(values[4]);
		element_scale[nlocal][2] = atoi(values[5]);
	}
	else if (strcmp(element_type_read, "Atom") == 0) {
		element_type[nlocal] = 0;
		nodetotal = nodes_count_list[element_type[nlocal]];
		npoly = 1;
		poly_count[nlocal] = npoly;
		element_scale[nlocal][0] = 1;
		element_scale[nlocal][1] = 1;
		element_scale[nlocal][2] = 1;
		
	}
	else {
		error->one(FLERR, "element type not yet defined, add definition in atom_vec_CAC.cpp style");
	}
	if (nodetotal > nodes_per_element)
		error->one(FLERR, "element type requires a greater number of nodes than the specified maximum nodes per element passed to atom style CAC");
	for (int polycount = 0; polycount < npoly; polycount++) {
		node_types[nlocal][polycount] = 0; //initialize
	}


	int m = 6;

	for (int nodecount = 0; nodecount < nodetotal; nodecount++)
	{
		for (int polycount = 0; polycount < npoly;polycount++) {


		node_index = atoi(values[m++]);
		if (node_index <= 0 ||node_index > nodetotal)
			error->one(FLERR, "Invalid node index in CAC_Elements section of data file");
		poly_index = atoi(values[m++]);
		if (poly_index <= 0 || poly_index > npoly)
			error->one(FLERR, "Invalid poly index in CAC_Elements section of data file");
		node_index = node_index - 1;
		poly_index = poly_index - 1;
		node_type = atoi(values[m++]);
		if (node_type <= 0 || node_type > atom->ntypes)
			error->one(FLERR, "Invalid atom type in CAC_Elements section of data file");
		 

		if (node_types[nlocal][poly_index] == 0 || node_types[nlocal][poly_index] == node_type) {
			node_types[nlocal][poly_index] = node_type;
		}
		else {
			error->one(FLERR, "more than one type assigned to the same poly index in an element");
		}


		
/*
		//search encountered types for current elements and add new type if not found
		for (int scan_sort = 0; scan_sort < types_filled; scan_sort++) {
			
			if (node_type == type_map[scan_sort]) {
				type_index = scan_sort;
				
				break;
			}
			if(scan_sort==types_filled-1) {
				types_filled += 1;
				if (types_filled > ntypes) {
					error->one(FLERR, "number of element types exceeds specified number of types");
				}

				type_map[types_filled - 1] = node_type;
				type_index = types_filled - 1;
			}


		}
		
		//add first type 
		if (types_filled == 0) {
			type_map[0] = node_type;
			types_filled = 1;
		}
		*/
		

		nodal_positions[nlocal][node_index][poly_index][0] = atof(values[m++]);
		nodal_positions[nlocal][node_index][poly_index][1] = atof(values[m++]);
		nodal_positions[nlocal][node_index][poly_index][2] = atof(values[m++]);
		initial_nodal_positions[nlocal][node_index][poly_index][0] = nodal_positions[nlocal][node_index][poly_index][0];
		initial_nodal_positions[nlocal][node_index][poly_index][1] = nodal_positions[nlocal][node_index][poly_index][1];
		initial_nodal_positions[nlocal][node_index][poly_index][2] = nodal_positions[nlocal][node_index][poly_index][2];
		nodal_gradients[nlocal][node_index][poly_index][0] = 0;
		nodal_gradients[nlocal][node_index][poly_index][1] = 0;
		nodal_gradients[nlocal][node_index][poly_index][2] = 0;
		nodal_velocities[nlocal][node_index][poly_index][0] = 0;
		nodal_velocities[nlocal][node_index][poly_index][1] = 0;
		nodal_velocities[nlocal][node_index][poly_index][2] = 0;
	}
	}
	
  double max_distancesq;
  double current_distancesq;
  double search_radius;
  double dx,dy,dz;
  int current_node_count;
  int search_range_delta_ratio = 4;
 
  int grow_size = 10;
  int error_scale=1.10; //essentially a fudge factor since  the search algorithm is not so rigorous
  int expand=0;
  int match[3];
	if(!atom->oneflag){
	
		/*
	if(scale_count==0&&element_type[nlocal]!=1){
		
		
		scale_count=1;
		scale_list[scale_count]=element_scale[nlocal][0];
		if(element_scale[nlocal][0]!=element_scale[nlocal][1]){
			scale_count++;
			scale_list[scale_count]=element_scale[nlocal][1];
		}
		if(element_scale[nlocal][0]!=element_scale[nlocal][2]&&element_scale[nlocal][1]!=element_scale[nlocal][2]){
			scale_count++;
			scale_list[scale_count]=element_scale[nlocal][2];
		}

				 max_distancesq=0;
	
		//compute search radius using maximum distance between nodes an element;
		//not the most rigorous approach; feel free to improve :).
	
		for(int ipoly=0; ipoly < poly_count[nlocal]; ipoly++)
		for(int i=0; i<current_node_count; i++){
			for (int j=i+1; j<current_node_count; j++){
				dx=nodal_positions[nlocal][i][ipoly][0]-nodal_positions[nlocal][j][ipoly][0];
				dy=nodal_positions[nlocal][i][ipoly][1]-nodal_positions[nlocal][j][ipoly][1];
				dz=nodal_positions[nlocal][i][ipoly][2]-nodal_positions[nlocal][j][ipoly][2];
				current_distancesq=dx*dx+dy*dy+dz*dz;
				if(current_distancesq>max_distancesq) max_distancesq=current_distancesq;
			}
		}
		search_radius=sqrt(max_distancesq);
		search_radius*=error_scale;
		scale_search_range[scale_count]=search_radius;
		if(scale_count>1)
		scale_search_range[scale_count-1]=search_radius;
		if(scale_count>2)
		scale_search_range[scale_count-2]=search_radius;
	}
	*/

	match[0]=match[1]=match[2]=0;
	for(int scalecheck=0; scalecheck<scale_count; scalecheck++){
	if(element_scale[nlocal][0]==scale_list[scalecheck]){
		match[0]=1;
	}
	if(element_scale[nlocal][1]==scale_list[scalecheck]){
		match[1]=1;
	}
	if(element_scale[nlocal][2]==scale_list[scalecheck]){
		match[2]=1;
	}
	}	
	if(!match[0]){
		expand++;
	}
	if(!match[1]&&element_scale[nlocal][0]!=element_scale[nlocal][1]){
		expand++;
	}
	if(!match[2]&&element_scale[nlocal][0]!=element_scale[nlocal][2]&&element_scale[nlocal][1]!=element_scale[nlocal][2]){
		expand++;
	}
	int element_max_scale;
	element_max_scale=element_scale[nlocal][0];
	if(element_max_scale<element_scale[nlocal][1])
	element_max_scale=element_scale[nlocal][1];
	if(element_max_scale<element_scale[nlocal][2])
	element_max_scale=element_scale[nlocal][2];

//expand the scale list and ranges if need be

	if(expand)
	{
		 max_distancesq=0;
	
		//compute search radius using maximum distance between nodes of an element;
		//not the most rigorous approach; feel free to improve :).
	
		for(int ipoly=0; ipoly < poly_count[nlocal]; ipoly++)
		for(int i=0; i<nodetotal; i++){
			for (int j=i+1; j<nodetotal; j++){
				dx=nodal_positions[nlocal][i][ipoly][0]-nodal_positions[nlocal][j][ipoly][0];
				dy=nodal_positions[nlocal][i][ipoly][1]-nodal_positions[nlocal][j][ipoly][1];
				dz=nodal_positions[nlocal][i][ipoly][2]-nodal_positions[nlocal][j][ipoly][2];
				current_distancesq=dx*dx+dy*dy+dz*dz;
				if(current_distancesq>max_distancesq) max_distancesq=current_distancesq;
			}
		}
		search_radius=sqrt(max_distancesq);
		search_radius*=error_scale;
		
		if(scale_count+expand>initial_size){
		initial_size+=grow_size;	
		scale_search_range= memory->grow(atom->scale_search_range, initial_size, "atom:element_type");
		scale_list= memory->grow(atom->scale_list, initial_size, "atom:scale_search_range");
		for(int i=scale_count; i<initial_size; i++){
			scale_search_range[i]=0;
			scale_list[i]=0;
			}
		}
		scale_count+=expand;
		scale_search_range[scale_count-1]=search_radius;
		scale_list[scale_count-1]=element_scale[nlocal][0];
		if(expand>1){
		scale_search_range[scale_count-2]=search_radius;
		scale_list[scale_count-2]=element_scale[nlocal][0];
		}
		if(expand>2){
		scale_search_range[scale_count-3]=search_radius;
		scale_list[scale_count-3]=element_scale[nlocal][0];
		}
		
		
		
	}
	//compute maximum search range
	for(int i=0; i<scale_count; i++) {
		if(scale_search_range[i]>atom->max_search_range) atom->max_search_range=scale_search_range[i];
	}
	}
	atom->scale_count=scale_count;
  x[nlocal][0] = coord[0];
  x[nlocal][1] = coord[1];
  x[nlocal][2] = coord[2];

  image[nlocal] = imagetmp;

  mask[nlocal] = 1;
  v[nlocal][0] = 0.0;
  v[nlocal][1] = 0.0;
  v[nlocal][2] = 0.0;

  atom->nlocal++;
  
}

/* ----------------------------------------------------------------------
   pack atom info for data file including 3 image flags
------------------------------------------------------------------------- */

void AtomVecCAC::pack_data(double **buf)
{
  int nlocal = atom->nlocal;
	int *nodes_count_list = atom->nodes_per_element_list;
  for (int i = 0; i < nlocal; i++) {
       int m=0;
    buf[i][m++] = ubuf(tag[i]).d;
    buf[i][m++] = ubuf(type[i]).d;
	buf[i][m++] = element_type[i];
	buf[i][m++] = element_scale[i][0];
	buf[i][m++] = element_scale[i][1];
	buf[i][m++] = element_scale[i][2];
	buf[i][m++] = poly_count[i];
	for (int type_map = 0; type_map < poly_count[i]; type_map++) {
		buf[i][m++] = node_types[i][type_map];
	}

	for (int nodecount = 0; nodecount< nodes_count_list[element_type[i]]; nodecount++) {
		for (int poly_index = 0; poly_index < maxpoly; poly_index++)
		{
			buf[i][m++] = nodal_positions[i][nodecount][poly_index][0];
			buf[i][m++] = nodal_positions[i][nodecount][poly_index][1];
			buf[i][m++] = nodal_positions[i][nodecount][poly_index][2];
			buf[i][m++] = initial_nodal_positions[i][nodecount][poly_index][0];
			buf[i][m++] = initial_nodal_positions[i][nodecount][poly_index][1];
			buf[i][m++] = initial_nodal_positions[i][nodecount][poly_index][2];
			buf[i][m++] = nodal_gradients[i][nodecount][poly_index][0];
			buf[i][m++] = nodal_gradients[i][nodecount][poly_index][1];
			buf[i][m++] = nodal_gradients[i][nodecount][poly_index][2];
			buf[i][m++] = nodal_velocities[i][nodecount][poly_index][0];
			buf[i][m++] = nodal_velocities[i][nodecount][poly_index][1];
			buf[i][m++] = nodal_velocities[i][nodecount][poly_index][2];
		}
	}
    buf[i][m++] = x[i][0];
    buf[i][m++] = x[i][1];
    buf[i][m++] = x[i][2];
    buf[i][m++] = ubuf((image[i] & IMGMASK) - IMGMAX).d;
    buf[i][m++] = ubuf((image[i] >> IMGBITS & IMGMASK) - IMGMAX).d;
    buf[i][m++] = ubuf((image[i] >> IMG2BITS) - IMGMAX).d;
  }
}

/* ----------------------------------------------------------------------
   write atom info to data file including 3 image flags
------------------------------------------------------------------------- */

void AtomVecCAC::write_data(FILE *fp, int n, double **buf)
{
  for (int i = 0; i < n; i++)
    fprintf(fp,TAGINT_FORMAT " %d %-1.16e %-1.16e %-1.16e %d %d %d\n",
            (tagint) ubuf(buf[i][0]).i,(int) ubuf(buf[i][1]).i,
            buf[i][2],buf[i][3],buf[i][4],
            (int) ubuf(buf[i][5]).i,(int) ubuf(buf[i][6]).i,
            (int) ubuf(buf[i][7]).i);
}

/* ----------------------------------------------------------------------
   return # of bytes of allocated memory
------------------------------------------------------------------------- */

bigint AtomVecCAC::memory_usage()
{
  bigint bytes = 0;

  if (atom->memcheck("tag")) bytes += memory->usage(tag,nmax);
  if (atom->memcheck("type")) bytes += memory->usage(type,nmax);
  if (atom->memcheck("mask")) bytes += memory->usage(mask,nmax);
  if (atom->memcheck("image")) bytes += memory->usage(image,nmax);
  if (atom->memcheck("x")) bytes += memory->usage(x,nmax,3);
  if (atom->memcheck("v")) bytes += memory->usage(v,nmax,3);
  if (atom->memcheck("f")) bytes += memory->usage(f,nmax*comm->nthreads,3);
  if (atom->memcheck("element_types")) bytes += memory->usage(element_type, nmax);
  if (atom->memcheck("poly_counts")) bytes += memory->usage(poly_count, nmax);
  if (atom->memcheck("node_types")) bytes += memory->usage(node_types, nmax,maxpoly);
  if (atom->memcheck("element_scale")) bytes += memory->usage(element_scale, nmax, 3);
  if (atom->memcheck("nodal_positions")) bytes += memory->usage(nodal_positions,nmax,nodes_per_element,maxpoly,3);
  if (atom->memcheck("initial_nodal_positions")) bytes += memory->usage(initial_nodal_positions, nmax, nodes_per_element, maxpoly, 3);
  if (atom->memcheck("nodal_velocities")) bytes += memory->usage(nodal_velocities,nmax,nodes_per_element, maxpoly,3);
  if (atom->memcheck("nodal_gradients")) bytes += memory->usage(nodal_gradients,nmax,nodes_per_element, maxpoly,3);
  if (atom->memcheck("nodal_forces")) bytes += memory->usage(nodal_forces,nmax,nodes_per_element, maxpoly,3);


  return bytes;
}

void AtomVecCAC::force_clear(int a, size_t) {

	for (int i = 0; i < atom->nlocal; i++) {


		

		for (int nodecount = 0; nodecount < nodes_per_element; nodecount++) {
			for (int poly_index = 0; poly_index < poly_count[i]; poly_index++)
			{

				nodal_forces[i][nodecount][poly_index][0] = 0;
				nodal_forces[i][nodecount][poly_index][1] = 0;
				nodal_forces[i][nodecount][poly_index][2] = 0;
				nodal_gradients[i][nodecount][poly_index][0] = 0;
				nodal_gradients[i][nodecount][poly_index][1] = 0;
				nodal_gradients[i][nodecount][poly_index][2] = 0;
			}
		}
	}
}
