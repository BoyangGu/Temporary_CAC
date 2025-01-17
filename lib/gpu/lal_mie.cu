// **************************************************************************
//                                   mie.cu
//                             -------------------
//                           Trung Dac Nguyen (ORNL)
//
//  Device code for acceleration of the mie pair style
//
// __________________________________________________________________________
//    This file is part of the LAMMPS Accelerator Library (LAMMPS_AL)
// __________________________________________________________________________
//
//    begin                :
//    email                : nguyentd@ornl.gov
// ***************************************************************************/

#ifdef NV_KERNEL
#include "lal_aux_fun1.h"
#ifndef _DOUBLE_DOUBLE
texture<float4> pos_tex;
#else
texture<int4,1> pos_tex;
#endif
#else
#define pos_tex x_
#endif

__kernel void k_mie(const __global numtyp4 *restrict x_,
                    const __global numtyp4 *restrict mie1,
                    const __global numtyp4 *restrict mie3,
                    const int lj_types,
                    const __global numtyp *restrict sp_lj_in,
                    const __global int *dev_nbor,
                    const __global int *dev_packed,
                    __global acctyp4 *restrict ans,
                    __global acctyp *restrict engv,
                    const int eflag, const int vflag, const int inum,
                    const int nbor_pitch, const int t_per_atom) {
  int tid, ii, offset;
  atom_info(t_per_atom,ii,tid,offset);

  __local numtyp sp_lj[4];
  sp_lj[0]=sp_lj_in[0];
  sp_lj[1]=sp_lj_in[1];
  sp_lj[2]=sp_lj_in[2];
  sp_lj[3]=sp_lj_in[3];

  acctyp energy=(acctyp)0;
  acctyp4 f;
  f.x=(acctyp)0; f.y=(acctyp)0; f.z=(acctyp)0;
  acctyp virial[6];
  for (int i=0; i<6; i++)
    virial[i]=(acctyp)0;

  if (ii<inum) {
    int nbor, nbor_end;
    int i, numj;
    __local int n_stride;
    nbor_info(dev_nbor,dev_packed,nbor_pitch,t_per_atom,ii,offset,i,numj,
              n_stride,nbor_end,nbor);

    numtyp4 ix; fetch4(ix,i,pos_tex); //x_[i];
    int itype=ix.w;

    numtyp factor_lj;
    for ( ; nbor<nbor_end; nbor+=n_stride) {

      int j=dev_packed[nbor];
      factor_lj = sp_lj[sbmask(j)];
      j &= NEIGHMASK;

      numtyp4 jx; fetch4(jx,j,pos_tex); //x_[j];
      int jtype=jx.w;

      // Compute r12
      numtyp delx = ix.x-jx.x;
      numtyp dely = ix.y-jx.y;
      numtyp delz = ix.z-jx.z;
      numtyp rsq = delx*delx+dely*dely+delz*delz;

      int mtype=itype*lj_types+jtype;
      if (rsq<mie3[mtype].w) {
        numtyp r2inv = ucl_recip(rsq);
        numtyp rgamA = pow(r2inv,(mie1[mtype].z/(numtyp)2.0));
        numtyp rgamR = pow(r2inv,(mie1[mtype].w/(numtyp)2.0));
        numtyp forcemie =  (mie1[mtype].x*rgamR - mie1[mtype].y*rgamA);
        numtyp force = factor_lj*forcemie*r2inv;

        f.x+=delx*force;
        f.y+=dely*force;
        f.z+=delz*force;

        if (eflag>0) {
          numtyp e=(mie3[mtype].x*rgamR - mie3[mtype].y*rgamA) -
            mie3[mtype].z;
          energy+=factor_lj*e;
        }
        if (vflag>0) {
          virial[0] += delx*delx*force;
          virial[1] += dely*dely*force;
          virial[2] += delz*delz*force;
          virial[3] += delx*dely*force;
          virial[4] += delx*delz*force;
          virial[5] += dely*delz*force;
        }
      }

    } // for nbor
    store_answers(f,energy,virial,ii,inum,tid,t_per_atom,offset,eflag,vflag,
                  ans,engv);
  } // if ii
}

__kernel void k_mie_fast(const __global numtyp4 *restrict x_,
                         const __global numtyp4 *restrict mie1_in,
                         const __global numtyp4 *restrict mie3_in,
                         const __global numtyp *restrict sp_lj_in,
                         const __global int *dev_nbor,
                         const __global int *dev_packed,
                         __global acctyp4 *restrict ans,
                         __global acctyp *restrict engv,
                         const int eflag, const int vflag, const int inum,
                         const int nbor_pitch, const int t_per_atom) {
  int tid, ii, offset;
  atom_info(t_per_atom,ii,tid,offset);

  __local numtyp4 mie1[MAX_SHARED_TYPES*MAX_SHARED_TYPES];
  __local numtyp4 mie3[MAX_SHARED_TYPES*MAX_SHARED_TYPES];
  __local numtyp sp_lj[4];
  if (tid<4)
    sp_lj[tid]=sp_lj_in[tid];
  if (tid<MAX_SHARED_TYPES*MAX_SHARED_TYPES) {
    mie1[tid]=mie1_in[tid];
    mie3[tid]=mie3_in[tid];
  }

  acctyp energy=(acctyp)0;
  acctyp4 f;
  f.x=(acctyp)0; f.y=(acctyp)0; f.z=(acctyp)0;
  acctyp virial[6];
  for (int i=0; i<6; i++)
    virial[i]=(acctyp)0;

  __syncthreads();

  if (ii<inum) {
    int nbor, nbor_end;
    int i, numj;
    __local int n_stride;
    nbor_info(dev_nbor,dev_packed,nbor_pitch,t_per_atom,ii,offset,i,numj,
              n_stride,nbor_end,nbor);

    numtyp4 ix; fetch4(ix,i,pos_tex); //x_[i];
    int iw=ix.w;
    int itype=fast_mul((int)MAX_SHARED_TYPES,iw);

    numtyp factor_lj;
    for ( ; nbor<nbor_end; nbor+=n_stride) {

      int j=dev_packed[nbor];
      factor_lj = sp_lj[sbmask(j)];
      j &= NEIGHMASK;

      numtyp4 jx; fetch4(jx,j,pos_tex); //x_[j];
      int mtype=itype+jx.w;

      // Compute r12
      numtyp delx = ix.x-jx.x;
      numtyp dely = ix.y-jx.y;
      numtyp delz = ix.z-jx.z;
      numtyp rsq = delx*delx+dely*dely+delz*delz;

      if (rsq<mie3[mtype].w) {
        numtyp r2inv = ucl_recip(rsq);
        numtyp rgamA = pow(r2inv,(mie1[mtype].z/(numtyp)2.0));
        numtyp rgamR = pow(r2inv,(mie1[mtype].w/(numtyp)2.0));
        numtyp forcemie =  (mie1[mtype].x*rgamR - mie1[mtype].y*rgamA);
        numtyp force = factor_lj*forcemie*r2inv;

        f.x+=delx*force;
        f.y+=dely*force;
        f.z+=delz*force;

        if (eflag>0) {
          numtyp e=(mie3[mtype].x*rgamR - mie3[mtype].y*rgamA) -
            mie3[mtype].z;
          energy+=factor_lj*e;
        }
        if (vflag>0) {
          virial[0] += delx*delx*force;
          virial[1] += dely*dely*force;
          virial[2] += delz*delz*force;
          virial[3] += delx*dely*force;
          virial[4] += delx*delz*force;
          virial[5] += dely*delz*force;
        }
      }

    } // for nbor
    store_answers(f,energy,virial,ii,inum,tid,t_per_atom,offset,eflag,vflag,
                  ans,engv);
  } // if ii
}

