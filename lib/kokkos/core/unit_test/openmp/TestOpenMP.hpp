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

#ifndef KOKKOS_TEST_OPENMP_HPP
#define KOKKOS_TEST_OPENMP_HPP

#include <gtest/gtest.h>

#include <Kokkos_Macros.hpp>

#ifdef KOKKOS_LAMBDA
#undef KOKKOS_LAMBDA
#endif
#define KOKKOS_LAMBDA [=]

#include <Kokkos_Core.hpp>

#include <TestTile.hpp>
#include <TestSharedAlloc.hpp>
#include <TestViewMapping.hpp>
#include <TestViewAPI.hpp>
#include <TestViewOfClass.hpp>
#include <TestViewSubview.hpp>
#include <TestAtomic.hpp>
#include <TestAtomicOperations.hpp>
#include <TestAtomicViews.hpp>
#include <TestRange.hpp>
#include <TestTeam.hpp>
#include <TestReduce.hpp>
#include <TestScan.hpp>
#include <TestAggregate.hpp>
#include <TestCompilerMacros.hpp>
#include <TestTaskScheduler.hpp>
#include <TestMemoryPool.hpp>
#include <TestCXX11.hpp>
#include <TestCXX11Deduction.hpp>
#include <TestTeamVector.hpp>
#include <TestTemplateMetaFunctions.hpp>
#include <TestPolicyConstruction.hpp>
#include <TestMDRange.hpp>
#include <TestConcurrentBitset.hpp>

namespace Test {

class openmp : public ::testing::Test {
protected:
  static void SetUpTestCase()
  {
    int threads_count = 0;
    #pragma omp parallel
    {
      #pragma omp atomic
      ++threads_count;
    }

    if (threads_count > 3) {
      threads_count /= 2;
    }

    Kokkos::OpenMP::initialize( threads_count );
    Kokkos::print_configuration( std::cout, true );

    srand( 10231 );
  }

  static void TearDownTestCase()
  {
    Kokkos::OpenMP::finalize();
  }
};

} // namespace Test

#endif
