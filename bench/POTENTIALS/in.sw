# bulk Si via Stillinger-Weber

units		metal
atom_style	atomic

lattice		diamond 5.431
region		box block 0 20 0 20 0 10
create_box	1 box
create_atoms	1 box

pair_style	sw
pair_coeff	* * Si.sw Si
mass            1 28.06

velocity	all create 1000.0 376847 loop geom

neighbor	1.0 bin
neigh_modify    delay 5 every 1

fix		1 all nve

timestep	0.001

run		100
