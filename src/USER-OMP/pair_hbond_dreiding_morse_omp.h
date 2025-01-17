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

/* ----------------------------------------------------------------------
   Contributing author: Axel Kohlmeyer (Temple U)
------------------------------------------------------------------------- */

#ifdef PAIR_CLASS

PairStyle(hbond/dreiding/morse/omp,PairHbondDreidingMorseOMP)

#else

#ifndef LMP_PAIR_HBOND_DREIDING_MORSE_OMP_H
#define LMP_PAIR_HBOND_DREIDING_MORSE_OMP_H

#include "pair_hbond_dreiding_morse.h"
#include "thr_omp.h"

namespace LAMMPS_NS {

class PairHbondDreidingMorseOMP : public PairHbondDreidingMorse, public ThrOMP {

 public:
  PairHbondDreidingMorseOMP(class LAMMPS *);
  virtual ~PairHbondDreidingMorseOMP();

  virtual void compute(int, int);
  virtual double memory_usage();

 protected:
  double *hbcount_thr, *hbeng_thr;

 private:
  template <int EVFLAG, int EFLAG, int NEWTON_PAIR>
  void eval(int ifrom, int ito, ThrData * const thr);
};

}

#endif
#endif
