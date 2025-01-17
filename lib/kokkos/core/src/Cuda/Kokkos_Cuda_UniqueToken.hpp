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

#ifndef KOKKOS_CUDA_UNIQUE_TOKEN_HPP
#define KOKKOS_CUDA_UNIQUE_TOKEN_HPP

#include <Kokkos_Macros.hpp>
#ifdef KOKKOS_ENABLE_CUDA

#include <Kokkos_CudaSpace.hpp>
#include <Kokkos_UniqueToken.hpp>
#include <impl/Kokkos_SharedAlloc.hpp>
#include <impl/Kokkos_ConcurrentBitset.hpp>

namespace Kokkos { namespace Experimental {

// both global and instance Unique Tokens are implemented in the same way
template<>
class UniqueToken< Cuda, UniqueTokenScope::Global >
{
private:

  uint32_t volatile * m_buffer ;
  uint32_t            m_count ;

public:

  using execution_space = Cuda;

  explicit
  UniqueToken( execution_space const& );

  KOKKOS_INLINE_FUNCTION
  UniqueToken() : m_buffer(0), m_count(0) {}

  KOKKOS_FUNCTION_DEFAULTED
  UniqueToken( const UniqueToken & ) = default;

  KOKKOS_FUNCTION_DEFAULTED
  UniqueToken( UniqueToken && )      = default;

  KOKKOS_FUNCTION_DEFAULTED
  UniqueToken & operator=( const UniqueToken & ) = default ;

  KOKKOS_FUNCTION_DEFAULTED
  UniqueToken & operator=( UniqueToken && ) = default ;

  /// \brief upper bound for acquired values, i.e. 0 <= value < size()
  KOKKOS_INLINE_FUNCTION
  int32_t size() const noexcept { return m_count ; }

  /// \brief acquire value such that 0 <= value < size()
  KOKKOS_INLINE_FUNCTION
  int32_t acquire() const
  {
    const Kokkos::pair<int,int> result =
      Kokkos::Impl::concurrent_bitset::
        acquire_bounded( m_buffer
                       , m_count
                       , Kokkos::Impl::clock_tic() % m_count
                       );

   if ( result.first < 0 ) {
     Kokkos::abort("UniqueToken<Cuda> failure to release tokens, no tokens available" );
   }

    return result.first;
  }

  /// \brief release an acquired value
  KOKKOS_INLINE_FUNCTION
  void release( int32_t i ) const noexcept
  {
    Kokkos::Impl::concurrent_bitset::release( m_buffer, i );
  }
};

template<>
class UniqueToken< Cuda, UniqueTokenScope::Instance >
  : public UniqueToken< Cuda, UniqueTokenScope::Global >
{
public:

  explicit
  UniqueToken( execution_space const& arg )
    : UniqueToken< Cuda, UniqueTokenScope::Global >( arg ) {}
};

}} // namespace Kokkos::Experimental

#endif // KOKKOS_ENABLE_CUDA
#endif // KOKKOS_CUDA_UNIQUE_TOKEN_HPP

