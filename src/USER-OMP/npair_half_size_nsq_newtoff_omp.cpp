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

#include <string.h>
#include "npair_half_size_nsq_newtoff_omp.h"
#include "npair_omp.h"
#include "neighbor.h"
#include "neigh_list.h"
#include "atom.h"
#include "atom_vec.h"
#include "group.h"
#include "my_page.h"
#include "error.h"

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

NPairHalfSizeNsqNewtoffOmp::NPairHalfSizeNsqNewtoffOmp(LAMMPS *lmp) :
  NPair(lmp) {}

/* ----------------------------------------------------------------------
   size particles
   N^2 / 2 search for neighbor pairs with partial Newton's 3rd law
   shear history must be accounted for when a neighbor pair is added
   pair added to list if atoms i and j are both owned and i < j
   pair added if j is ghost (also stored by proc owning j)
------------------------------------------------------------------------- */

void NPairHalfSizeNsqNewtoffOmp::build(NeighList *list)
{
  const int nlocal = (includegroup) ? atom->nfirst : atom->nlocal;
  const int bitmask = (includegroup) ? group->bitmask[includegroup] : 0;
  const int history = list->history;
  const int mask_history = 3 << SBBITS;

  NPAIR_OMP_INIT;

#if defined(_OPENMP)
#pragma omp parallel default(none) shared(list)
#endif
  NPAIR_OMP_SETUP(nlocal);

  int i,j,n;
  double xtmp,ytmp,ztmp,delx,dely,delz,rsq;
  double radi,radsum,cutsq;
  int *neighptr;

  double **x = atom->x;
  double *radius = atom->radius;
  int *type = atom->type;
  int *mask = atom->mask;
  tagint *molecule = atom->molecule;
  int nall = atom->nlocal + atom->nghost;

  int *ilist = list->ilist;
  int *numneigh = list->numneigh;
  int **firstneigh = list->firstneigh;

  // each thread has its own page allocator
  MyPage<int> &ipage = list->ipage[tid];
  ipage.reset();

  for (i = ifrom; i < ito; i++) {

    n = 0;
    neighptr = ipage.vget();

    xtmp = x[i][0];
    ytmp = x[i][1];
    ztmp = x[i][2];
    radi = radius[i];

    // loop over remaining atoms, owned and ghost

    for (j = i+1; j < nall; j++) {
      if (includegroup && !(mask[j] & bitmask)) continue;
      if (exclude && exclusion(i,j,type[i],type[j],mask,molecule)) continue;

      delx = xtmp - x[j][0];
      dely = ytmp - x[j][1];
      delz = ztmp - x[j][2];
      rsq = delx*delx + dely*dely + delz*delz;
      radsum = radi + radius[j];
      cutsq = (radsum+skin) * (radsum+skin);

      if (rsq <= cutsq) {
        if (history && rsq < radsum*radsum)
          neighptr[n++] = j ^ mask_history;
        else
          neighptr[n++] = j;
      }
    }

    ilist[i] = i;
    firstneigh[i] = neighptr;
    numneigh[i] = n;
    ipage.vgot(n);
    if (ipage.status())
      error->one(FLERR,"Neighbor list overflow, boost neigh_modify one");
  }
  NPAIR_OMP_CLOSE;
  list->inum = nlocal;
}
