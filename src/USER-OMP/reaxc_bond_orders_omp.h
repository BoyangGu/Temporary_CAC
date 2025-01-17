/*----------------------------------------------------------------------
  PuReMD - Purdue ReaxFF Molecular Dynamics Program
  Website: https://www.cs.purdue.edu/puremd

  Copyright (2010) Purdue University

  Contributing authors:
  H. M. Aktulga, J. Fogarty, S. Pandit, A. Grama
  Corresponding author:
  Hasan Metin Aktulga, Michigan State University, hma@cse.msu.edu

  Please cite the related publication:
  H. M. Aktulga, J. C. Fogarty, S. A. Pandit, A. Y. Grama,
  "Parallel Reactive Molecular Dynamics: Numerical Methods and
  Algorithmic Techniques", Parallel Computing, 38 (4-5), 245-259

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of
  the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details:
  <http://www.gnu.org/licenses/>.
  ----------------------------------------------------------------------*/

#ifndef __BOND_ORDERS_OMP_H_
#define __BOND_ORDERS_OMP_H_

#include "reaxc_types.h"
#include "reaxc_bond_orders.h"

void Add_dBond_to_ForcesOMP( reax_system*, int, int, storage*, reax_list** );
void Add_dBond_to_Forces_NPTOMP( reax_system *system, int, int,
                                 simulation_data*, storage*, reax_list** );

int BOp_OMP(storage*, reax_list*, double, int, int, far_neighbor_data*,
            single_body_parameters*, single_body_parameters*, two_body_parameters*,
            int, double, double, double, double, double, double, double);

void BOOMP( reax_system*, control_params*, simulation_data*,
         storage*, reax_list**, output_controls* );
#endif
