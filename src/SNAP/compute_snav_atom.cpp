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
#include "sna.h"
#include <string.h>
#include <stdlib.h>
#include "compute_snav_atom.h"
#include "atom.h"
#include "update.h"
#include "modify.h"
#include "neighbor.h"
#include "neigh_list.h"
#include "neigh_request.h"
#include "force.h"
#include "pair.h"
#include "comm.h"
#include "memory.h"
#include "error.h"
#include "openmp_snap.h"

using namespace LAMMPS_NS;

ComputeSNAVAtom::ComputeSNAVAtom(LAMMPS *lmp, int narg, char **arg) :
  Compute(lmp, narg, arg), cutsq(NULL), list(NULL), snav(NULL),
  radelem(NULL), wjelem(NULL)
{
  double rfac0, rmin0;
  int twojmax, switchflag, bzeroflag;
  radelem = NULL;
  wjelem = NULL;

  int ntypes = atom->ntypes;
  int nargmin = 6+2*ntypes;

  if (narg < nargmin) error->all(FLERR,"Illegal compute snav/atom command");

  // default values

  diagonalstyle = 0;
  rmin0 = 0.0;
  switchflag = 1;
  bzeroflag = 1;
  quadraticflag = 0;

  // process required arguments

  memory->create(radelem,ntypes+1,"sna/atom:radelem"); // offset by 1 to match up with types
  memory->create(wjelem,ntypes+1,"sna/atom:wjelem");
  rcutfac = atof(arg[3]);
  rfac0 = atof(arg[4]);
  twojmax = atoi(arg[5]);
  for(int i = 0; i < ntypes; i++)
    radelem[i+1] = atof(arg[6+i]);
  for(int i = 0; i < ntypes; i++)
    wjelem[i+1] = atof(arg[6+ntypes+i]);
  // construct cutsq
  double cut;
  memory->create(cutsq,ntypes+1,ntypes+1,"sna/atom:cutsq");
  for(int i = 1; i <= ntypes; i++) {
    cut = 2.0*radelem[i]*rcutfac;
    cutsq[i][i] = cut*cut;
    for(int j = i+1; j <= ntypes; j++) {
      cut = (radelem[i]+radelem[j])*rcutfac;
      cutsq[i][j] = cutsq[j][i] = cut*cut;
    }
  }

  // process optional args

  int iarg = nargmin;

  while (iarg < narg) {
    if (strcmp(arg[iarg],"diagonal") == 0) {
      if (iarg+2 > narg)
        error->all(FLERR,"Illegal compute snav/atom command");
      diagonalstyle = atof(arg[iarg+1]);
      if (diagonalstyle < 0 || diagonalstyle > 3)
        error->all(FLERR,"Illegal compute snav/atom command");
      iarg += 2;
    } else if (strcmp(arg[iarg],"rmin0") == 0) {
      if (iarg+2 > narg)
        error->all(FLERR,"Illegal compute snav/atom command");
      rmin0 = atof(arg[iarg+1]);
      iarg += 2;
    } else if (strcmp(arg[iarg],"switchflag") == 0) {
      if (iarg+2 > narg)
        error->all(FLERR,"Illegal compute snav/atom command");
      switchflag = atoi(arg[iarg+1]);
      iarg += 2;
    } else if (strcmp(arg[iarg],"quadraticflag") == 0) {
      if (iarg+2 > narg)
        error->all(FLERR,"Illegal compute snav/atom command");
      quadraticflag = atoi(arg[iarg+1]);
      iarg += 2;
    } else error->all(FLERR,"Illegal compute snav/atom command");
  }

  snaptr = new SNA*[comm->nthreads];
#if defined(_OPENMP)
#pragma omp parallel default(none) shared(lmp,rfac0,twojmax,rmin0,switchflag,bzeroflag)
#endif
  {
    int tid = omp_get_thread_num();

    // always unset use_shared_arrays since it does not work with computes
    snaptr[tid] = new SNA(lmp,rfac0,twojmax,diagonalstyle,
                          0 /*use_shared_arrays*/, rmin0,switchflag,bzeroflag);
  }

  ncoeff = snaptr[0]->ncoeff;
  twoncoeff = 2*ncoeff;
  threencoeff = 3*ncoeff;
  fourncoeff = 4*ncoeff;
  fivencoeff = 5*ncoeff;
  sixncoeff = 6*ncoeff;
  size_peratom_cols = sixncoeff*atom->ntypes;
  if (quadraticflag) {
    ncoeffq = ncoeff*ncoeff;
    twoncoeffq = 2*ncoeffq;
    threencoeffq = 3*ncoeffq;
    fourncoeffq = 4*ncoeffq;
    fivencoeffq = 5*ncoeffq;
    sixncoeffq = 6*ncoeffq;
    size_peratom_cols +=
      sixncoeffq*atom->ntypes;
  }
  comm_reverse = size_peratom_cols;
  peratom_flag = 1;

  nmax = 0;
  njmax = 0;
  snav = NULL;

}

/* ---------------------------------------------------------------------- */

ComputeSNAVAtom::~ComputeSNAVAtom()
{
  memory->destroy(snav);
  memory->destroy(radelem);
  memory->destroy(wjelem);
  memory->destroy(cutsq);
  delete [] snaptr;
}

/* ---------------------------------------------------------------------- */

void ComputeSNAVAtom::init()
{
  if (force->pair == NULL)
    error->all(FLERR,"Compute snav/atom requires a pair style be defined");
   // TODO: Not sure what to do with this error check since cutoff radius is not
  // a single number
 //if (sqrt(cutsq) > force->pair->cutforce)
   // error->all(FLERR,"Compute snav/atom cutoff is longer than pairwise cutoff");

  // need an occasional full neighbor list

  int irequest = neighbor->request(this,instance_me);
  neighbor->requests[irequest]->pair = 0;
  neighbor->requests[irequest]->compute = 1;
  neighbor->requests[irequest]->half = 0;
  neighbor->requests[irequest]->full = 1;
  neighbor->requests[irequest]->occasional = 1;

  int count = 0;
  for (int i = 0; i < modify->ncompute; i++)
    if (strcmp(modify->compute[i]->style,"snav/atom") == 0) count++;
  if (count > 1 && comm->me == 0)
    error->warning(FLERR,"More than one compute snav/atom");
#if defined(_OPENMP)
#pragma omp parallel default(none)
#endif
  {
    int tid = omp_get_thread_num();
    snaptr[tid]->init();
  }
}

/* ---------------------------------------------------------------------- */

void ComputeSNAVAtom::init_list(int id, NeighList *ptr)
{
  list = ptr;
}

/* ---------------------------------------------------------------------- */

void ComputeSNAVAtom::compute_peratom()
{
  int ntotal = atom->nlocal + atom->nghost;

  invoked_peratom = update->ntimestep;

  // grow snav array if necessary

  if (atom->nmax > nmax) {
    memory->destroy(snav);
    nmax = atom->nmax;
    memory->create(snav,nmax,size_peratom_cols,
                   "snav/atom:snav");
    array_atom = snav;
  }

  // clear local array

  for (int i = 0; i < ntotal; i++)
    for (int icoeff = 0; icoeff < size_peratom_cols; icoeff++) {
      snav[i][icoeff] = 0.0;
    }

  // invoke full neighbor list (will copy or build if necessary)

  neighbor->build_one(list);

  const int inum = list->inum;
  const int* const ilist = list->ilist;
  const int* const numneigh = list->numneigh;
  int** const firstneigh = list->firstneigh;
  int * const type = atom->type;
  // compute sna derivatives for each atom in group
  // use full neighbor list to count atoms less than cutoff

  double** const x = atom->x;
  const int* const mask = atom->mask;

#if defined(_OPENMP)
#pragma omp parallel for default(none)
#endif
  for (int ii = 0; ii < inum; ii++) {
    const int tid = omp_get_thread_num();
    const int i = ilist[ii];
    if (mask[i] & groupbit) {

      const double xtmp = x[i][0];
      const double ytmp = x[i][1];
      const double ztmp = x[i][2];
      const int itype = type[i];
      const double radi = radelem[itype];

      const int* const jlist = firstneigh[i];
      const int jnum = numneigh[i];

      const int typeoffset = sixncoeff*(atom->type[i]-1);
      const int quadraticoffset = sixncoeff*atom->ntypes +
        sixncoeffq*(atom->type[i]-1);

      // insure rij, inside, and typej  are of size jnum

          snaptr[tid]->grow_rij(jnum);

      // rij[][3] = displacements between atom I and those neighbors
      // inside = indices of neighbors of I within cutoff
      // typej = types of neighbors of I within cutoff
      // note Rij sign convention => dU/dRij = dU/dRj = -dU/dRi

      int ninside = 0;
      for (int jj = 0; jj < jnum; jj++) {
        int j = jlist[jj];
        j &= NEIGHMASK;

        const double delx = x[j][0] - xtmp;
        const double dely = x[j][1] - ytmp;
        const double delz = x[j][2] - ztmp;
        const double rsq = delx*delx + dely*dely + delz*delz;
        int jtype = type[j];
        if (rsq < cutsq[itype][jtype]&&rsq>1e-20) {
          snaptr[tid]->rij[ninside][0] = delx;
          snaptr[tid]->rij[ninside][1] = dely;
          snaptr[tid]->rij[ninside][2] = delz;
          snaptr[tid]->inside[ninside] = j;
          snaptr[tid]->wj[ninside] = wjelem[jtype];
          snaptr[tid]->rcutij[ninside] = (radi+radelem[jtype])*rcutfac;
          ninside++;
        }
      }

      snaptr[tid]->compute_ui(ninside);
      snaptr[tid]->compute_zi();
      if (quadraticflag) {
        snaptr[tid]->compute_bi();
        snaptr[tid]->copy_bi2bvec();
      }

      for (int jj = 0; jj < ninside; jj++) {
        const int j = snaptr[tid]->inside[jj];

        snaptr[tid]->compute_duidrj(snaptr[tid]->rij[jj],
                                    snaptr[tid]->wj[jj],
                                    snaptr[tid]->rcutij[jj]);
        snaptr[tid]->compute_dbidrj();
        snaptr[tid]->copy_dbi2dbvec();

        // Accumulate -dBi/dRi*Ri, -dBi/dRj*Rj

        double *snavi = snav[i]+typeoffset;
        double *snavj = snav[j]+typeoffset;

        for (int icoeff = 0; icoeff < ncoeff; icoeff++) {
          snavi[icoeff]             += snaptr[tid]->dbvec[icoeff][0]*xtmp;
          snavi[icoeff+ncoeff]      += snaptr[tid]->dbvec[icoeff][1]*ytmp;
          snavi[icoeff+twoncoeff]   += snaptr[tid]->dbvec[icoeff][2]*ztmp;
          snavi[icoeff+threencoeff] += snaptr[tid]->dbvec[icoeff][1]*ztmp;
          snavi[icoeff+fourncoeff]  += snaptr[tid]->dbvec[icoeff][0]*ztmp;
          snavi[icoeff+fivencoeff]  += snaptr[tid]->dbvec[icoeff][0]*ytmp;
          snavj[icoeff]             -= snaptr[tid]->dbvec[icoeff][0]*x[j][0];
          snavj[icoeff+ncoeff]      -= snaptr[tid]->dbvec[icoeff][1]*x[j][1];
          snavj[icoeff+twoncoeff]   -= snaptr[tid]->dbvec[icoeff][2]*x[j][2];
          snavj[icoeff+threencoeff] -= snaptr[tid]->dbvec[icoeff][1]*x[j][2];
          snavj[icoeff+fourncoeff]  -= snaptr[tid]->dbvec[icoeff][0]*x[j][2];
          snavj[icoeff+fivencoeff]  -= snaptr[tid]->dbvec[icoeff][0]*x[j][1];
        }

        if (quadraticflag) {
          double *snavi = snav[i]+quadraticoffset;
          double *snavj = snav[j]+quadraticoffset;
          int ncount = 0;
          for (int icoeff = 0; icoeff < ncoeff; icoeff++) {
            double bi = snaptr[tid]->bvec[icoeff];
            double bix = snaptr[tid]->dbvec[icoeff][0];
            double biy = snaptr[tid]->dbvec[icoeff][1];
            double biz = snaptr[tid]->dbvec[icoeff][2];

            // upper-triangular elements of quadratic matrix

            for (int jcoeff = icoeff; jcoeff < ncoeff; jcoeff++) {
              double dbxtmp = bi*snaptr[tid]->dbvec[jcoeff][0]
                + bix*snaptr[tid]->bvec[jcoeff];
              double dbytmp = bi*snaptr[tid]->dbvec[jcoeff][1]
                + biy*snaptr[tid]->bvec[jcoeff];
              double dbztmp = bi*snaptr[tid]->dbvec[jcoeff][2]
                + biz*snaptr[tid]->bvec[jcoeff];
              snavi[ncount] +=               dbxtmp*xtmp;
              snavi[ncount+ncoeffq] +=      dbytmp*ytmp;
              snavi[ncount+twoncoeffq] +=   dbztmp*ztmp;
              snavi[ncount+threencoeffq] += dbytmp*ztmp;
              snavi[ncount+fourncoeffq] +=  dbxtmp*ztmp;
              snavi[ncount+fivencoeffq] +=  dbxtmp*ytmp;
              snavj[ncount] -=               dbxtmp*x[j][0];
              snavj[ncount+ncoeffq] -=      dbytmp*x[j][1];
              snavj[ncount+twoncoeffq] -=   dbztmp*x[j][2];
              snavj[ncount+threencoeffq] -= dbytmp*x[j][2];
              snavj[ncount+fourncoeffq] -=  dbxtmp*x[j][2];
              snavj[ncount+fivencoeffq] -=  dbxtmp*x[j][1];
              ncount++;
            }
          }
        }
      }
    }
  }

  // communicate snav contributions between neighbor procs

  comm->reverse_comm_compute(this);

}

/* ---------------------------------------------------------------------- */

int ComputeSNAVAtom::pack_reverse_comm(int n, int first, double *buf)
{
  int i,m,last,icoeff;

  m = 0;
  last = first + n;
  for (i = first; i < last; i++)
    for (icoeff = 0; icoeff < size_peratom_cols; icoeff++)
      buf[m++] = snav[i][icoeff];
  return comm_reverse;
}

/* ---------------------------------------------------------------------- */

void ComputeSNAVAtom::unpack_reverse_comm(int n, int *list, double *buf)
{
  int i,j,m,icoeff;

  m = 0;
  for (i = 0; i < n; i++) {
    j = list[i];
    for (icoeff = 0; icoeff < size_peratom_cols; icoeff++)
      snav[j][icoeff] += buf[m++];
  }
}

/* ----------------------------------------------------------------------
   memory usage
------------------------------------------------------------------------- */

double ComputeSNAVAtom::memory_usage()
{
  double bytes = nmax*size_peratom_cols * sizeof(double);
  bytes += 3*njmax*sizeof(double);
  bytes += njmax*sizeof(int);
  bytes += sixncoeff*atom->ntypes;
  if (quadraticflag) bytes += sixncoeffq*atom->ntypes;
  bytes += snaptr[0]->memory_usage()*comm->nthreads;
  return bytes;
}
