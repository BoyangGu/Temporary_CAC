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

#ifdef PAIR_CLASS

PairStyle(gw/zbl,PairGWZBL)

#else

#ifndef LMP_PAIR_GW_ZBL_H
#define LMP_PAIR_GW_ZBL_H

#include "pair_gw.h"

namespace LAMMPS_NS {

class PairGWZBL : public PairGW {
 public:
  PairGWZBL(class LAMMPS *);
  ~PairGWZBL() {}

 private:
  double global_a_0;                // Bohr radius for Coulomb repulsion
  double global_epsilon_0;        // permittivity of vacuum for Coulomb repulsion
  double global_e;                // proton charge (negative of electron charge)

  void read_file(char *);
  void repulsive(Param *, double, double &, int, double &);

  double gw_fa(double, Param *);
  double gw_fa_d(double, Param *);

  double F_fermi(double, Param *);
  double F_fermi_d(double, Param *);
};

}

#endif
#endif

/* ERROR/WARNING messages:

E: Pair GW/zbl requires metal or real units

This is a current restriction of this pair potential.

E: Cannot open GW potential file %s

The specified GW potential file cannot be opened.  Check that the
path and name are correct.

E: Incorrect format in GW potential file

Incorrect number of words per line in the potential file.

E: Illegal GW parameter

One or more of the coefficients defined in the potential file is
invalid.

*/
