#ifndef __KOKKOSKERNELS_GEMV_TEAM_INTERNAL_HPP__
#define __KOKKOSKERNELS_GEMV_TEAM_INTERNAL_HPP__


/// \author Kyungjoo Kim (kyukim@sandia.gov)

#include "KokkosKernels_Util.hpp"

#include "KokkosKernels_Set_Internal.hpp"
#include "KokkosKernels_Scale_Internal.hpp"

#include "KokkosKernels_InnerMultipleDotProduct_Serial_Impl.hpp"

namespace KokkosKernels {
  namespace Batched {
    namespace Experimental {

      ///
      /// Team Internal Impl
      /// ====================
      namespace Team {

        template<typename ArgAlgo>
        struct GemvInternal {
          template<typename MemberType,
                   typename ScalarType,
                   typename ValueType>
          KOKKOS_INLINE_FUNCTION
          static int
          invoke(const MemberType &member,
                 const int m, const int n, 
                 const ScalarType alpha,
                 const ValueType *__restrict__ A, const int as0, const int as1,
                 const ValueType *__restrict__ x, const int xs0, 
                 const ScalarType beta,
                 /**/  ValueType *__restrict__ y, const int ys0);
        };

        template<>
        template<typename MemberType,
                 typename ScalarType,
                 typename ValueType>
        KOKKOS_INLINE_FUNCTION
        int
        GemvInternal<Algo::Gemv::Unblocked>::
        invoke(const MemberType &member,
               const int m, const int n, 
               const ScalarType alpha,
               const ValueType *__restrict__ A, const int as0, const int as1,
               const ValueType *__restrict__ x, const int xs0,
               const ScalarType beta,
               /**/  ValueType *__restrict__ y, const int ys0) {
          // y = beta y + alpha A x
          // y (m), A(m x n), B(n)

          typedef ValueType value_type;

          if      (beta == 0) Team::SetInternal  ::invoke(member, m, value_type(0),    y, ys0);
          else if (beta != 1) Team::ScaleInternal::invoke(member, m, value_type(beta), y, ys0);
      
          if (alpha != 0) {
            if (m <= 0 || n <= 0) return 0;

            if (beta != 1) 
              member.barrier();
            
            Kokkos::parallel_for(Kokkos::TeamThreadRange(member,0,m),[&](const int &i) {
                value_type t(0);
                const value_type *__restrict__ tA = (A + i*as0);

                KOKKOSKERNELS_LOOP_UNROLL
                for (int j=0;j<n;++j)
                  t += tA[j*as1]*x[j*xs0];
                y[i*ys0] += alpha*t;
              });
          }
          return 0;
        }

        template<>
        template<typename MemberType,
                 typename ScalarType,
                 typename ValueType>
        KOKKOS_INLINE_FUNCTION
        int
        GemvInternal<Algo::Gemv::Blocked>::
        invoke(const MemberType &member,
               const int m, const int n, 
               const ScalarType alpha,
               const ValueType *__restrict__ A, const int as0, const int as1,
               const ValueType *__restrict__ x, const int xs0,
               const ScalarType beta,
               /**/  ValueType *__restrict__ y, const int ys0) {
          // y = beta y + alpha A x
          // y (m), A(m x n), B(n)

          typedef ValueType value_type;
          enum : int {
            mbAlgo = Algo::Gemv::Blocked::mb<Kokkos::Impl::ActiveExecutionMemorySpace>()
          };
          
          if      (beta == 0) Team::SetInternal  ::invoke(member, m, value_type(0),    y, ys0);
          else if (beta != 1) Team::ScaleInternal::invoke(member, m, value_type(beta), y, ys0);
      
          if (alpha != 0) {
            if (m <= 0 || n <= 0) return 0;

            if (beta != 1) 
              member.barrier();
        
            InnerMultipleDotProduct<mbAlgo> inner(as0, as1, xs0, ys0);
            const int tsize = member.team_size();
            const int mb = min(m/tsize + (m%tsize)>0, mbAlgo), mp = m%mb;
            
            Kokkos::parallel_for
              (Kokkos::TeamThreadRange(member, (m/mb) + (mp>0)),
               [&](const int &ii) {
                const int i = ii*mb;
                inner.serial_invoke(alpha, A+i*as0, x, (i+mb) > m ? (m-i) : mb, n, y+i*ys0 );
              });
            member.team_barrier();
          }
          
          return 0;
        }
      }
    } 
  }
}// end namespace KokkosKernels

#endif
