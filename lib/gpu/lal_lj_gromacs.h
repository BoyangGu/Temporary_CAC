/***************************************************************************
                                lj_gromacs.h
                             -------------------
                            Trung Dac Nguyen (ORNL)

  Class for acceleration of the lj/gromacs pair style.

 __________________________________________________________________________
    This file is part of the LAMMPS Accelerator Library (LAMMPS_AL)
 __________________________________________________________________________

    begin                :
    email                : nguyentd@ornl.gov
 ***************************************************************************/

#ifndef LAL_LJ_GROMACS_H
#define LAL_LJ_GROMACS_H

#include "lal_base_atomic.h"

namespace LAMMPS_AL {

template <class numtyp, class acctyp>
class LJGROMACS : public BaseAtomic<numtyp, acctyp> {
 public:
  LJGROMACS();
  ~LJGROMACS();

  /// Clear any previous data and set up for a new LAMMPS run
  /** \param max_nbors initial number of rows in the neighbor matrix
    * \param cell_size cutoff + skin
    * \param gpu_split fraction of particles handled by device
    *
    * Returns:
    * -  0 if successfull
    * - -1 if fix gpu not found
    * - -3 if there is an out of memory error
    * - -4 if the GPU library was not compiled for GPU
    * - -5 Double precision is not supported on card **/
  int init(const int ntypes, double **host_cutsq,
           double **host_lj1, double **host_lj2, double **host_lj3,
           double **host_lj4, double *host_special_lj,
           const int nlocal, const int nall, const int max_nbors,
           const int maxspecial, const double cell_size,
           const double gpu_split, FILE *screen,
           double **host_ljsw1, double **host_ljsw2, double **host_ljsw3,
           double **host_ljsw4, double **host_ljsw5,
           double **cut_inner, double **cut_inner_sq);

  /// Clear all host and device data
  /** \note This is called at the beginning of the init() routine **/
  void clear();

  /// Returns memory usage on device per atom
  int bytes_per_atom(const int max_nbors) const;

  /// Total host memory used by library for pair style
  double host_memory_usage() const;

  // --------------------------- TYPE DATA --------------------------

  /// lj1.x = lj1, lj1.y = lj2, lj1.z = cutsq, lj1.w = cut_inner_sq
  UCL_D_Vec<numtyp4> lj1;
  /// lj3.x = lj3, lj3.y = lj4, lj3.z = cut_inner, lj3.w = ljsw5
  UCL_D_Vec<numtyp4> lj3;
  /// ljsw.x = ljsw1, ljsw.y = ljsw2, ljsw.z = ljsw3, ljsw.w = ljsw4
  UCL_D_Vec<numtyp4> ljsw;
  /// Special LJ values [0-3] and Special Coul values [4-7]
  UCL_D_Vec<numtyp> sp_lj;

  /// If atom type constants fit in shared memory, use fast kernels
  bool shared_types;

  /// Number of atom types
  int _lj_types;

 private:
  bool _allocated;
  void loop(const bool _eflag, const bool _vflag);
};

}

#endif
