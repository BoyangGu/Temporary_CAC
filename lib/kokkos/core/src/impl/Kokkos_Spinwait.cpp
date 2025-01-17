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

#include <Kokkos_Macros.hpp>
#if defined( KOKKOS_ACTIVE_EXECUTION_MEMORY_SPACE_HOST )

#include <Kokkos_Atomic.hpp>
#include <impl/Kokkos_Spinwait.hpp>
#include <impl/Kokkos_BitOps.hpp>

#if defined( KOKKOS_ENABLE_STDTHREAD) || defined( _WIN32 )
  #include <thread>
#elif !defined( _WIN32 )
  #include <sched.h>
  #include <time.h>
#else
  #include <process.h>
  #include <winsock2.h>
  #include <windows.h>
#endif

/*--------------------------------------------------------------------------*/

namespace Kokkos {
namespace Impl {

void host_thread_yield( const uint32_t i , const WaitMode mode )
{
  static constexpr uint32_t sleep_limit = 1 << 13 ;
  static constexpr uint32_t yield_limit = 1 << 12 ;

  const int c = Kokkos::Impl::bit_scan_reverse(i);

  if ( WaitMode::ROOT != mode ) {
    if ( sleep_limit < i ) {

      // Attempt to put the thread to sleep for 'c' milliseconds

      #if defined( KOKKOS_ENABLE_STDTHREAD ) || defined( _WIN32 )
        auto start = std::chrono::high_resolution_clock::now();
        std::this_thread::yield();
        std::this_thread::sleep_until( start + std::chrono::nanoseconds( c * 1000 ) );
      #else
        timespec req ;
        req.tv_sec  = 0 ;
        req.tv_nsec = 1000 * c ;
        nanosleep( &req, nullptr );
      #endif
    }

    else if ( mode == WaitMode::PASSIVE || yield_limit < i ) {

      // Attempt to yield thread resources to runtime

      #if defined( KOKKOS_ENABLE_STDTHREAD ) || defined( _WIN32 )
        std::this_thread::yield();
      #else
        sched_yield();
      #endif
    }

    #if defined( KOKKOS_ENABLE_ASM )

    else if ( (1u<<4) < i ) {

      // Insert a few no-ops to quiet the thread:

      for ( int k = 0 ; k < c ; ++k ) {
        #if defined( __amd64 ) || defined( __amd64__ ) || \
              defined( __x86_64 ) || defined( __x86_64__ )
          #if !defined( _WIN32 ) /* IS NOT Microsoft Windows */
            asm volatile( "nop\n" );
          #else
            __asm__ __volatile__( "nop\n" );
          #endif
        #elif defined(__PPC64__)
            asm volatile( "nop\n" );
        #endif
      }
    }
    #endif /* defined( KOKKOS_ENABLE_ASM ) */
  }
  #if defined( KOKKOS_ENABLE_ASM )
  else if ( (1u<<3) < i ) {
    // no-ops for root thread
    for ( int k = 0 ; k < c ; ++k ) {
      #if defined( __amd64 ) || defined( __amd64__ ) || \
            defined( __x86_64 ) || defined( __x86_64__ )
        #if !defined( _WIN32 ) /* IS NOT Microsoft Windows */
          asm volatile( "nop\n" );
        #else
          __asm__ __volatile__( "nop\n" );
        #endif
      #elif defined(__PPC64__)
          asm volatile( "nop\n" );
      #endif
    }
  }

  {
    // Insert memory pause
      #if defined( __amd64 )  || defined( __amd64__ ) || \
       	  defined( __x86_64 ) || defined( __x86_64__ )
    	  #if !defined( _WIN32 ) /* IS NOT Microsoft Windows */
          asm volatile( "pause\n":::"memory" );
	      #else
          __asm__ __volatile__( "pause\n":::"memory" );
        #endif
      #elif defined(__PPC64__)
	      asm volatile( "or 27, 27, 27" ::: "memory" );
      #endif
  }

  #endif /* defined( KOKKOS_ENABLE_ASM ) */
}

}} // namespace Kokkos::Impl

#else
void KOKKOS_CORE_SRC_IMPL_SPINWAIT_PREVENT_LINK_ERROR() {}
#endif

