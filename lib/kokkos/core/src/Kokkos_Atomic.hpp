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

/// \file Kokkos_Atomic.hpp
/// \brief Atomic functions
///
/// This header file defines prototypes for the following atomic functions:
///   - exchange
///   - compare and exchange
///   - add
///
/// Supported types include:
///   - signed and unsigned 4 and 8 byte integers
///   - float
///   - double
///
/// They are implemented through GCC compatible intrinsics, OpenMP
/// directives and native CUDA intrinsics.
///
/// Including this header file requires one of the following
/// compilers:
///   - NVCC (for CUDA device code only)
///   - GCC (for host code only)
///   - Intel (for host code only)
///   - A compiler that supports OpenMP 3.1 (for host code only)

#ifndef KOKKOS_ATOMIC_HPP
#define KOKKOS_ATOMIC_HPP

#include <Kokkos_Macros.hpp>
#include <Kokkos_HostSpace.hpp>
#include <impl/Kokkos_Traits.hpp>

//----------------------------------------------------------------------------
#if defined(_WIN32)
#define KOKKOS_ENABLE_WINDOWS_ATOMICS
#else
#if defined( KOKKOS_ENABLE_CUDA )

// Compiling NVIDIA device code, must use Cuda atomics:

#define KOKKOS_ENABLE_CUDA_ATOMICS

#elif defined(KOKKOS_ACTIVE_EXECUTION_MEMORY_SPACE_ROCM_GPU)

#define KOKKOS_ENABLE_ROCM_ATOMICS

#endif

#if ! defined( KOKKOS_ENABLE_GNU_ATOMICS ) && \
    ! defined( KOKKOS_ENABLE_INTEL_ATOMICS ) && \
    ! defined( KOKKOS_ENABLE_OPENMP_ATOMICS ) && \
    ! defined( KOKKOS_ENABLE_SERIAL_ATOMICS )

// Compiling for non-Cuda atomic implementation has not been pre-selected.
// Choose the best implementation for the detected compiler.
// Preference: GCC, INTEL, OMP31

#if defined( KOKKOS_INTERNAL_NOT_PARALLEL )

#define KOKKOS_ENABLE_SERIAL_ATOMICS

#elif defined( KOKKOS_COMPILER_GNU ) || \
    defined( KOKKOS_COMPILER_CLANG ) || \
    ( defined ( KOKKOS_COMPILER_NVCC ) )

#define KOKKOS_ENABLE_GNU_ATOMICS

#elif defined( KOKKOS_COMPILER_INTEL ) || \
      defined( KOKKOS_COMPILER_CRAYC )

#define KOKKOS_ENABLE_INTEL_ATOMICS

#elif defined( _OPENMP ) && ( 201107 <= _OPENMP )

#define KOKKOS_ENABLE_OPENMP_ATOMICS

#else

#error "KOKKOS_ATOMICS_USE : Unsupported compiler"

#endif

#endif /* Not pre-selected atomic implementation */
#endif

#ifdef KOKKOS_ENABLE_CUDA
#include <Cuda/Kokkos_Cuda_Locks.hpp>
#endif

namespace Kokkos {
template <typename T>
KOKKOS_INLINE_FUNCTION
void atomic_add(volatile T * const dest, const T src);

// Atomic increment
template<typename T>
KOKKOS_INLINE_FUNCTION
void atomic_increment(volatile T* a);

template<typename T>
KOKKOS_INLINE_FUNCTION
void atomic_decrement(volatile T* a);
}

namespace Kokkos {


inline
const char * atomic_query_version()
{
#if defined( KOKKOS_ENABLE_CUDA_ATOMICS )
  return "KOKKOS_ENABLE_CUDA_ATOMICS" ;
#elif defined( KOKKOS_ENABLE_GNU_ATOMICS )
  return "KOKKOS_ENABLE_GNU_ATOMICS" ;
#elif defined( KOKKOS_ENABLE_INTEL_ATOMICS )
  return "KOKKOS_ENABLE_INTEL_ATOMICS" ;
#elif defined( KOKKOS_ENABLE_OPENMP_ATOMICS )
  return "KOKKOS_ENABLE_OPENMP_ATOMICS" ;
#elif defined( KOKKOS_ENABLE_WINDOWS_ATOMICS )
  return "KOKKOS_ENABLE_WINDOWS_ATOMICS";
#elif defined( KOKKOS_ENABLE_SERIAL_ATOMICS )
  return "KOKKOS_ENABLE_SERIAL_ATOMICS";
#else
#error "No valid response for atomic_query_version!"
#endif
}

} // namespace Kokkos

#if defined( KOKKOS_ENABLE_ROCM )
#include <ROCm/Kokkos_ROCm_Atomic.hpp>
namespace Kokkos {
namespace Impl {
extern KOKKOS_INLINE_FUNCTION
bool lock_address_rocm_space(void* ptr);

extern KOKKOS_INLINE_FUNCTION
void unlock_address_rocm_space(void* ptr);
}
}
#endif

#ifdef _WIN32
#include "impl/Kokkos_Atomic_Windows.hpp"
#else

//----------------------------------------------------------------------------
// Atomic Assembly
//
// Implements CAS128-bit in assembly

#include "impl/Kokkos_Atomic_Assembly.hpp"

//----------------------------------------------------------------------------
// Atomic exchange
//
// template< typename T >
// T atomic_exchange( volatile T* const dest , const T val )
// { T tmp = *dest ; *dest = val ; return tmp ; }

#include "impl/Kokkos_Atomic_Exchange.hpp"

//----------------------------------------------------------------------------
// Atomic compare-and-exchange
//
// template<class T>
// bool atomic_compare_exchange_strong(volatile T* const dest, const T compare, const T val)
// { bool equal = compare == *dest ; if ( equal ) { *dest = val ; } return equal ; }

#include "impl/Kokkos_Atomic_Compare_Exchange_Strong.hpp"

//----------------------------------------------------------------------------
// Atomic fetch and add
//
// template<class T>
// T atomic_fetch_add(volatile T* const dest, const T val)
// { T tmp = *dest ; *dest += val ; return tmp ; }

#include "impl/Kokkos_Atomic_Fetch_Add.hpp"

//----------------------------------------------------------------------------
// Atomic increment
//
// template<class T>
// T atomic_increment(volatile T* const dest)
// { dest++; }

#include "impl/Kokkos_Atomic_Increment.hpp"

//----------------------------------------------------------------------------
// Atomic Decrement
//
// template<class T>
// T atomic_decrement(volatile T* const dest)
// { dest--; }

#include "impl/Kokkos_Atomic_Decrement.hpp"

//----------------------------------------------------------------------------
// Atomic fetch and sub
//
// template<class T>
// T atomic_fetch_sub(volatile T* const dest, const T val)
// { T tmp = *dest ; *dest -= val ; return tmp ; }

#include "impl/Kokkos_Atomic_Fetch_Sub.hpp"

//----------------------------------------------------------------------------
// Atomic fetch and or
//
// template<class T>
// T atomic_fetch_or(volatile T* const dest, const T val)
// { T tmp = *dest ; *dest = tmp | val ; return tmp ; }

#include "impl/Kokkos_Atomic_Fetch_Or.hpp"

//----------------------------------------------------------------------------
// Atomic fetch and and
//
// template<class T>
// T atomic_fetch_and(volatile T* const dest, const T val)
// { T tmp = *dest ; *dest = tmp & val ; return tmp ; }

#include "impl/Kokkos_Atomic_Fetch_And.hpp"
#endif /*Not _WIN32*/

//----------------------------------------------------------------------------
// Memory fence
//
// All loads and stores from this thread will be globally consistent before continuing
//
// void memory_fence() {...};
#include "impl/Kokkos_Memory_Fence.hpp"

//----------------------------------------------------------------------------
// Provide volatile_load and safe_load
//
// T volatile_load(T const volatile * const ptr);
//
// T const& safe_load(T const * const ptr);
// XEON PHI
// T safe_load(T const * const ptr

#include "impl/Kokkos_Volatile_Load.hpp"

#ifndef _WIN32
#include "impl/Kokkos_Atomic_Generic.hpp"
#endif
//----------------------------------------------------------------------------
// This atomic-style macro should be an inlined function, not a macro

#if defined( KOKKOS_COMPILER_GNU ) && !defined(__PGIC__) && !defined(__CUDA_ARCH__)

  #define KOKKOS_NONTEMPORAL_PREFETCH_LOAD(addr) __builtin_prefetch(addr,0,0)
  #define KOKKOS_NONTEMPORAL_PREFETCH_STORE(addr) __builtin_prefetch(addr,1,0)

#else

  #define KOKKOS_NONTEMPORAL_PREFETCH_LOAD(addr) ((void)0)
  #define KOKKOS_NONTEMPORAL_PREFETCH_STORE(addr) ((void)0)

#endif

//----------------------------------------------------------------------------

#endif /* KOKKOS_ATOMIC_HPP */

