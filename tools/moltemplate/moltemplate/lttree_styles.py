#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Author: Andrew Jewett (jewett.aij at g mail)
#         http://www.chem.ucsb.edu/~sheagroup
# License: 3-clause BSD License  (See LICENSE.TXT)
# Copyright (c) 2012, Regents of the University of California
# All rights reserved.

try:
    from .ttree_lex import InputError
except (SystemError, ValueError):
    # not installed as a package
    from ttree_lex import InputError

# Users of lttree typically generate the following files:
# The variable below refer to file names generated by
# write() and write_once() commands in a lttree-file.
# (I keep changing my mind what I want these names to be.)
data_prefix = "Data "
data_prefix_no_space = "Data"
data_atoms = "Data Atoms"
data_masses = "Data Masses"
data_velocities = "Data Velocities"
data_bonds = "Data Bonds"
data_bond_list = "Data Bond List"
data_angles = "Data Angles"
data_dihedrals = "Data Dihedrals"
data_impropers = "Data Impropers"
data_bond_coeffs = "Data Bond Coeffs"
data_angle_coeffs = "Data Angle Coeffs"
data_dihedral_coeffs = "Data Dihedral Coeffs"
data_improper_coeffs = "Data Improper Coeffs"
data_pair_coeffs = "Data Pair Coeffs"

# interactions-by-type (not id. This is not part of the LAMMPS standard.)
data_bonds_by_type = "Data Bonds By Type"
data_angles_by_type = "Data Angles By Type"
data_dihedrals_by_type = "Data Dihedrals By Type"
data_impropers_by_type = "Data Impropers By Type"

# class2 data sections
data_bondbond_coeffs = "Data BondBond Coeffs"
data_bondangle_coeffs = "Data BondAngle Coeffs"
data_middlebondtorsion_coeffs = "Data MiddleBondTorsion Coeffs"
data_endbondtorsion_coeffs = "Data EndBondTorsion Coeffs"
data_angletorsion_coeffs = "Data AngleTorsion Coeffs"
data_angleangletorsion_coeffs = "Data AngleAngleTorsion Coeffs"
data_bondbond13_coeffs = "Data BondBond13 Coeffs"
data_angleangle_coeffs = "Data AngleAngle Coeffs"

# sections for non-point-like particles:
data_ellipsoids = "Data Ellipsoids"
data_lines = "Data Lines"
data_triangles = "Data Triangles"

# periodic boundary conditions
data_boundary = "Data Boundary"
# (for backward compatibility), an older version of this file was named:
data_pbc = "Data PBC"

# The files are fragments of a LAMMPS data file (see "read_data").
# In addition, moltemplate may also generate the following files:
in_prefix = "In "
in_prefix_no_space = "In"
in_init = "In Init"
in_settings = "In Settings"
in_coords = "In Coords"
# These files represent different sections of the LAMMPS input script.


# Atom Styles in LAMMPS as of 2011-7-29
g_style_map = {'angle':     ['atom-ID', 'molecule-ID', 'atom-type', 'x', 'y', 'z'],
               'atomic':    ['atom-ID', 'atom-type', 'x', 'y', 'z'],
               'body':      ['atom-ID', 'atom-type', 'bodyflag', 'mass', 'x', 'y', 'z'],
               'bond':      ['atom-ID', 'molecule-ID', 'atom-type', 'x', 'y', 'z'],
               'charge':    ['atom-ID', 'atom-type', 'q', 'x', 'y', 'z'],
               'dipole':    ['atom-ID', 'atom-type', 'q', 'x', 'y', 'z', 'mux', 'muy', 'muz'],
               'dpd':       ['atom-ID', 'atom-type', 'theta', 'x', 'y', 'z'],
               'electron':  ['atom-ID', 'atom-type', 'q', 'spin', 'eradius', 'x', 'y', 'z'],
               'ellipsoid': ['atom-ID', 'atom-type', 'x', 'y', 'z', 'quatw', 'quati', 'quatj', 'quatk'],
               'full':      ['atom-ID', 'molecule-ID', 'atom-type', 'q', 'x', 'y', 'z'],
               'line':      ['atom-ID', 'molecule-ID', 'atom-type', 'lineflag', 'density', 'x', 'y', 'z'],
               'meso':      ['atom-ID', 'atom-type', 'rho', 'e', 'cv', 'x', 'y', 'z'],
               'molecular': ['atom-ID', 'molecule-ID', 'atom-type', 'x', 'y', 'z'],
               'peri':      ['atom-ID', 'atom-type', 'volume', 'density', 'x', 'y', 'z'],
               'smd':       ['atom-ID', 'atom-type', 'molecule-ID' 'volume', 'mass', 'kernel-radius', 'contact-radius', 'x', 'y', 'z'],
               'sphere':    ['atom-ID', 'atom-type', 'diameter', 'density', 'x', 'y', 'z'],
               'template':  ['atom-ID', 'molecule-ID', 'template-index', 'template-atom', 'atom-type', 'x', 'y', 'z'],
               'tri':       ['atom-ID', 'molecule-ID', 'atom-type', 'triangleflag', 'density', 'x', 'y', 'z'],
               'wavepacket': ['atom-ID', 'atom-type', 'charge', 'spin', 'eradius', 'etag', 'cs_re', 'cs_im', 'x', 'y', 'z'],
               'hybrid':    ['atom-ID', 'atom-type', 'x', 'y', 'z'],
               # The following styles were removed from LAMMPS as of 2012-3
               'colloid':   ['atom-ID', 'atom-type', 'x', 'y', 'z'],
               'granular':  ['atom-ID', 'atom-type', 'diameter', 'density', 'x', 'y', 'z']}


def AtomStyle2ColNames(atom_style_string):

    atom_style_string = atom_style_string.strip()
    if len(atom_style_string) == 0:
        raise InputError('Error: Invalid atom_style\n'
                         '       (The atom_style command was followed by an empty string.)\n')
    atom_style_args = atom_style_string.split()
    atom_style = atom_style_args[0]

    hybrid_args = atom_style_args[1:]
    if (atom_style not in g_style_map):
        if (len(atom_style_args) >= 2):
            # If the atom_style_string includes at least 2 words, then we
            # interpret this as a list of the individual column names
            return atom_style_args
        else:
            raise InputError(
                'Error: Unrecognized atom_style: \"' + atom_style + '\"\n')

    if (atom_style != 'hybrid'):
        return g_style_map[atom_style]
    else:
        column_names = ['atom-ID', 'atom-type', 'x', 'y', 'z']
        if (len(hybrid_args) == 0):
            raise InputError(
                'Error: atom_style hybrid must be followed by a sub_style.\n')
        for sub_style in hybrid_args:
            if (sub_style not in g_style_map):
                raise InputError(
                    'Error: Unrecognized atom_style: \"' + sub_style + '\"\n')
            for cname in g_style_map[sub_style]:
                if cname not in column_names:
                    column_names.append(cname)

        return column_names


def ColNames2AidAtypeMolid(column_names):
    # Because of the diversity of ways that these
    # numbers are referred to in the LAMMPS documentation,
    # we have to be flexible and allow the user to refer
    # to these quantities in a variety of ways.
    # Hopefully this covers everything:

    if 'atom-ID' in column_names:
        i_atomid = column_names.index('atom-ID')
    elif 'atom−ID' in column_names:  # (− is the character used in the manual)
        i_atomid = column_names.index('atom−ID')
    elif 'atomID' in column_names:
        i_atomid = column_names.index('atomID')
    elif 'atomid' in column_names:
        i_atomid = column_names.index('atomid')
    elif 'id' in column_names:
        i_atomid = column_names.index('id')
    elif 'atom' in column_names:
        i_atomid = column_names.index('atom')
    elif '$atom' in column_names:
        i_atomid = column_names.index('$atom')
    else:
        raise InputError('Error: List of column names lacks an \"atom-ID\"\n')

    if 'atom-type' in column_names:
        i_atomtype = column_names.index('atom-type')
    elif 'atom−type' in column_names:  # (− hyphen character used in manual)
        i_atomtype = column_names.index('atom−type')
    elif 'atomtype' in column_names:
        i_atomtype = column_names.index('atomtype')
    elif 'type' in column_names:
        i_atomtype = column_names.index('type')
    elif '@atom' in column_names:
        i_atomtype = column_names.index('@atom')
    else:
        raise InputError(
            'Error: List of column names lacks an \"atom-type\"\n')

    i_molid = None
    if 'molecule-ID' in column_names:
        i_molid = column_names.index('molecule-ID')
    elif 'molecule−ID' in column_names:  # (− hyphen character used in manual)
        i_molid = column_names.index('molecule−ID')
    elif 'moleculeID' in column_names:
        i_molid = column_names.index('moleculeID')
    elif 'moleculeid' in column_names:
        i_molid = column_names.index('moleculeid')
    elif 'molecule' in column_names:
        i_molid = column_names.index('molecule')
    elif 'molID' in column_names:
        i_molid = column_names.index('molID')
    elif 'molid' in column_names:
        i_molid = column_names.index('molid')
    elif 'mol' in column_names:
        i_molid = column_names.index('mol')
    elif '$mol' in column_names:
        i_molid = column_names.index('$mol')
    else:
        pass  # some atom_types do not have a valid molecule-ID

    return i_atomid, i_atomtype, i_molid


def ColNames2Coords(column_names):
    """ Which of the columns correspond to coordinates
        which must be transformed using rigid-body
        (affine: rotation + translation) transformations?
        This function outputs a list of lists of triplets of integers.

    """
    i_x = None
    i_y = None
    i_z = None
    if 'x' in column_names:
        i_x = column_names.index('x')
    if 'y' in column_names:
        i_y = column_names.index('y')
    if 'z' in column_names:
        i_z = column_names.index('z')
    if (((i_x != None) != (i_y != None)) or
            ((i_y != None) != (i_z != None)) or
            ((i_z != None) != (i_x != None))):
        raise InputError(
            'Error: custom atom_style list must define x, y, and z.\n')
    return [[i_x, i_y, i_z]]


def ColNames2Vects(column_names):
    """ Which of the columns correspond to coordinates
        which must be transformed using rotations?
        Some coordinates like dipole moments and
        ellipsoid orientations should only be rotated
        (not translated).
        This function outputs a list of lists of triplets of integers.

    """
    vects = []
    i_mux = None
    i_muy = None
    i_muz = None
    if 'mux' in column_names:
        i_mux = column_names.index('mux')
    if 'muy' in column_names:
        i_muy = column_names.index('muy')
    if 'muz' in column_names:
        i_muz = column_names.index('muz')
    if (((i_mux != None) != (i_muy != None)) or
            ((i_muy != None) != (i_muz != None)) or
            ((i_muz != None) != (i_mux != None))):
        raise InputError(
            'Error: custom atom_style list must define mux, muy, and muz or none.\n')
    if i_mux != None:
        vects.append([i_mux, i_muy, i_muz])
    return vects
