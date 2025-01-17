#ifndef LMP_MANIFOLD_CYLINDER_H
#define LMP_MANIFOLD_CYLINDER_H

#include "manifold.h"

// A normal cylinder


namespace LAMMPS_NS {

namespace user_manifold {

  class manifold_cylinder : public manifold {
  public:
    enum { NPARAMS = 1 }; // Number of parameters.
    manifold_cylinder( LAMMPS *lmp, int, char ** );
    virtual ~manifold_cylinder(){}
    virtual double g( const double *x );
    virtual void   n( const double *x, double *n );
    static const char *type(){ return "cylinder"; }
    virtual const char *id(){ return type(); }
    static int expected_argc(){ return NPARAMS; }
    virtual int nparams(){ return NPARAMS; }
  };
}

}

#endif // LMP_MANIFOLD_CYLINDER_H
