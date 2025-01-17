/*
//@HEADER
// ************************************************************************
//
//                        Kokkos v. 2.0
//              Copyright (2014) Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Christian R. Trott (crtrott@sandia.gov)
//
// ************************************************************************
//@HEADER
*/

#if defined( KOKKOS_ENABLE_RFO_PREFETCH )
#include <xmmintrin.h>
#endif

#include <Kokkos_Macros.hpp>
#if defined( KOKKOS_ATOMIC_HPP ) && ! defined( KOKKOS_ATOMIC_FETCH_ADD_HPP )
#define KOKKOS_ATOMIC_FETCH_ADD_HPP

#if defined(KOKKOS_ENABLE_CUDA)
#include<Cuda/Kokkos_Cuda_Version_9_8_Compatibility.hpp>
#endif

namespace Kokkos {

//----------------------------------------------------------------------------

#if defined( KOKKOS_ENABLE_CUDA )
#if defined(__CUDA_ARCH__) || defined(KOKKOS_IMPL_CUDA_CLANG_WORKAROUND)

// Support for int, unsigned int, unsigned long long int, and float

__inline__ __device__
int atomic_fetch_add( volatile int * const dest , const int val )
{ return atomicAdd((int*)dest,val); }

__inline__ __device__
unsigned int atomic_fetch_add( volatile unsigned int * const dest , const unsigned int val )
{ return atomicAdd((unsigned int*)dest,val); }

__inline__ __device__
unsigned long long int atomic_fetch_add( volatile unsigned long long int * const dest ,
                                         const unsigned long long int val )
{ return atomicAdd((unsigned long long int*)dest,val); }

__inline__ __device__
float atomic_fetch_add( volatile float * const dest , const float val )
{ return atomicAdd((float*)dest,val); }

#if ( 600 <= __CUDA_ARCH__ )
__inline__ __device__
double atomic_fetch_add( volatile double * const dest , const double val )
{ return atomicAdd((double*)dest,val); }
#endif

template < typename T >
__inline__ __device__
T atomic_fetch_add( volatile T * const dest ,
  typename Kokkos::Impl::enable_if< sizeof(T) == sizeof(int) , const T >::type val )
{
  union U {
    int i ;
    T t ;
    KOKKOS_INLINE_FUNCTION U() {};
  } assume , oldval , newval ;

  oldval.t = *dest ;

  do {
    assume.i = oldval.i ;
    newval.t = assume.t + val ;
    oldval.i = atomicCAS( (int*)dest , assume.i , newval.i );
  } while ( assume.i != oldval.i );

  return oldval.t ;
}

template < typename T >
__inline__ __device__
T atomic_fetch_add( volatile T * const dest ,
  typename Kokkos::Impl::enable_if< sizeof(T) != sizeof(int) &&
                                    sizeof(T) == sizeof(unsigned long long int) , const T >::type val )
{
  union U {
    unsigned long long int i ;
    T t ;
    KOKKOS_INLINE_FUNCTION U() {};
  } assume , oldval , newval ;

  oldval.t = *dest ;

  do {
    assume.i = oldval.i ;
    newval.t = assume.t + val ;
    oldval.i = atomicCAS( (unsigned long long int*)dest , assume.i , newval.i );
  } while ( assume.i != oldval.i );

  return oldval.t ;
}

//----------------------------------------------------------------------------

template < typename T >
__inline__ __device__
T atomic_fetch_add( volatile T * const dest ,
    typename Kokkos::Impl::enable_if<
                  ( sizeof(T) != 4 )
               && ( sizeof(T) != 8 )
             , const T >::type& val )
{
  T return_val;
  // This is a way to (hopefully) avoid dead lock in a warp
  int done = 0;
  unsigned int active = KOKKOS_IMPL_CUDA_BALLOT(1);
  unsigned int done_active = 0;
  while (active!=done_active) {
    if(!done) {
      bool locked = Impl::lock_address_cuda_space( (void*) dest );
      if( locked ) {
        return_val = *dest;
        *dest = return_val + val;
        Impl::unlock_address_cuda_space( (void*) dest );
        done = 1;
      }
    }
    done_active = KOKKOS_IMPL_CUDA_BALLOT(done);
  }
  return return_val;
}
#endif
#endif
//----------------------------------------------------------------------------
#if !defined(KOKKOS_ENABLE_ROCM_ATOMICS)
#if !defined(__CUDA_ARCH__) || defined(KOKKOS_IMPL_CUDA_CLANG_WORKAROUND)
#if defined(KOKKOS_ENABLE_GNU_ATOMICS) || defined(KOKKOS_ENABLE_INTEL_ATOMICS)

#if defined( KOKKOS_ENABLE_ASM ) && defined ( KOKKOS_ENABLE_ISA_X86_64 )
inline
int atomic_fetch_add( volatile int * dest , const int val )
{
#if defined( KOKKOS_ENABLE_RFO_PREFETCH ) 
  _mm_prefetch( (const char*) dest, _MM_HINT_ET0 );
#endif

  int original = val;

  __asm__ __volatile__(
  	"lock xadd %1, %0"
        : "+m" (*dest), "+r" (original)
        : "m" (*dest), "r" (original)
        : "memory"
        );

  return original;
}
#else
inline
int atomic_fetch_add( volatile int * const dest , const int val )
{
#if defined( KOKKOS_ENABLE_RFO_PREFETCH )
  _mm_prefetch( (const char*) dest, _MM_HINT_ET0 );
#endif
  return __sync_fetch_and_add(dest, val);
}
#endif

inline
long int atomic_fetch_add( volatile long int * const dest , const long int val )
{ 
#if defined( KOKKOS_ENABLE_RFO_PREFETCH )
  _mm_prefetch( (const char*) dest, _MM_HINT_ET0 );
#endif
  return __sync_fetch_and_add(dest,val);
}

#if defined( KOKKOS_ENABLE_GNU_ATOMICS )

inline
unsigned int atomic_fetch_add( volatile unsigned int * const dest , const unsigned int val )
{
#if defined( KOKKOS_ENABLE_RFO_PREFETCH )
  _mm_prefetch( (const char*) dest, _MM_HINT_ET0 );
#endif
  return __sync_fetch_and_add(dest,val);
}

inline
unsigned long int atomic_fetch_add( volatile unsigned long int * const dest , const unsigned long int val )
{ 
#if defined( KOKKOS_ENABLE_RFO_PREFETCH )
  _mm_prefetch( (const char*) dest, _MM_HINT_ET0 );
#endif
  return __sync_fetch_and_add(dest,val);
}

#endif

template < typename T >
inline
T atomic_fetch_add( volatile T * const dest ,
  typename Kokkos::Impl::enable_if< sizeof(T) == sizeof(int) , const T >::type val )
{
  union U {
    int i ;
    T t ;
    inline U() {};
  } assume , oldval , newval ;

#if defined( KOKKOS_ENABLE_RFO_PREFETCH )
  _mm_prefetch( (const char*) dest, _MM_HINT_ET0 );
#endif

  oldval.t = *dest ;

  do {
    assume.i = oldval.i ;
    newval.t = assume.t + val ;
    oldval.i = __sync_val_compare_and_swap( (int*) dest , assume.i , newval.i );
  } while ( assume.i != oldval.i );

  return oldval.t ;
}

template < typename T >
inline
T atomic_fetch_add( volatile T * const dest ,
  typename Kokkos::Impl::enable_if< sizeof(T) != sizeof(int) &&
                                    sizeof(T) == sizeof(long) , const T >::type val )
{
  union U {
    long i ;
    T t ;
    inline U() {};
  } assume , oldval , newval ;

#if defined( KOKKOS_ENABLE_RFO_PREFETCH )
  _mm_prefetch( (const char*) dest, _MM_HINT_ET0 );
#endif

  oldval.t = *dest ;

  do {
    assume.i = oldval.i ;
    newval.t = assume.t + val ;
    oldval.i = __sync_val_compare_and_swap( (long*) dest , assume.i , newval.i );
  } while ( assume.i != oldval.i );

  return oldval.t ;
}

#if defined( KOKKOS_ENABLE_ASM ) && defined ( KOKKOS_ENABLE_ISA_X86_64 )
template < typename T >
inline
T atomic_fetch_add( volatile T * const dest ,
  typename Kokkos::Impl::enable_if< sizeof(T) != sizeof(int) &&
                                    sizeof(T) != sizeof(long) &&
                                    sizeof(T) == sizeof(Impl::cas128_t) , const T >::type val )
{
  union U {
    Impl::cas128_t i ;
    T t ;
    inline U() {};
  } assume , oldval , newval ;

#if defined( KOKKOS_ENABLE_RFO_PREFETCH )
  _mm_prefetch( (const char*) dest, _MM_HINT_ET0 );
#endif

  oldval.t = *dest ;

  do {
    assume.i = oldval.i ;
    newval.t = assume.t + val ;
    oldval.i = Impl::cas128( (volatile Impl::cas128_t*) dest , assume.i , newval.i );
  } while ( assume.i != oldval.i );

  return oldval.t ;
}
#endif

//----------------------------------------------------------------------------

template < typename T >
inline
T atomic_fetch_add( volatile T * const dest ,
    typename Kokkos::Impl::enable_if<
                  ( sizeof(T) != 4 )
               && ( sizeof(T) != 8 )
              #if defined(KOKKOS_ENABLE_ASM) && defined ( KOKKOS_ENABLE_ISA_X86_64 )
               && ( sizeof(T) != 16 )
              #endif
                 , const T >::type& val )
{
  while( !Impl::lock_address_host_space( (void*) dest ) );
  T return_val = *dest;

  // Don't use the following line of code here:
  //
  //const T tmp = *dest = return_val + val;
  //
  // Instead, put each assignment in its own statement.  This is
  // because the overload of T::operator= for volatile *this should
  // return void, not volatile T&.  See Kokkos #177:
  //
  // https://github.com/kokkos/kokkos/issues/177
  *dest = return_val + val;
  const T tmp = *dest;
  (void) tmp;
  Impl::unlock_address_host_space( (void*) dest );

  return return_val;
}
//----------------------------------------------------------------------------

#elif defined( KOKKOS_ENABLE_OPENMP_ATOMICS )

template< typename T >
T atomic_fetch_add( volatile T * const dest , const T val )
{
  T retval;
#pragma omp atomic capture
  {
    retval = dest[0];
    dest[0] += val;
  }
  return retval;
}

#elif defined( KOKKOS_ENABLE_SERIAL_ATOMICS )

template< typename T >
T atomic_fetch_add( volatile T * const dest_v , const T val )
{
  T* dest = const_cast<T*>(dest_v);
  T retval = *dest;
  *dest += val;
  return retval;
}

#endif
#endif
#endif // !defined ROCM_ATOMICS
//----------------------------------------------------------------------------

// Simpler version of atomic_fetch_add without the fetch
template <typename T>
KOKKOS_INLINE_FUNCTION
void atomic_add(volatile T * const dest, const T src) {
  atomic_fetch_add(dest,src);
}

}
#endif

