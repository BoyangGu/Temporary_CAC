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
#if defined( KOKKOS_ATOMIC_HPP) && ! defined( KOKKOS_ATOMIC_INCREMENT_HPP )
#define KOKKOS_ATOMIC_INCREMENT_HPP

namespace Kokkos {

// Atomic increment
template<>
KOKKOS_INLINE_FUNCTION
void atomic_increment<char>(volatile char* a) {
#if defined( KOKKOS_ENABLE_ASM ) && defined( KOKKOS_ENABLE_ISA_X86_64 ) && ! defined(_WIN32) && ! defined(__CUDA_ARCH__)
#if defined( KOKKOS_ENABLE_RFO_PREFETCH )
  _mm_prefetch( (const char*) a, _MM_HINT_ET0 );
#endif
  __asm__ __volatile__(
      "lock incb %0"
      : /* no output registers */
      : "m" (a[0])
      : "memory"
    );
#elif defined( KOKKOS_ENABLE_SERIAL_ATOMICS )
  char* a_nv = const_cast<char*>(a);
  ++(*a_nv);
#else
  Kokkos::atomic_fetch_add(a, char(1));
#endif
}

template<>
KOKKOS_INLINE_FUNCTION
void atomic_increment<short>(volatile short* a) {
#if defined( KOKKOS_ENABLE_ASM ) && defined( KOKKOS_ENABLE_ISA_X86_64 ) && ! defined(_WIN32) && ! defined(__CUDA_ARCH__)
#if defined( KOKKOS_ENABLE_RFO_PREFETCH )
  _mm_prefetch( (const char*) a, _MM_HINT_ET0 );
#endif
  __asm__ __volatile__(
      "lock incw %0"
      : /* no output registers */
      : "m" (a[0])
      : "memory"
    );
#elif defined( KOKKOS_ENABLE_SERIAL_ATOMICS )
  short* a_nv = const_cast<short*>(a);
  ++(*a_nv);
#else
  Kokkos::atomic_fetch_add(a, short(1));
#endif
}

template<>
KOKKOS_INLINE_FUNCTION
void atomic_increment<int>(volatile int* a) {
#if defined( KOKKOS_ENABLE_ASM ) && defined( KOKKOS_ENABLE_ISA_X86_64 ) && ! defined(_WIN32) && ! defined(__CUDA_ARCH__)
#if defined( KOKKOS_ENABLE_RFO_PREFETCH )
  _mm_prefetch( (const char*) a, _MM_HINT_ET0 );
#endif
  __asm__ __volatile__(
      "lock incl %0"
      : /* no output registers */
      : "m" (a[0])
      : "memory"
    );
#elif defined( KOKKOS_ENABLE_SERIAL_ATOMICS )
  int* a_nv = const_cast<int*>(a);
  ++(*a_nv);
#else
  Kokkos::atomic_fetch_add(a,int(1));
#endif
}

template<>
KOKKOS_INLINE_FUNCTION
void atomic_increment<long long int>(volatile long long int* a) {
#if defined( KOKKOS_ENABLE_ASM ) && defined( KOKKOS_ENABLE_ISA_X86_64 ) && ! defined(_WIN32) && ! defined(__CUDA_ARCH__)
#if defined( KOKKOS_ENABLE_RFO_PREFETCH )
  _mm_prefetch( (const char*) a, _MM_HINT_ET0 );
#endif
  __asm__ __volatile__(
      "lock incq %0"
      : /* no output registers */
      : "m" (a[0])
      : "memory"
    );
#elif defined( KOKKOS_ENABLE_SERIAL_ATOMICS )
  long long int* a_nv = const_cast<long long int*>(a);
  ++(*a_nv);
#else
  using T = long long int;
  Kokkos::atomic_fetch_add(a,T(1));
#endif
}

template<typename T>
KOKKOS_INLINE_FUNCTION
void atomic_increment(volatile T* a) {
#if defined( KOKKOS_ENABLE_SERIAL_ATOMICS )
  T* a_nv = const_cast<T*>(a);
  ++(*a_nv);
#else
  Kokkos::atomic_fetch_add(a,T(1));
#endif
}

} // End of namespace Kokkos
#endif

