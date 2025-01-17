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

#include "nbin_CAC.h"
#include "neighbor.h"
#include "atom.h"
#include "group.h"
#include "domain.h"
#include "comm.h"
#include "update.h"
#include "error.h"
#include "memory.h"

using namespace LAMMPS_NS;

enum{NSQ,BIN,MULTI};       // also in Neighbor

#define SMALL 1.0e-6
#define CUT2BIN_RATIO 100
#define MAXBINCONTENT 100  //used to bound local memory
#define EXPAND 100  

/* ---------------------------------------------------------------------- */

NBinCAC::NBinCAC(LAMMPS *lmp) : NBin(lmp) {
  memory->create(bin_overlap_limits,6,"nbin_CAC:bin_overlap_limits");
  bin_ncontent=NULL;
  bin_content=NULL;
  quad2bin=NULL;
  atom2bin=NULL;
  bin_expansion_counts=NULL;
  first_alloc=0;
  max_bin_expansion_count=0;
  quad_rule_initialized=0;
  nmax=0;
  surface_counts = NULL;
	interior_scales = NULL;
  current_element_quad_points=NULL;
  //quad_allocated = 0;
  surface_counts_max[0] = 0;
  surface_counts_max[1] = 0;
  surface_counts_max[2] = 0;
  surface_counts_max_old[0] = 0;
  surface_counts_max_old[1] = 0;
  surface_counts_max_old[2] = 0;
	setup_called=0;
}

/* ---------------------------------------------------------------------- */

NBinCAC::~NBinCAC() {
  memory->destroy(bin_ncontent);
  memory->destroy(quad2bin);
  memory->destroy(bin_expansion_counts);
	
  for(int i=0 ; i<maxbin; i++)
  memory->destroy(bin_content[i]);
  memory->sfree(bin_content);
	memory->destroy(current_element_quad_points);
  memory->destroy(surface_counts);
	memory->destroy(interior_scales);
}

/* ----------------------------------------------------------------------
   setup for bin_atoms()
------------------------------------------------------------------------- */

void NBinCAC::bin_atoms_setup(int nall)
{
	if(!setup_called)
	setup_bins(1);
  int *element_type = atom->element_type;
	int *poly_count = atom->poly_count;
  int **element_scale = atom->element_scale;
  int current_element_type;
  int neighbor_element_type;
  double ****nodal_positions = atom->nodal_positions;
  double interior_scale[3];
  //check if quadrature rules were initialized
  if(quad_rule_initialized==0)quadrature_init(2);

  // binhead = per-bin vector, mbins in length
  // add 1 bin for USER-INTEL package
  
 if (mbins > maxbin) {
    //compute maximum expansion count used in previous allocations
    
    
    if(!first_alloc){
		first_alloc=1;
		memory->grow(bin_expansion_counts,mbins,"neigh:bin_ncontent");
		  for (int init = 0; init < mbins; init++){
      bin_expansion_counts[init]=max_bin_expansion_count;
    }
    bin_content= (int **) memory->smalloc(mbins*sizeof(int *), "bin_CAC:bin_content");
    for (int init = 0; init < mbins; init++) 
    memory->create(bin_content[init],MAXBINCONTENT,"neigh:bin_content");
    }
    else{
		for (int init = 0; init < maxbin; init++){
      if(bin_expansion_counts[init]>max_bin_expansion_count) max_bin_expansion_count=bin_expansion_counts[init];
    }
    bin_content= (int **) memory->srealloc(bin_content,mbins*sizeof(int *), "bin_CAC:bin_content");
    for (int init = 0; init < mbins; init++){
    if(init>=maxbin) bin_content[init]=NULL;  
    memory->grow(bin_content[init],MAXBINCONTENT+max_bin_expansion_count*EXPAND,"neigh:bin_content");
    }
    memory->grow(bin_expansion_counts,mbins,"neigh:bin_ncontent");
		for (int init = 0; init < mbins; init++)
      bin_expansion_counts[init]=max_bin_expansion_count;
     
    }
    maxbin = mbins;
    memory->grow(bin_ncontent,maxbin,"neigh:bin_ncontent");
   
    //initialize number of times bin memory sizes have been expanded
    
  }

  // bins and atom2bin = per-atom vectors
  // for both local and ghost atoms
 //allocate quad2bin in next patch
  if (nall > maxatom) {
    maxatom = nall;
    memory->destroy(bins);
    memory->create(bins,maxatom,"neigh:bins");
    memory->destroy(atom2bin);
    memory->create(atom2bin,maxatom,"neigh:atom2bin");
  }


// initialize or grow surface counts array for quadrature scheme
			// along with interior scaling for the quadrature domain
			if (atom->nlocal  > nmax) {
				allocate_surface_counts();
			}
			surface_counts_max_old[0] = surface_counts_max[0];
			surface_counts_max_old[1] = surface_counts_max[1];
			surface_counts_max_old[2] = surface_counts_max[2];
			//atomic_counter = 0;
			for (int i = 0; i < atom->nlocal; i++) {
			current_nodal_positions = nodal_positions[i];
			current_element_scale[0] = element_scale[i][0];
			current_element_scale[1] = element_scale[i][1];
			current_element_scale[2] = element_scale[i][2];	
				//if (current_element_type == 0) atomic_counter += 1;
				if (element_type[i] != 0) {
					
					for (current_poly_counter = 0; current_poly_counter < poly_count[i]; current_poly_counter++) {
						int poly_surface_count[3];
						compute_surface_depths(interior_scale[0], interior_scale[1], interior_scale[2],
							poly_surface_count[0], poly_surface_count[1], poly_surface_count[2], 1);
						if(current_poly_counter==0){
								surface_counts[i][0]=poly_surface_count[0];
								surface_counts[i][1]=poly_surface_count[1];
								surface_counts[i][2]=poly_surface_count[2];
								interior_scales[i][0]=interior_scale[0];
								interior_scales[i][1]=interior_scale[1];
								interior_scales[i][2]=interior_scale[2];
						}
						else{
						if (poly_surface_count[0] > surface_counts[i][0]){ surface_counts[i][0] = poly_surface_count[0];
																		      interior_scales[i][0]=interior_scale[0]; }
						if (poly_surface_count[1] > surface_counts[i][1]){ surface_counts[i][1] = poly_surface_count[1];
																			  interior_scales[i][1]=interior_scale[1]; }
						if (poly_surface_count[2] > surface_counts[i][2]){ surface_counts[i][2] = poly_surface_count[2];
						 													  interior_scales[i][2]=interior_scale[2]; }
						}
					
					}
						if (surface_counts[i][0] > surface_counts_max[0]) surface_counts_max[0] = surface_counts[i][0];
						if (surface_counts[i][1] > surface_counts_max[1]) surface_counts_max[1] = surface_counts[i][1];
						if (surface_counts[i][2] > surface_counts_max[2]) surface_counts_max[2] = surface_counts[i][2];
				}
			}
     //size the memory storage for the quadrature points belonging to each element 
     //according to the maximum # of quad/element
     int n1= surface_counts_max[0];
     int n2= surface_counts_max[1];
     int n3= surface_counts_max[2];
     int quad= quadrature_node_count;
     	int max_quad_count = quad*quad*quad + 2 * n1*quad*quad + 2 * n2*quad*quad +
		+2 * n3*quad*quad + 4 * n1*n2*quad + 4 * n3*n2*quad + 4 * n1*n3*quad
		+ 8 * n1*n2*n3;
		//grow array used to store the set of quadrature points for each element at a time
		memory->grow(current_element_quad_points,max_quad_count*atom->maxpoly,3,"NBinCAC:current_element_quad_points");

    //compute number of quadrature points needed for the size of quad2bin
    int quadrature_point_count=0;
    int c1,c2,c3;
    int current_quad_count;
    		for (int init = 0; init < atom->nlocal; init++) {
			
			if (element_type[init] == 0){ 
				quadrature_point_count+=1;
      }
			else {
				c1=surface_counts[init][0];
				c2=surface_counts[init][1];
				c3=surface_counts[init][2];
				current_quad_count = quad*quad*quad + 2 * c1*quad*quad + 2 * c2*quad*quad +
		    +2 * c3*quad*quad + 4 * c1*c2*quad + 4 * c3*c2*quad + 4 * c1*c3*quad
		     + 8 * c1*c2*c3;
				quadrature_point_count+=current_quad_count*poly_count[init];
				
			}
		}
    memory->grow(quad2bin,quadrature_point_count,"NBinCAC:quad2bin");
		setup_called=0;
}

/* ----------------------------------------------------------------------
   setup neighbor binning geometry
   bin numbering in each dimension is global:
     0 = 0.0 to binsize, 1 = binsize to 2*binsize, etc
     nbin-1,nbin,etc = bbox-binsize to bbox, bbox to bbox+binsize, etc
     -1,-2,etc = -binsize to 0.0, -2*binsize to -binsize, etc
   code will work for any binsize
     since next(xyz) and stencil extend as far as necessary
     binsize = 1/2 of cutoff is roughly optimal
   for orthogonal boxes:
     a dim must be filled exactly by integer # of bins
     in periodic, procs on both sides of PBC must see same bin boundary
     in non-periodic, coord2bin() still assumes this by use of nbin xyz
   for triclinic boxes:
     tilted simulation box cannot contain integer # of bins
     stencil & neigh list built differently to account for this
   mbinlo = lowest global bin any of my ghost atoms could fall into
   mbinhi = highest global bin any of my ghost atoms could fall into
   mbin = number of bins I need in a dimension
------------------------------------------------------------------------- */

void NBinCAC::setup_bins(int style)
{
  // bbox = size of bbox of entire domain
  // bsubbox lo/hi = bounding box of my subdomain extended by comm->cutghost
  // for triclinic:
  //   bbox bounds all 8 corners of tilted box
  //   subdomain is in lamda coords
  //   include dimension-dependent extension via comm->cutghost
  //   domain->bbox() converts lamda extent to box coords and computes bbox

  double bbox[3],bsubboxlo[3],bsubboxhi[3];
  double *cutghost = comm->cutghost;
	double lamda_temp[3];
  double nodal_temp[3];
	double ***nodal_positions;
	double shape_func;
	int *element_type = atom->element_type;
	int *poly_count = atom->poly_count;
	double xtmp, ytmp, ztmp, delx, dely, delz, rsq;
	double ebounding_boxlo[3];
	double ebounding_boxhi[3];
  double CAC_cut= atom->CAC_cut;
  double CAC_skin= atom->CAC_skin;
  CAC_cut=CAC_cut+CAC_skin;
	int *nodes_per_element_list = atom->nodes_per_element_list;
	double max_search_range=atom->max_search_range;
	double cut_max=neighbor->cutneighmax;
  setup_called=1;
  //expand sub-box sizes to contain all element bounding boxes
	//in general initial box wont since only x, element centroid, must lie 
	//inside box for lammps comms

  if (triclinic == 0) {
		bsubboxlo[0] = domain->sublo[0];
    bsubboxlo[1] = domain->sublo[1];
    bsubboxlo[2] = domain->sublo[2];
    bsubboxhi[0] = domain->subhi[0];
    bsubboxhi[1] = domain->subhi[1];
    bsubboxhi[2] = domain->subhi[2];
		//loop through elements to compute bounding boxes and test
	//whether they should stretch the local bounding box
	for(int element_index=0; element_index < atom->nlocal; element_index++){
	if(element_type[element_index]){
   nodal_positions = atom->nodal_positions[element_index];
	//int current_poly_count = poly_count[element_index];
    int current_poly_count = poly_count[element_index];
	int nodes_per_element = nodes_per_element_list[element_type[element_index]];
 



	//initialize bounding box values
	ebounding_boxlo[0] = nodal_positions[0][0][0];
	ebounding_boxlo[1] = nodal_positions[0][0][1];
	ebounding_boxlo[2] = nodal_positions[0][0][2];
	ebounding_boxhi[0] = nodal_positions[0][0][0];
	ebounding_boxhi[1] = nodal_positions[0][0][1];
	ebounding_boxhi[2] = nodal_positions[0][0][2];
     //define the bounding box for the element being considered as a neighbor
	
	for (int poly_counter = 0; poly_counter < current_poly_count; poly_counter++) {
		for (int kkk = 0; kkk < nodes_per_element; kkk++) {
			for (int dim = 0; dim < 3; dim++) {
				if (nodal_positions[kkk][poly_counter][dim] < ebounding_boxlo[dim]) {
					ebounding_boxlo[dim] = nodal_positions[kkk][poly_counter][dim];
				}
				if (nodal_positions[kkk][poly_counter][dim] > ebounding_boxhi[dim]) {
					ebounding_boxhi[dim] = nodal_positions[kkk][poly_counter][dim];
				}
			}
		}
	}
	//test if this bounding box exceeds local sub box
	for(int dim=0; dim < dimension; dim++){
	if(ebounding_boxhi[dim]>bsubboxhi[dim])
	bsubboxhi[dim]=ebounding_boxhi[dim];
	if(ebounding_boxlo[dim]<bsubboxlo[dim])
	bsubboxlo[dim]=ebounding_boxlo[dim];
	}
	}
	}
		
    bsubboxlo[0] -= max_search_range+cutghost[0];
    bsubboxlo[1] -= max_search_range+cutghost[1];
    bsubboxlo[2] -= max_search_range+cutghost[2];
    bsubboxhi[0] += max_search_range+cutghost[0];
    bsubboxhi[1] += max_search_range+cutghost[1];
    bsubboxhi[2] += max_search_range+cutghost[2];
		
  } else {
	double lo[3],hi[3];
	lo[0] = domain->sublo_lamda[0];
  lo[1] = domain->sublo_lamda[1];
  lo[2] = domain->sublo_lamda[2];
  hi[0] = domain->subhi_lamda[0];
  hi[1] = domain->subhi_lamda[1];
  hi[2] = domain->subhi_lamda[2];
	//loop through elements to compute bounding boxes and test
	//whether they should stretch the local bounding box
	for(int element_index=0; element_index < atom->nlocal; element_index++){
	if(element_type[element_index]){
   nodal_positions = atom->nodal_positions[element_index];
	//int current_poly_count = poly_count[element_index];
    int current_poly_count = poly_count[element_index];
	int nodes_per_element = nodes_per_element_list[element_type[element_index]];
 



	//initialize bounding box values
	ebounding_boxlo[0] = nodal_positions[0][0][0];
	ebounding_boxlo[1] = nodal_positions[0][0][1];
	ebounding_boxlo[2] = nodal_positions[0][0][2];
	ebounding_boxhi[0] = nodal_positions[0][0][0];
	ebounding_boxhi[1] = nodal_positions[0][0][1];
	ebounding_boxhi[2] = nodal_positions[0][0][2];
     //define the bounding box for the element being considered as a neighbor
	
	for (int poly_counter = 0; poly_counter < current_poly_count; poly_counter++) {
		for (int kkk = 0; kkk < nodes_per_element; kkk++) {
			
		nodal_temp[0]=nodal_positions[kkk][poly_counter][0];
		nodal_temp[1]=nodal_positions[kkk][poly_counter][1];
		nodal_temp[2]=nodal_positions[kkk][poly_counter][2];
		domain->x2lamda(nodal_temp, lamda_temp);
			//test if this node lies outside local box and stretch box
		for(int dim=0; dim < dimension; dim++){
	   if(lamda_temp[dim]>hi[dim])
	   hi[dim]=lamda_temp[dim];
	   if(lamda_temp[dim]<lo[dim])
	   lo[dim]=lamda_temp[dim];
	  }
		}
	}

	}
	}
		
    lo[0] -= cutghost[0]*(max_search_range/cut_max+1);
    lo[1] -= cutghost[1]*(max_search_range/cut_max+1);
    lo[2] -= cutghost[2]*(max_search_range/cut_max+1);
    hi[0] += cutghost[0]*(max_search_range/cut_max+1);
    hi[1] += cutghost[1]*(max_search_range/cut_max+1);
    hi[2] += cutghost[2]*(max_search_range/cut_max+1);
    domain->bbox(lo,hi,bsubboxlo,bsubboxhi);
  }
  
  

	bbox[0] = bboxhi[0] - bboxlo[0];
  bbox[1] = bboxhi[1] - bboxlo[1];
  bbox[2] = bboxhi[2] - bboxlo[2];
  // optimal bin size is roughly 1/2 the cutoff
  // for BIN style, binsize = 1/2 of max neighbor cutoff
  // for MULTI style, binsize = 1/2 of min neighbor cutoff
  // special case of all cutoffs = 0.0, binsize = box size

  double binsize_optimal;
  if (binsizeflag) binsize_optimal = binsize_user;
  else if (style == BIN) binsize_optimal = 0.5*cutneighmax;
  else binsize_optimal = 0.5*cutneighmin;
  if (binsize_optimal == 0.0) binsize_optimal = bbox[0];
	binsize_optimal=0.5*max_search_range;
  double binsizeinv = 1.0/binsize_optimal;

  // test for too many global bins in any dimension due to huge global domain

  if (bbox[0]*binsizeinv > MAXSMALLINT || bbox[1]*binsizeinv > MAXSMALLINT ||
      bbox[2]*binsizeinv > MAXSMALLINT)
    error->all(FLERR,"Domain too large for neighbor bins");

  // create actual bins
  // always have one bin even if cutoff > bbox
  // for 2d, nbinz = 1

  nbinx = static_cast<int> (bbox[0]*binsizeinv);
  nbiny = static_cast<int> (bbox[1]*binsizeinv);
  if (dimension == 3) nbinz = static_cast<int> (bbox[2]*binsizeinv);
  else nbinz = 1;

  if (nbinx == 0) nbinx = 1;
  if (nbiny == 0) nbiny = 1;
  if (nbinz == 0) nbinz = 1;

  // compute actual bin size for nbins to fit into box exactly
  // error if actual bin size << cutoff, since will create a zillion bins
  // this happens when nbin = 1 and box size << cutoff
  // typically due to non-periodic, flat system in a particular dim
  // in that extreme case, should use NSQ not BIN neighbor style

  binsizex = bbox[0]/nbinx;
  binsizey = bbox[1]/nbiny;
  binsizez = bbox[2]/nbinz;

  bininvx = 1.0 / binsizex;
  bininvy = 1.0 / binsizey;
  bininvz = 1.0 / binsizez;

  if (binsize_optimal*bininvx > CUT2BIN_RATIO ||
      binsize_optimal*bininvy > CUT2BIN_RATIO ||
      binsize_optimal*bininvz > CUT2BIN_RATIO)
    error->all(FLERR,"Cannot use neighbor bins - box size << cutoff");

  // mbinlo/hi = lowest and highest global bins my ghost atoms could be in
  // coord = lowest and highest values of coords for my ghost atoms
  // static_cast(-1.5) = -1, so subract additional -1
  // add in SMALL for round-off safety

  int mbinxhi,mbinyhi,mbinzhi;
  double coord;

  coord = bsubboxlo[0] - SMALL*bbox[0];
  mbinxlo = static_cast<int> ((coord-bboxlo[0])*bininvx);
  if (coord < bboxlo[0]) mbinxlo = mbinxlo - 1;
  coord = bsubboxhi[0] + SMALL*bbox[0];
  mbinxhi = static_cast<int> ((coord-bboxlo[0])*bininvx);

  coord = bsubboxlo[1] - SMALL*bbox[1];
  mbinylo = static_cast<int> ((coord-bboxlo[1])*bininvy);
  if (coord < bboxlo[1]) mbinylo = mbinylo - 1;
  coord = bsubboxhi[1] + SMALL*bbox[1];
  mbinyhi = static_cast<int> ((coord-bboxlo[1])*bininvy);

  if (dimension == 3) {
    coord = bsubboxlo[2] - SMALL*bbox[2];
    mbinzlo = static_cast<int> ((coord-bboxlo[2])*bininvz);
    if (coord < bboxlo[2]) mbinzlo = mbinzlo - 1;
    coord = bsubboxhi[2] + SMALL*bbox[2];
    mbinzhi = static_cast<int> ((coord-bboxlo[2])*bininvz);
  }

  // extend bins by 1 to insure stencil extent is included
  // for 2d, only 1 bin in z

  mbinxlo = mbinxlo - 1;
  mbinxhi = mbinxhi + 1;
  mbinx = mbinxhi - mbinxlo + 1;

  mbinylo = mbinylo - 1;
  mbinyhi = mbinyhi + 1;
  mbiny = mbinyhi - mbinylo + 1;

  if (dimension == 3) {
    mbinzlo = mbinzlo - 1;
    mbinzhi = mbinzhi + 1;
  } else mbinzlo = mbinzhi = 0;
  mbinz = mbinzhi - mbinzlo + 1;

  bigint bbin = ((bigint) mbinx) * ((bigint) mbiny) * ((bigint) mbinz) + 1;
  if (bbin > MAXSMALLINT) error->one(FLERR,"Too many neighbor bins");
  mbins = bbin;
}

/* ----------------------------------------------------------------------
   bin owned and ghost atoms
------------------------------------------------------------------------- */

void NBinCAC::bin_atoms()
{
  int i,ibin;

  last_bin = update->ntimestep;
  //for (i = 0; i < mbins; i++) binhead[i] = -1;
  //initialize bin counts
  for(int init=0; init<mbins; init++)
  bin_ncontent[init]=0;
  // bin in reverse order so linked list will be in forward order
  // also puts ghost atoms at end of list, which is necessary

  double **x = atom->x;
	int **element_scale = atom->element_scale;
  int *mask = atom->mask;
  int nlocal = atom->nlocal;
  int nall = nlocal + atom->nghost;
  int noverlap;
  int current_bin_ncount;
  int *element_type= atom->element_type;
  int current_bin;
  int qi=0;
  int quadrature_count;

  if (includegroup) {
    qi=0;
    int bitmask = group->bitmask[includegroup];
    for (i = nlocal; i < nall; i++) {
      if (mask[i] & bitmask) {
			int current_element_type = element_type[i];
	    current_element_scale[0] = element_scale[i][0];
	    current_element_scale[1] = element_scale[i][1];
	    current_element_scale[2] = element_scale[i][2];
 
  
		//find the current quadrature points of this element
		if(i<atom->nlocal){
		if(current_element_type!=0){
		quadrature_count=compute_quad_points(i);
		}
		else{
    quadrature_count=1;
    current_element_quad_points[0][0]=x[i][0];
		current_element_quad_points[0][1]=x[i][1];
		current_element_quad_points[0][2]=x[i][2];
		}
      //bin this element's quadrature points and assign to quad2bin
      for (int iquad = 0; iquad < quadrature_count; iquad++) {
      ibin = quad2bins(current_element_quad_points[iquad]);
      quad2bin[qi] = ibin;
      qi++;
      }
		}	
         //returns the set of bins an elements bounding box overlaps
      //atoms are binned typically
      ibin = element2bins(i);
      atom2bin[i] = ibin;
      if(element_type[i]==0){
            current_bin_ncount=bin_ncontent[ibin];
            if(bin_ncontent[ibin]==MAXBINCONTENT+bin_expansion_counts[ibin]*EXPAND){
            bin_expansion_counts[ibin]++;
            memory->grow(bin_content[ibin],MAXBINCONTENT+bin_expansion_counts[ibin]*EXPAND,"neigh:bin_content grow");
            }
            bin_content[ibin][current_bin_ncount] = i;
            bin_ncontent[ibin]++;
      }
      else{
      for(int overlapx=bin_overlap_limits[0]; overlapx<=bin_overlap_limits[3]; overlapx++){
        for(int overlapy=bin_overlap_limits[1]; overlapy<=bin_overlap_limits[4]; overlapy++){
          for(int overlapz=bin_overlap_limits[2]; overlapz<=bin_overlap_limits[5]; overlapz++){
            current_bin=(overlapz-mbinzlo)*mbiny*mbinx + (overlapy-mbinylo)*mbinx + (overlapx-mbinxlo);
            current_bin_ncount=bin_ncontent[current_bin];
            //check bin memory allocation is large enough and grow if needed
            if(bin_ncontent[current_bin]==MAXBINCONTENT+bin_expansion_counts[current_bin]*EXPAND){
            bin_expansion_counts[current_bin]++;
            memory->grow(bin_content[current_bin],MAXBINCONTENT+bin_expansion_counts[current_bin]*EXPAND,"neigh:bin_content grow");
            }
            bin_content[current_bin][current_bin_ncount] = i;
            bin_ncontent[current_bin]++;
          }
        }
      }
      }
      }
    }
    for (i = 0; i < atom->nfirst; i++) {

		int current_element_type = element_type[i];
	  current_element_scale[0] = element_scale[i][0];
	  current_element_scale[1] = element_scale[i][1];
	  current_element_scale[2] = element_scale[i][2];
 
  
		//find the current quadrature points of this element
		if(i<atom->nlocal){
		if(current_element_type!=0){
		quadrature_count=compute_quad_points(i);
		}
		else{
    quadrature_count=1;
    current_element_quad_points[0][0]=x[i][0];
		current_element_quad_points[0][1]=x[i][1];
		current_element_quad_points[0][2]=x[i][2];
		}
      //bin this element's quadrature points and assign to quad2bin
      for (int iquad = 0; iquad < quadrature_count; iquad++) {
      ibin = quad2bins(current_element_quad_points[iquad]);
      quad2bin[qi] = ibin;
      qi++;
      }
		}
      //returns the set of bins an elements bounding box overlaps
      //atoms are binned typically
      ibin = element2bins(i);
      atom2bin[i] = ibin;
      if(element_type[i]==0){
            current_bin_ncount=bin_ncontent[ibin];
            if(bin_ncontent[ibin]==MAXBINCONTENT+bin_expansion_counts[ibin]*EXPAND){
            bin_expansion_counts[ibin]++;
            memory->grow(bin_content[ibin],MAXBINCONTENT+bin_expansion_counts[ibin]*EXPAND,"neigh:bin_content grow");
            }
            bin_content[ibin][current_bin_ncount] = i;
            bin_ncontent[ibin]++;
      }
      else{
      for(int overlapx=bin_overlap_limits[0]; overlapx<=bin_overlap_limits[3]; overlapx++){
        for(int overlapy=bin_overlap_limits[1]; overlapy<=bin_overlap_limits[4]; overlapy++){
          for(int overlapz=bin_overlap_limits[2]; overlapz<=bin_overlap_limits[5]; overlapz++){
            current_bin=(overlapz-mbinzlo)*mbiny*mbinx + (overlapy-mbinylo)*mbinx + (overlapx-mbinxlo);
            current_bin_ncount=bin_ncontent[current_bin];
            //check bin memory allocation is large enough and grow if needed
            if(bin_ncontent[current_bin]==MAXBINCONTENT+bin_expansion_counts[current_bin]*EXPAND){
            bin_expansion_counts[current_bin]++;
            memory->grow(bin_content[current_bin],MAXBINCONTENT+bin_expansion_counts[current_bin]*EXPAND,"neigh:bin_content grow");
            }
            bin_content[current_bin][current_bin_ncount] = i;
            bin_ncontent[current_bin]++;
          }
        }
      }
      }
    }

  } else {
    qi=0;
    for (i = 0; i < nall; i++) {
      //computes the quadrature point locations for this ith element and returns the # of points
  int current_element_type = element_type[i];
	current_element_scale[0] = element_scale[i][0];
	current_element_scale[1] = element_scale[i][1];
	current_element_scale[2] = element_scale[i][2];
 
  
		//find the current quadrature points of this element
		if(i<atom->nlocal){
		if(current_element_type!=0){
		quadrature_count=compute_quad_points(i);
		}
		else{
    quadrature_count=1;
    current_element_quad_points[0][0]=x[i][0];
		current_element_quad_points[0][1]=x[i][1];
		current_element_quad_points[0][2]=x[i][2];
		}
      //bin this element's quadrature points and assign to quad2bin
      for (int iquad = 0; iquad < quadrature_count; iquad++) {
      ibin = quad2bins(current_element_quad_points[iquad]);
      quad2bin[qi] = ibin;
      qi++;
      }
		}
      //returns the set of bins an elements bounding box overlaps
      //atoms are binned typically
      ibin = element2bins(i);
      
      if(element_type[i]==0){
            current_bin_ncount=bin_ncontent[ibin];
            if(bin_ncontent[ibin]==MAXBINCONTENT+bin_expansion_counts[ibin]*EXPAND){
            bin_expansion_counts[ibin]++;
            memory->grow(bin_content[ibin],MAXBINCONTENT+bin_expansion_counts[ibin]*EXPAND,"neigh:bin_content grow");
            }
            bin_content[ibin][current_bin_ncount] = i;
            bin_ncontent[ibin]++;
      }
      if(element_type[i]!=0){
      for(int overlapx=bin_overlap_limits[0]; overlapx<=bin_overlap_limits[3]; overlapx++){
        for(int overlapy=bin_overlap_limits[1]; overlapy<=bin_overlap_limits[4]; overlapy++){
          for(int overlapz=bin_overlap_limits[2]; overlapz<=bin_overlap_limits[5]; overlapz++){
            current_bin=(overlapz-mbinzlo)*mbiny*mbinx + (overlapy-mbinylo)*mbinx + (overlapx-mbinxlo);
            current_bin_ncount=bin_ncontent[current_bin];
            //check bin memory allocation is large enough and grow if needed
            if(bin_ncontent[current_bin]==MAXBINCONTENT+bin_expansion_counts[current_bin]*EXPAND){
            bin_expansion_counts[current_bin]++;
            memory->grow(bin_content[current_bin],MAXBINCONTENT+bin_expansion_counts[current_bin]*EXPAND,"neigh:bin_content grow");
            }
            bin_content[current_bin][current_bin_ncount] = i;
            bin_ncontent[current_bin]++;
          }
        }
      }
      }
    }
  }
}
/* ----------------------------------------------------------------------
   convert atom and CAC element coords into local bin #
   for orthogonal, only ghost atoms will have coord >= bboxhi or coord < bboxlo
     take special care to insure ghosts are in correct bins even w/ roundoff
     hi ghost atoms = nbin,nbin+1,etc
     owned atoms = 0 to nbin-1
     lo ghost atoms = -1,-2,etc
     this is necessary so that both procs on either side of PBC
       treat a pair of atoms straddling the PBC in a consistent way
   for triclinic, doesn't matter since stencil & neigh list built differently
------------------------------------------------------------------------- */

 int NBinCAC::quad2bins(double *x)
{

 

  int ix,iy,iz;
  
  //typical binning for atoms
  
  if (!ISFINITE(x[0]) || !ISFINITE(x[1]) || !ISFINITE(x[2]))
    error->one(FLERR,"Non-numeric positions - simulation unstable");

  if (x[0] >= bboxhi[0])
    ix = static_cast<int> ((x[0]-bboxhi[0])*bininvx) + nbinx;
  else if (x[0] >= bboxlo[0]) {
    ix = static_cast<int> ((x[0]-bboxlo[0])*bininvx);
    ix = MIN(ix,nbinx-1);
  } else
    ix = static_cast<int> ((x[0]-bboxlo[0])*bininvx) - 1;

  if (x[1] >= bboxhi[1])
    iy = static_cast<int> ((x[1]-bboxhi[1])*bininvy) + nbiny;
  else if (x[1] >= bboxlo[1]) {
    iy = static_cast<int> ((x[1]-bboxlo[1])*bininvy);
    iy = MIN(iy,nbiny-1);
  } else
    iy = static_cast<int> ((x[1]-bboxlo[1])*bininvy) - 1;

  if (x[2] >= bboxhi[2])
    iz = static_cast<int> ((x[2]-bboxhi[2])*bininvz) + nbinz;
  else if (x[2] >= bboxlo[2]) {
    iz = static_cast<int> ((x[2]-bboxlo[2])*bininvz);
    iz = MIN(iz,nbinz-1);
  } else
    iz = static_cast<int> ((x[2]-bboxlo[2])*bininvz) - 1;


  //return the bin id of this quadrature point
  
  return (iz-mbinzlo)*mbiny*mbinx + (iy-mbinylo)*mbinx + (ix-mbinxlo);
}
/* ----------------------------------------------------------------------
   convert atom and CAC element coords into local bin #
   for orthogonal, only ghost atoms will have coord >= bboxhi or coord < bboxlo
     take special care to insure ghosts are in correct bins even w/ roundoff
     hi ghost atoms = nbin,nbin+1,etc
     owned atoms = 0 to nbin-1
     lo ghost atoms = -1,-2,etc
     this is necessary so that both procs on either side of PBC
       treat a pair of atoms straddling the PBC in a consistent way
   for triclinic, doesn't matter since stencil & neigh list built differently
------------------------------------------------------------------------- */

 int NBinCAC::element2bins(int element_index)
{

  double *x = atom->x[element_index];

	
	double ***nodal_positions = atom->nodal_positions[element_index];
	double shape_func;
	int *element_type = atom->element_type;
	int *poly_count = atom->poly_count;
	double xtmp, ytmp, ztmp, delx, dely, delz, rsq;
	double bounding_boxlo[3];
	double bounding_boxhi[3];
  double CAC_cut= atom->CAC_cut;
  double CAC_skin= atom->CAC_skin;
  CAC_cut=CAC_cut+CAC_skin;
	int *nodes_per_element_list = atom->nodes_per_element_list;
  int ix,iy,iz;
  int ixl,iyl,izl,ixh,iyh,izh;
  //typical binning for atoms
  
  if (!ISFINITE(x[0]) || !ISFINITE(x[1]) || !ISFINITE(x[2]))
    error->one(FLERR,"Non-numeric positions - simulation unstable");

  if (x[0] >= bboxhi[0])
    ix = static_cast<int> ((x[0]-bboxhi[0])*bininvx) + nbinx;
  else if (x[0] >= bboxlo[0]) {
    ix = static_cast<int> ((x[0]-bboxlo[0])*bininvx);
    ix = MIN(ix,nbinx-1);
  } else
    ix = static_cast<int> ((x[0]-bboxlo[0])*bininvx) - 1;

  if (x[1] >= bboxhi[1])
    iy = static_cast<int> ((x[1]-bboxhi[1])*bininvy) + nbiny;
  else if (x[1] >= bboxlo[1]) {
    iy = static_cast<int> ((x[1]-bboxlo[1])*bininvy);
    iy = MIN(iy,nbiny-1);
  } else
    iy = static_cast<int> ((x[1]-bboxlo[1])*bininvy) - 1;

  if (x[2] >= bboxhi[2])
    iz = static_cast<int> ((x[2]-bboxhi[2])*bininvz) + nbinz;
  else if (x[2] >= bboxlo[2]) {
    iz = static_cast<int> ((x[2]-bboxlo[2])*bininvz);
    iz = MIN(iz,nbinz-1);
  } else
    iz = static_cast<int> ((x[2]-bboxlo[2])*bininvz) - 1;

  
  

  //calculate the set of bins this element's bounding box overlaps
  if(element_type[element_index]){
    
	//int current_poly_count = poly_count[element_index];
    int current_poly_count = poly_count[element_index];
	int nodes_per_element = nodes_per_element_list[element_type[element_index]];
 



	//initialize bounding box values
	bounding_boxlo[0] = nodal_positions[0][0][0];
	bounding_boxlo[1] = nodal_positions[0][0][1];
	bounding_boxlo[2] = nodal_positions[0][0][2];
	bounding_boxhi[0] = nodal_positions[0][0][0];
	bounding_boxhi[1] = nodal_positions[0][0][1];
	bounding_boxhi[2] = nodal_positions[0][0][2];
     //define the bounding box for the element being considered as a neighbor
	
	for (int poly_counter = 0; poly_counter < current_poly_count; poly_counter++) {
		for (int kkk = 0; kkk < nodes_per_element; kkk++) {
			for (int dim = 0; dim < 3; dim++) {
				if (nodal_positions[kkk][poly_counter][dim] < bounding_boxlo[dim]) {
					bounding_boxlo[dim] = nodal_positions[kkk][poly_counter][dim];
				}
				if (nodal_positions[kkk][poly_counter][dim] > bounding_boxhi[dim]) {
					bounding_boxhi[dim] = nodal_positions[kkk][poly_counter][dim];
				}
			}
		}
	}

	bounding_boxlo[0] -= CAC_cut;
	bounding_boxlo[1] -= CAC_cut;
	bounding_boxlo[2] -= CAC_cut;
	bounding_boxhi[0] += CAC_cut;
	bounding_boxhi[1] += CAC_cut;
	bounding_boxhi[2] += CAC_cut;
    
    //compute lowest overlap id for each dimension
    if (bounding_boxlo[0] >= bboxhi[0])
    ixl = static_cast<int> ((bounding_boxlo[0]-bboxhi[0])*bininvx) + nbinx;
  else if (bounding_boxlo[0] >= bboxlo[0]) {
    ixl = static_cast<int> ((bounding_boxlo[0]-bboxlo[0])*bininvx);
    ixl = MIN(ixl,nbinx-1);
  } else
    ixl = static_cast<int> ((bounding_boxlo[0]-bboxlo[0])*bininvx) - 1;

  if (bounding_boxlo[1] >= bboxhi[1])
    iyl = static_cast<int> ((bounding_boxlo[1]-bboxhi[1])*bininvy) + nbiny;
  else if (bounding_boxlo[1] >= bboxlo[1]) {
    iyl = static_cast<int> ((bounding_boxlo[1]-bboxlo[1])*bininvy);
    iyl = MIN(iyl,nbiny-1);
  } else
    iyl = static_cast<int> ((bounding_boxlo[1]-bboxlo[1])*bininvy) - 1;

  if (bounding_boxlo[2] >= bboxhi[2])
    izl = static_cast<int> ((bounding_boxlo[2]-bboxhi[2])*bininvz) + nbinz;
  else if (bounding_boxlo[2] >= bboxlo[2]) {
    izl = static_cast<int> ((bounding_boxlo[2]-bboxlo[2])*bininvz);
    izl = MIN(izl,nbinz-1);
  } else
    izl = static_cast<int> ((bounding_boxlo[2]-bboxlo[2])*bininvz) - 1;
 
  //compute highest overlap id for each dimension
   if (bounding_boxhi[0] >= bboxhi[0])
    ixh = static_cast<int> ((bounding_boxhi[0]-bboxhi[0])*bininvx) + nbinx;
  else if (bounding_boxhi[0] >= bboxlo[0]) {
    ixh = static_cast<int> ((bounding_boxhi[0]-bboxlo[0])*bininvx);
    ixh = MIN(ixh,nbinx-1);
  } else
    ixh = static_cast<int> ((bounding_boxhi[0]-bboxlo[0])*bininvx) - 1;

  if (bounding_boxhi[1] >= bboxhi[1])
    iyh = static_cast<int> ((bounding_boxhi[1]-bboxhi[1])*bininvy) + nbiny;
  else if (bounding_boxhi[1] >= bboxlo[1]) {
    iyh = static_cast<int> ((bounding_boxhi[1]-bboxlo[1])*bininvy);
    iyh = MIN(iyh,nbiny-1);
  } else
    iyh = static_cast<int> ((bounding_boxhi[1]-bboxlo[1])*bininvy) - 1;

  if (bounding_boxhi[2] >= bboxhi[2])
    izh = static_cast<int> ((bounding_boxhi[2]-bboxhi[2])*bininvz) + nbinz;
  else if (bounding_boxhi[2] >= bboxlo[2]) {
    izh = static_cast<int> ((bounding_boxhi[2]-bboxlo[2])*bininvz);
    izh = MIN(izh,nbinz-1);
  } else
    izh = static_cast<int> ((bounding_boxhi[2]-bboxlo[2])*bininvz) - 1;
  
  bin_overlap_limits[0]=ixl;
  bin_overlap_limits[1]=iyl;
  bin_overlap_limits[2]=izl;
  bin_overlap_limits[3]=ixl;
  bin_overlap_limits[4]=iyl;
  bin_overlap_limits[5]=izl;

  //return (iz-mbinzlo)*mbiny*mbinx + (iy-mbinylo)*mbinx + (ix-mbinxlo);

  }
  return (iz-mbinzlo)*mbiny*mbinx + (iy-mbinylo)*mbinx + (ix-mbinxlo);
}

//compute how many surface layers to use for quadrature sampling

void NBinCAC::compute_surface_depths(double &scalex, double &scaley, double &scalez,
	int &countx, int &county, int &countz, int flag) {
	int poly = current_poly_counter;
	double unit_cell_mapped[3];
	double rcut;
	rcut = atom->CAC_cut - atom->CAC_skin; 
	//flag determines the current element type and corresponding procedure to calculate parameters for 
	//surface penetration depth to be used when computing force density with influences from neighboring
	//elements

	unit_cell_mapped[0] = 2 / double(current_element_scale[0]);
	unit_cell_mapped[1] = 2 / double(current_element_scale[1]);
	unit_cell_mapped[2] = 2 / double(current_element_scale[2]);
	double ds_x = (current_nodal_positions[0][poly][0] - current_nodal_positions[1][poly][0])*
		(current_nodal_positions[0][poly][0] - current_nodal_positions[1][poly][0]);
	double ds_y = (current_nodal_positions[0][poly][1] - current_nodal_positions[1][poly][1])*
		(current_nodal_positions[0][poly][1] - current_nodal_positions[1][poly][1]);
	double ds_z = (current_nodal_positions[0][poly][2] - current_nodal_positions[1][poly][2])*
		(current_nodal_positions[0][poly][2] - current_nodal_positions[1][poly][2]);
	double ds_surf = 2 * rcut / sqrt(ds_x + ds_y + ds_z);
	ds_surf = unit_cell_mapped[0] * (int)(ds_surf / unit_cell_mapped[0]) + unit_cell_mapped[0];

	double dt_x = (current_nodal_positions[0][poly][0] - current_nodal_positions[3][poly][0])*
		(current_nodal_positions[0][poly][0] - current_nodal_positions[3][poly][0]);
	double dt_y = (current_nodal_positions[0][poly][1] - current_nodal_positions[3][poly][1])*
		(current_nodal_positions[0][poly][1] - current_nodal_positions[3][poly][1]);
	double dt_z = (current_nodal_positions[0][poly][2] - current_nodal_positions[3][poly][2])*
		(current_nodal_positions[0][poly][2] - current_nodal_positions[3][poly][2]);

	double dt_surf = 2 * rcut / sqrt(dt_x + dt_y + dt_z);
	dt_surf = unit_cell_mapped[1] * (int)(dt_surf / unit_cell_mapped[1]) + unit_cell_mapped[1];

	double dw_x = (current_nodal_positions[0][poly][0] - current_nodal_positions[4][poly][0])*
		(current_nodal_positions[0][poly][0] - current_nodal_positions[4][poly][0]);
	double dw_y = (current_nodal_positions[0][poly][1] - current_nodal_positions[4][poly][1])*
		(current_nodal_positions[0][poly][1] - current_nodal_positions[3][poly][1]);
	double dw_z = (current_nodal_positions[0][poly][2] - current_nodal_positions[4][poly][2])*
		(current_nodal_positions[0][poly][2] - current_nodal_positions[4][poly][2]);

	double dw_surf = 2 * rcut / sqrt(dw_x + dw_y + dw_z);
	dw_surf = unit_cell_mapped[2] * (int)(dw_surf / unit_cell_mapped[2]) + unit_cell_mapped[2];
	if (ds_surf > 1) {
		ds_surf = 1;


	}
	if (dt_surf > 1) {

		dt_surf = 1;


	}
	if (dw_surf > 1) {

		dw_surf = 1;

	}

	if (atom->one_layer_flag) {
		ds_surf = unit_cell_mapped[0];
		dt_surf = unit_cell_mapped[1];
		dw_surf = unit_cell_mapped[2];
	}
	scalex = 1 - ds_surf;
	scaley = 1 - dt_surf;
	scalez = 1 - dw_surf;

	countx = (int)(ds_surf / unit_cell_mapped[0]);
	county = (int)(dt_surf / unit_cell_mapped[1]);
	countz = (int)(dw_surf / unit_cell_mapped[2]);




}


//initialize Gaussian quadrature rule information, i.e. 2-point quadrature rule vs. 3-point quadrature rule


void NBinCAC::quadrature_init(int quadrature_rank) {
  
	if (quadrature_rank == 1) {
		quadrature_node_count = 1;
		
		memory->create(quadrature_abcissae, quadrature_node_count, "pairCAC:quadrature_abcissae");

		quadrature_abcissae[0] = 0;
	}
	if (quadrature_rank == 2) {



		quadrature_node_count = 2;

		memory->create(quadrature_abcissae, quadrature_node_count, "pairCAC:quadrature_abcissae");

		quadrature_abcissae[0] = -0.5773502691896258;
		quadrature_abcissae[1] = 0.5773502691896258;

	}

	if (quadrature_rank == 3)
	{


	}
	if (quadrature_rank == 4)
	{


	}
	if (quadrature_rank == 5)
	{


	}

 quad_rule_initialized=1;
}

//compute set of quadrature points in sampling set

int NBinCAC::compute_quad_points(int element_index){

	double unit_cell_mapped[3];
	double interior_scale[3];
	int surface_count[3];
	int nodes_per_element;
	int found_flag=0;
	double s, t, w;
	double **x = atom->x;
	s = t = w = 0;
	double sq, tq, wq;
	double quad_position[3];
	double ****nodal_positions = atom->nodal_positions;
	double shape_func;
	int *element_type = atom->element_type;
  int **element_scale = atom->element_scale;
	int *poly_count = atom->poly_count;
	double xtmp, ytmp, ztmp, delx, dely, delz, rsq;
	int *nodes_per_element_list = atom->nodes_per_element_list;
	 nodes_per_element = nodes_per_element_list[element_type[element_index]];
	unit_cell_mapped[0] = 2 / double(element_scale[element_index][0]);
	unit_cell_mapped[1] = 2 / double(element_scale[element_index][1]);
	unit_cell_mapped[2] = 2 / double(element_scale[element_index][2]);
	double ***current_nodal_positions = nodal_positions[element_index];
	int current_poly_count = poly_count[element_index];
  int quadrature_counter=0;


	//compute quadrature point positions to test for neighboring

	int sign[2];
	sign[0] = -1;
	sign[1] = 1;
	
  	    surface_count[0]=surface_counts[element_index][0];
		surface_count[1]=surface_counts[element_index][1];
		surface_count[2]=surface_counts[element_index][2];
	    interior_scale[0]= interior_scales[element_index][0];
		interior_scale[1]= interior_scales[element_index][1];
		interior_scale[2]= interior_scales[element_index][2];
	for (int poly_counter = 0; poly_counter < current_poly_count; poly_counter++) {
		//current_poly_counter = poly_counter;
		
		
		//interior contributions


		for (int i = 0; i < quadrature_node_count; i++) {
			for (int j = 0; j < quadrature_node_count; j++) {
				for (int k = 0; k < quadrature_node_count; k++) {

					sq = s = interior_scale[0] * quadrature_abcissae[i];
					tq = t = interior_scale[1] * quadrature_abcissae[j];
					wq = w = interior_scale[2] * quadrature_abcissae[k];
					s = unit_cell_mapped[0] * (int(s / unit_cell_mapped[0]));
					t = unit_cell_mapped[1] * (int(t / unit_cell_mapped[1]));
					w = unit_cell_mapped[2] * (int(w / unit_cell_mapped[2]));


					quad_position[0] = 0;
					quad_position[1] = 0;
					quad_position[2] = 0;
					for (int kkk = 0; kkk < nodes_per_element; kkk++) {
						shape_func = shape_function(s, t, w, 2, kkk + 1);
						quad_position[0] += current_nodal_positions[kkk][poly_counter][0] * shape_func;
						quad_position[1] += current_nodal_positions[kkk][poly_counter][1] * shape_func;
						quad_position[2] += current_nodal_positions[kkk][poly_counter][2] * shape_func;
					}

				 current_element_quad_points[quadrature_counter][0]=quad_position[0];
         		 current_element_quad_points[quadrature_counter][1]=quad_position[1];
				 current_element_quad_points[quadrature_counter][2]=quad_position[2];
                 quadrature_counter+=1;

				}
			}
		}

		// s axis surface contributions
	for (int sc = 0; sc < 2; sc++) {
		for (int i = 0; i < surface_count[0]; i++) {
			for (int j = 0; j < quadrature_node_count; j++) {
				for (int k = 0; k < quadrature_node_count; k++) {
					
						s = sign[sc] - i*unit_cell_mapped[0] * sign[sc];

						s = s - 0.5*unit_cell_mapped[0] * sign[sc];
						tq = t = interior_scale[1] * quadrature_abcissae[j];
						wq = w = interior_scale[2] * quadrature_abcissae[k];
						t = unit_cell_mapped[1] * (int(t / unit_cell_mapped[1]));
						w = unit_cell_mapped[2] * (int(w / unit_cell_mapped[2]));

						if (quadrature_abcissae[k] < 0)
							w = w - 0.5*unit_cell_mapped[2];
						else
							w = w + 0.5*unit_cell_mapped[2];

						if (quadrature_abcissae[j] < 0)
							t = t - 0.5*unit_cell_mapped[1];
						else
							t = t + 0.5*unit_cell_mapped[1];

						quad_position[0] = 0;
						quad_position[1] = 0;
						quad_position[2] = 0;
						for (int kkk = 0; kkk < nodes_per_element; kkk++) {
							shape_func = shape_function(s, t, w, 2, kkk + 1);
							quad_position[0] += current_nodal_positions[kkk][poly_counter][0] * shape_func;
							quad_position[1] += current_nodal_positions[kkk][poly_counter][1] * shape_func;
							quad_position[2] += current_nodal_positions[kkk][poly_counter][2] * shape_func;
						}

				 current_element_quad_points[quadrature_counter][0]=quad_position[0];
                 current_element_quad_points[quadrature_counter][1]=quad_position[1];
				 current_element_quad_points[quadrature_counter][2]=quad_position[2];
         quadrature_counter+=1;
					}
				}
			}
		}
		
	

	// t axis contributions
	
	for (int sc = 0; sc < 2; sc++) {
		for (int i = 0; i < surface_count[1]; i++) {
			for (int j = 0; j < quadrature_node_count; j++) {
				for (int k = 0; k < quadrature_node_count; k++) {
					

						sq = s = interior_scale[0] * quadrature_abcissae[j];
						s = unit_cell_mapped[0] * (int(s / unit_cell_mapped[0]));
						t = sign[sc] - i*unit_cell_mapped[1] * sign[sc];

						t = t - 0.5*unit_cell_mapped[1] * sign[sc];
						wq = w = interior_scale[2] * quadrature_abcissae[k];
						w = unit_cell_mapped[2] * (int(w / unit_cell_mapped[2]));

						if (quadrature_abcissae[j] < 0)
							s = s - 0.5*unit_cell_mapped[0];
						else
							s = s + 0.5*unit_cell_mapped[0];

						if (quadrature_abcissae[k] < 0)
							w = w - 0.5*unit_cell_mapped[2];
						else
							w = w + 0.5*unit_cell_mapped[2];

						quad_position[0] = 0;
						quad_position[1] = 0;
						quad_position[2] = 0;
						for (int kkk = 0; kkk < nodes_per_element; kkk++) {
							shape_func = shape_function(s, t, w, 2, kkk + 1);
							quad_position[0] += current_nodal_positions[kkk][poly_counter][0] * shape_func;
							quad_position[1] += current_nodal_positions[kkk][poly_counter][1] * shape_func;
							quad_position[2] += current_nodal_positions[kkk][poly_counter][2] * shape_func;
						}

				 current_element_quad_points[quadrature_counter][0]=quad_position[0];
                 current_element_quad_points[quadrature_counter][1]=quad_position[1];
				 current_element_quad_points[quadrature_counter][2]=quad_position[2];
         quadrature_counter+=1;

					}
				}
			}
		}
	

	//w axis surface contributions
	
	for (int sc = 0; sc < 2; sc++) {
		for (int i = 0; i < surface_count[2]; i++) {
			for (int j = 0; j < quadrature_node_count; j++) {
				for (int k = 0; k < quadrature_node_count; k++) {
					

						sq = s = interior_scale[0] * quadrature_abcissae[j];
						s = unit_cell_mapped[0] * (int(s / unit_cell_mapped[0]));
						tq = t = interior_scale[1] * quadrature_abcissae[k];
						t = unit_cell_mapped[1] * (int(t / unit_cell_mapped[1]));
						w = sign[sc] - i*unit_cell_mapped[2] * sign[sc];

						w = w - 0.5*unit_cell_mapped[2] * sign[sc];

						if (quadrature_abcissae[j] < 0)
							s = s - 0.5*unit_cell_mapped[0];
						else
							s = s + 0.5*unit_cell_mapped[0];

						if (quadrature_abcissae[k] < 0)
							t = t - 0.5*unit_cell_mapped[1];
						else
							t = t + 0.5*unit_cell_mapped[1];


						quad_position[0] = 0;
						quad_position[1] = 0;
						quad_position[2] = 0;
						for (int kkk = 0; kkk < nodes_per_element; kkk++) {
							shape_func = shape_function(s, t, w, 2, kkk + 1);
							quad_position[0] += current_nodal_positions[kkk][poly_counter][0] * shape_func;
							quad_position[1] += current_nodal_positions[kkk][poly_counter][1] * shape_func;
							quad_position[2] += current_nodal_positions[kkk][poly_counter][2] * shape_func;
						}

				 current_element_quad_points[quadrature_counter][0]=quad_position[0];
                 current_element_quad_points[quadrature_counter][1]=quad_position[1];
				 current_element_quad_points[quadrature_counter][2]=quad_position[2];
         quadrature_counter+=1;

					}
				}
			}
		}
	


	int surface_countx;
	int surface_county;

	//compute edge contributions

	for (int sc = 0; sc < 12; sc++) {
		if (sc == 0 || sc == 1 || sc == 2 || sc == 3) {

			surface_countx = surface_count[0];
			surface_county = surface_count[1];
		}
		else if (sc == 4 || sc == 5 || sc == 6 || sc == 7) {

			surface_countx = surface_count[1];
			surface_county = surface_count[2];
		}
		else if (sc == 8 || sc == 9 || sc == 10 || sc == 11) {

			surface_countx = surface_count[0];
			surface_county = surface_count[2];
		}


		
		for (int i = 0; i < surface_countx; i++) {//alter surface counts for specific corner
			for (int j = 0; j < surface_county; j++) {
				
				for (int k = 0; k < quadrature_node_count; k++) {
					
						if (sc == 0) {

							sq = s = -1 + (i + 0.5)*unit_cell_mapped[0];
							tq = t = -1 + (j + 0.5)*unit_cell_mapped[1];
							wq = w = interior_scale[2] * quadrature_abcissae[k];
							w = unit_cell_mapped[2] * (int(w / unit_cell_mapped[2]));
							if (quadrature_abcissae[k] < 0)
								w = w - 0.5*unit_cell_mapped[2];
							else
								w = w + 0.5*unit_cell_mapped[2];
						}
						else if (sc == 1) {
							sq = s = 1 - (i + 0.5)*unit_cell_mapped[0];
							tq = t = -1 + (j + 0.5)*unit_cell_mapped[1];
							wq = w = interior_scale[2] * quadrature_abcissae[k];
							w = unit_cell_mapped[2] * (int(w / unit_cell_mapped[2]));
							if (quadrature_abcissae[k] < 0)
								w = w - 0.5*unit_cell_mapped[2];
							else
								w = w + 0.5*unit_cell_mapped[2];
						}
						else if (sc == 2) {
							sq = s = -1 + (i + 0.5)*unit_cell_mapped[0];
							tq = t = 1 - (j + 0.5)*unit_cell_mapped[1];
							wq = w = interior_scale[2] * quadrature_abcissae[k];
							w = unit_cell_mapped[2] * (int(w / unit_cell_mapped[2]));
							if (quadrature_abcissae[k] < 0)
								w = w - 0.5*unit_cell_mapped[2];
							else
								w = w + 0.5*unit_cell_mapped[2];
						}
						else if (sc == 3) {
							sq = s = 1 - (i + 0.5)*unit_cell_mapped[0];
							tq = t = 1 - (j + 0.5)*unit_cell_mapped[1];
							wq = w = interior_scale[2] * quadrature_abcissae[k];
							w = unit_cell_mapped[2] * (int(w / unit_cell_mapped[2]));
							if (quadrature_abcissae[k] < 0)
								w = w - 0.5*unit_cell_mapped[2];
							else
								w = w + 0.5*unit_cell_mapped[2];
						}
						else if (sc == 4) {
							sq = s = interior_scale[0] * quadrature_abcissae[k];
							s = unit_cell_mapped[0] * (int(s / unit_cell_mapped[0]));
							tq = t = -1 + (i + 0.5)*unit_cell_mapped[1];
							wq = w = -1 + (j + 0.5)*unit_cell_mapped[2];
							if (quadrature_abcissae[k] < 0)
								s = s - 0.5*unit_cell_mapped[0];
							else
								s = s + 0.5*unit_cell_mapped[0];

						}
						else if (sc == 5) {
							sq = s = interior_scale[0] * quadrature_abcissae[k];
							s = unit_cell_mapped[0] * (int(s / unit_cell_mapped[0]));
							tq = t = 1 - (i + 0.5)*unit_cell_mapped[1];
							wq = w = -1 + (j + 0.5)*unit_cell_mapped[2];
							if (quadrature_abcissae[k] < 0)
								s = s - 0.5*unit_cell_mapped[0];
							else
								s = s + 0.5*unit_cell_mapped[0];
						}
						else if (sc == 6) {
							sq = s = interior_scale[0] * quadrature_abcissae[k];
							s = unit_cell_mapped[0] * (int(s / unit_cell_mapped[0]));
							tq = t = -1 + (i + 0.5)*unit_cell_mapped[1];
							wq = w = 1 - (j + 0.5)*unit_cell_mapped[2];
							if (quadrature_abcissae[k] < 0)
								s = s - 0.5*unit_cell_mapped[0];
							else
								s = s + 0.5*unit_cell_mapped[0];
						}
						else if (sc == 7) {
							sq = s = interior_scale[0] * quadrature_abcissae[k];
							s = unit_cell_mapped[0] * (int(s / unit_cell_mapped[0]));
							tq = t = 1 - (i + 0.5)*unit_cell_mapped[1];
							wq = w = 1 - (j + 0.5)*unit_cell_mapped[2];
							if (quadrature_abcissae[k] < 0)
								s = s - 0.5*unit_cell_mapped[0];
							else
								s = s + 0.5*unit_cell_mapped[0];
						}
						else if (sc == 8) {
							sq = s = -1 + (i + 0.5)*unit_cell_mapped[0];
							tq = t = interior_scale[1] * quadrature_abcissae[k];
							t = unit_cell_mapped[1] * (int(t / unit_cell_mapped[1]));
							wq = w = -1 + (j + 0.5)*unit_cell_mapped[2];
							if (quadrature_abcissae[k] < 0)
								t = t - 0.5*unit_cell_mapped[1];
							else
								t = t + 0.5*unit_cell_mapped[1];

						}
						else if (sc == 9) {
							sq = s = 1 - (i + 0.5)*unit_cell_mapped[0];
							tq = t = interior_scale[1] * quadrature_abcissae[k];
							t = unit_cell_mapped[1] * (int(t / unit_cell_mapped[1]));
							wq = w = -1 + (j + 0.5)*unit_cell_mapped[2];
							if (quadrature_abcissae[k] < 0)
								t = t - 0.5*unit_cell_mapped[1];
							else
								t = t + 0.5*unit_cell_mapped[1];
						}
						else if (sc == 10) {
							sq = s = -1 + (i + 0.5)*unit_cell_mapped[0];
							tq = t = interior_scale[1] * quadrature_abcissae[k];
							t = unit_cell_mapped[1] * (int(t / unit_cell_mapped[1]));
							wq = w = 1 - (j + 0.5)*unit_cell_mapped[2];
							if (quadrature_abcissae[k] < 0)
								t = t - 0.5*unit_cell_mapped[1];
							else
								t = t + 0.5*unit_cell_mapped[1];
						}
						else if (sc == 11) {
							sq = s = 1 - (i + 0.5)*unit_cell_mapped[0];
							tq = t = interior_scale[1] * quadrature_abcissae[k];
							t = unit_cell_mapped[1] * (int(t / unit_cell_mapped[1]));
							wq = w = 1 - (j + 0.5)*unit_cell_mapped[2];
							if (quadrature_abcissae[k] < 0)
								t = t - 0.5*unit_cell_mapped[1];
							else
								t = t + 0.5*unit_cell_mapped[1];
						}



						quad_position[0] = 0;
						quad_position[1] = 0;
						quad_position[2] = 0;
						for (int kkk = 0; kkk < nodes_per_element; kkk++) {
							shape_func = shape_function(s, t, w, 2, kkk + 1);
							quad_position[0] += current_nodal_positions[kkk][poly_counter][0] * shape_func;
							quad_position[1] += current_nodal_positions[kkk][poly_counter][1] * shape_func;
							quad_position[2] += current_nodal_positions[kkk][poly_counter][2] * shape_func;
						}

				 current_element_quad_points[quadrature_counter][0]=quad_position[0];
                 current_element_quad_points[quadrature_counter][1]=quad_position[1];
				 current_element_quad_points[quadrature_counter][2]=quad_position[2];
         quadrature_counter+=1;
					}

				}

			}
		}
	


	//compute corner contributions

	for (int sc = 0; sc < 8; sc++) {
		for (int i = 0; i < surface_count[0]; i++) {//alter surface counts for specific corner
			for (int j = 0; j < surface_count[1]; j++) {
				for (int k = 0; k < surface_count[2]; k++) {
					
					
						if (sc == 0) {

							s = -1 + (i + 0.5)*unit_cell_mapped[0];
							t = -1 + (j + 0.5)*unit_cell_mapped[1];
							w = -1 + (k + 0.5)*unit_cell_mapped[2];

						}
						else if (sc == 1) {
							s = 1 - (i + 0.5)*unit_cell_mapped[0];
							t = -1 + (j + 0.5)*unit_cell_mapped[1];
							w = -1 + (k + 0.5)*unit_cell_mapped[2];
						}
						else if (sc == 2) {
							s = 1 - (i + 0.5)*unit_cell_mapped[0];
							t = 1 - (j + 0.5)*unit_cell_mapped[1];
							w = -1 + (k + 0.5)*unit_cell_mapped[2];
						}
						else if (sc == 3) {
							s = -1 + (i + 0.5)*unit_cell_mapped[0];
							t = 1 - (j + 0.5)*unit_cell_mapped[1];
							w = -1 + (k + 0.5)*unit_cell_mapped[2];
						}
						else if (sc == 4) {
							s = -1 + (i + 0.5)*unit_cell_mapped[0];
							t = -1 + (j + 0.5)*unit_cell_mapped[1];
							w = 1 - (k + 0.5)*unit_cell_mapped[2];

						}
						else if (sc == 5) {
							s = 1 - (i + 0.5)*unit_cell_mapped[0];
							t = -1 + (j + 0.5)*unit_cell_mapped[1];
							w = 1 - (k + 0.5)*unit_cell_mapped[2];
						}
						else if (sc == 6) {
							s = 1 - (i + 0.5)*unit_cell_mapped[0];
							t = 1 - (j + 0.5)*unit_cell_mapped[1];
							w = 1 - (k + 0.5)*unit_cell_mapped[2];
						}
						else if (sc == 7) {
							s = -1 + (i + 0.5)*unit_cell_mapped[0];
							t = 1 - (j + 0.5)*unit_cell_mapped[1];
							w = 1 - (k + 0.5)*unit_cell_mapped[2];
						}

						quad_position[0] = 0;
						quad_position[1] = 0;
						quad_position[2] = 0;
						for (int kkk = 0; kkk < nodes_per_element; kkk++) {
							shape_func = shape_function(s, t, w, 2, kkk + 1);
							quad_position[0] += current_nodal_positions[kkk][poly_counter][0] * shape_func;
							quad_position[1] += current_nodal_positions[kkk][poly_counter][1] * shape_func;
							quad_position[2] += current_nodal_positions[kkk][poly_counter][2] * shape_func;
						}

				 current_element_quad_points[quadrature_counter][0]=quad_position[0];
                 current_element_quad_points[quadrature_counter][1]=quad_position[1];
				 current_element_quad_points[quadrature_counter][2]=quad_position[2];
         quadrature_counter+=1;

					}
				}

			}
		}
	}


	return quadrature_counter;

}

///////////////////////////////////////////
double NBinCAC::shape_function(double s, double t, double w, int flag, int index) {
	double shape_function = 0;
	if (flag == 2) {

		if (index == 1) {
			shape_function = (1 - s)*(1 - t)*(1 - w) / 8;
		}
		else if (index == 2) {
			shape_function = (1 + s)*(1 - t)*(1 - w) / 8;
		}
		else if (index == 3) {
			shape_function = (1 + s)*(1 + t)*(1 - w) / 8;
		}
		else if (index == 4) {
			shape_function = (1 - s)*(1 + t)*(1 - w) / 8;
		}
		else if (index == 5) {
			shape_function = (1 - s)*(1 - t)*(1 + w) / 8;
		}
		else if (index == 6) {
			shape_function = (1 + s)*(1 - t)*(1 + w) / 8;
		}
		else if (index == 7) {
			shape_function = (1 + s)*(1 + t)*(1 + w) / 8;
		}
		else if (index == 8) {
			shape_function = (1 - s)*(1 + t)*(1 + w) / 8;
		}


	}
	return shape_function;


}

//allocate surface_counts used in quadrature sampling for each element
void NBinCAC::allocate_surface_counts() {
	memory->grow(surface_counts, atom->nlocal , 3, "NBinCAC:surface_counts");
	memory->grow(interior_scales, atom->nlocal , 3, "NBinCAC:interior_scales");
	nmax = atom->nlocal;
}
