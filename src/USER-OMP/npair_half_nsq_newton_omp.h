/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifdef NPAIR_CLASS

NPairStyle(half/nsq/newton/omp,
           NPairHalfNsqNewtonOmp,
           NP_HALF | NP_NSQ | NP_NEWTON | NP_OMP | NP_ORTHO | NP_TRI)

#else

#ifndef LMP_NPAIR_HALF_NSQ_NEWTON_OMP_H
#define LMP_NPAIR_HALF_NSQ_NEWTON_OMP_H

#include "npair.h"

namespace LAMMPS_NS {

class NPairHalfNsqNewtonOmp : public NPair {
 public:
  NPairHalfNsqNewtonOmp(class LAMMPS *);
  ~NPairHalfNsqNewtonOmp() {}
  void build(class NeighList *);
};

}

#endif
#endif

/* ERROR/WARNING messages:

*/
