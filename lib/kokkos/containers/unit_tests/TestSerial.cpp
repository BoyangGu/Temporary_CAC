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
#ifdef KOKKOS_ENABLE_SERIAL

#include <gtest/gtest.h>

#include <Kokkos_Core.hpp>

#include <Kokkos_Bitset.hpp>
#include <Kokkos_UnorderedMap.hpp>
#include <Kokkos_Vector.hpp>

#include <TestBitset.hpp>
#include <TestUnorderedMap.hpp>
#include <TestStaticCrsGraph.hpp>
#include <TestVector.hpp>
#include <TestDualView.hpp>
#include <TestDynamicView.hpp>
#include <TestScatterView.hpp>

#include <iomanip>

#include <Kokkos_DynRankView.hpp>
#include <TestDynViewAPI.hpp>

#include <Kokkos_ErrorReporter.hpp>
#include <TestErrorReporter.hpp>

#include <TestViewCtorPropEmbeddedDim.hpp>

namespace Test {

class serial : public ::testing::Test {
protected:
  static void SetUpTestCase () {
    std::cout << std::setprecision(5) << std::scientific;
  }

  static void TearDownTestCase () {
  }
};

TEST_F( serial, dyn_view_api) {
  TestDynViewAPI< double , Kokkos::Serial >();
}

TEST_F( serial, viewctorprop_embedded_dim ) {
  TestViewCtorProp_EmbeddedDim< Kokkos::Serial >::test_vcpt( 2, 3 );
}

TEST_F( serial , staticcrsgraph )
{
  TestStaticCrsGraph::run_test_graph< Kokkos::Serial >();
  TestStaticCrsGraph::run_test_graph2< Kokkos::Serial >();
  TestStaticCrsGraph::run_test_graph3< Kokkos::Serial >(1, 0);
  TestStaticCrsGraph::run_test_graph3< Kokkos::Serial >(1, 1000);
  TestStaticCrsGraph::run_test_graph3< Kokkos::Serial >(1, 10000);
  TestStaticCrsGraph::run_test_graph3< Kokkos::Serial >(1, 100000);
  TestStaticCrsGraph::run_test_graph3< Kokkos::Serial >(3, 0);
  TestStaticCrsGraph::run_test_graph3< Kokkos::Serial >(3, 1000);
  TestStaticCrsGraph::run_test_graph3< Kokkos::Serial >(3, 10000);
  TestStaticCrsGraph::run_test_graph3< Kokkos::Serial >(3, 100000);
  TestStaticCrsGraph::run_test_graph3< Kokkos::Serial >(75, 0);
  TestStaticCrsGraph::run_test_graph3< Kokkos::Serial >(75, 1000);
  TestStaticCrsGraph::run_test_graph3< Kokkos::Serial >(75, 10000);
  TestStaticCrsGraph::run_test_graph3< Kokkos::Serial >(75, 100000);
}

TEST_F( serial, bitset )
{
  test_bitset<Kokkos::Serial> ();
}

#define SERIAL_INSERT_TEST( name, num_nodes, num_inserts, num_duplicates, repeat, near ) \
  TEST_F( serial, UnorderedMap_insert_##name##_##num_nodes##_##num_inserts##_##num_duplicates##_##repeat##x) { \
    for (int i=0; i<repeat; ++i)                                        \
      test_insert<Kokkos::Serial> (num_nodes, num_inserts, num_duplicates, near); \
  }

#define SERIAL_FAILED_INSERT_TEST( num_nodes, repeat )                  \
  TEST_F( serial, UnorderedMap_failed_insert_##num_nodes##_##repeat##x) { \
    for (int i=0; i<repeat; ++i)                                        \
      test_failed_insert<Kokkos::Serial> (num_nodes);                   \
  }

#define SERIAL_ASSIGNEMENT_TEST( num_nodes, repeat )                    \
  TEST_F( serial, UnorderedMap_assignment_operators_##num_nodes##_##repeat##x) { \
    for (int i=0; i<repeat; ++i)                                        \
      test_assignement_operators<Kokkos::Serial> (num_nodes);           \
  }

#define SERIAL_DEEP_COPY( num_nodes, repeat )                           \
  TEST_F( serial, UnorderedMap_deep_copy##num_nodes##_##repeat##x) {    \
    for (int i=0; i<repeat; ++i)                                        \
      test_deep_copy<Kokkos::Serial> (num_nodes);                       \
  }

#define SERIAL_VECTOR_COMBINE_TEST( size )             \
  TEST_F( serial, vector_combination##size##x) {                        \
    test_vector_combinations<int,Kokkos::Serial>(size);                 \
  }

#define SERIAL_DUALVIEW_COMBINE_TEST( size )             \
  TEST_F( serial, dualview_combination##size##x) {                      \
    test_dualview_combinations<int,Kokkos::Serial>(size);               \
  }

#define SERIAL_SCATTERVIEW_TEST( size )             \
  TEST_F( serial, scatterview_##size##x) {                      \
    test_scatter_view<Kokkos::Serial>(size);               \
  }

SERIAL_INSERT_TEST(close, 100000, 90000, 100, 500, true)
SERIAL_INSERT_TEST(far, 100000, 90000, 100, 500, false)
SERIAL_FAILED_INSERT_TEST( 10000, 1000 )
SERIAL_DEEP_COPY( 10000, 1 )

SERIAL_VECTOR_COMBINE_TEST( 10 )
SERIAL_VECTOR_COMBINE_TEST( 3057 )
SERIAL_DUALVIEW_COMBINE_TEST( 10 )

SERIAL_SCATTERVIEW_TEST( 10 )

SERIAL_SCATTERVIEW_TEST( 1000000 )

#undef SERIAL_INSERT_TEST
#undef SERIAL_FAILED_INSERT_TEST
#undef SERIAL_ASSIGNEMENT_TEST
#undef SERIAL_DEEP_COPY
#undef SERIAL_VECTOR_COMBINE_TEST
#undef SERIAL_DUALVIEW_COMBINE_TEST

TEST_F( serial , dynamic_view )
{
  typedef TestDynamicView< double , Kokkos::Serial >
    TestDynView ;

  for ( int i = 0 ; i < 10 ; ++i ) {
    TestDynView::run( 100000 + 100 * i );
  }
}

#if defined(KOKKOS_CLASS_LAMBDA)
TEST_F(serial, ErrorReporterViaLambda)
{
  TestErrorReporter<ErrorReporterDriverUseLambda<Kokkos::Serial>>();
}
#endif

TEST_F(serial, ErrorReporter)
{
  TestErrorReporter<ErrorReporterDriver<Kokkos::Serial>>();
}


} // namespace Test

#else
void KOKKOS_CONTAINERS_UNIT_TESTS_TESTSERIAL_PREVENT_EMPTY_LINK_ERROR() {}
#endif // KOKKOS_ENABLE_SERIAL

