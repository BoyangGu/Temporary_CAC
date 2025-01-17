/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifdef FIX_CLASS

FixStyle(qeq/comb/omp,FixQEQCombOMP)

#else

#ifndef LMP_FIX_QEQ_COMB_OMP_H
#define LMP_FIX_QEQ_COMB_OMP_H

#include "fix_qeq_comb.h"

namespace LAMMPS_NS {

class FixQEQCombOMP : public FixQEQComb {
 public:
  FixQEQCombOMP(class LAMMPS *, int, char **);
  virtual void init();
  virtual void post_force(int);
};

}

#endif
#endif
