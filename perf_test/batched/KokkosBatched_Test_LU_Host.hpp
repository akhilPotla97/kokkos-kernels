/// \author Kyungjoo Kim (kyukim@sandia.gov)

#define __KOKKOSBATCHED_INTEL_MKL__
//#define __KOKKOSBATCHED_INTEL_MKL_BATCHED__
#include <iomanip>

#if defined(__KOKKOSBATCHED_INTEL_MKL__)
#include "mkl.h"
#endif

#include "Kokkos_Core.hpp"
#include "impl/Kokkos_Timer.hpp"

#include "KokkosBatched_Vector.hpp"

#include "KokkosBatched_LU_Decl.hpp"
#include "KokkosBatched_LU_Serial_Impl.hpp"

namespace KokkosBatched {
  namespace Experimental {

    namespace PerfTest {

#undef FLOP_MUL
#undef FLOP_ADD

      // no complex yet
#if defined( KokkosBatched_Test_LU_Host_Complex )      
#define FLOP_MUL 6.0
#define FLOP_ADD 2.0
      typedef Kokkos::complex<double> value_type;
#endif

#if defined( KokkosBatched_Test_LU_Host_Real )      
#define FLOP_MUL 1.0
#define FLOP_ADD 1.0      
      typedef double value_type;
#endif

      double FlopCount(int mm, int nn) {
        double m = (double)mm;    double n = (double)nn;
        if (m > n)
          return (FLOP_MUL*(0.5*m*n*n-(1.0/6.0)*n*n*n+0.5*m*n-0.5*n*n+(2.0/3.0)*n) +
                  FLOP_ADD*(0.5*m*n*n-(1.0/6.0)*n*n*n-0.5*m*n+        (1.0/6.0)*n));
        else
          return (FLOP_MUL*(0.5*n*m*m-(1.0/6.0)*m*m*m+0.5*n*m-0.5*m*m+(2.0/3.0)*m) +
                  FLOP_ADD*(0.5*n*m*m-(1.0/6.0)*m*m*m-0.5*n*m+        (1.0/6.0)*m));
      }

      template<int BlkSize, typename HostSpaceType, typename AlgoTagType>
      void LU(const int NN) {
        typedef Kokkos::Schedule<Kokkos::Static> ScheduleType;
        //typedef Kokkos::Schedule<Kokkos::Dynamic> ScheduleType;

        constexpr int VectorLength = DefaultVectorLength<value_type,typename HostSpaceType::memory_space>::value;
        const int N = NN/VectorLength;

        {
          std::string value_type_name;
          if (std::is_same<value_type,double>::value)                   value_type_name = "double";
          if (std::is_same<value_type,Kokkos::complex<double> >::value) value_type_name = "Kokkos::complex<double>";

#if   defined(__AVX__) || defined(__AVX2__)
          std::cout << "AVX or AVX2 is defined: datatype " << value_type_name <<  " a vector length " << VectorLength << "\n";
#elif defined(__AVX512F__)
          std::cout << "AVX512 is defined: datatype " << value_type_name <<  " a vector length " << VectorLength << "\n";
#else
          std::cout << "SIMD (compiler vectorization) is defined: datatype " << value_type_name <<  " a vector length " << VectorLength << "\n";
#endif
        }

        const double flop = (N*VectorLength)*FlopCount(BlkSize,BlkSize);
        const double tmax = 1.0e15;

        typedef typename
          Kokkos::Impl::is_space<HostSpaceType>::host_mirror_space::execution_space HostSpaceType ;
        const int iter_begin = -10, iter_end = 100;
        Kokkos::Impl::Timer timer;

        ///
        /// Reference version using MKL DGETRF
        ///
        Kokkos::View<value_type***,Kokkos::LayoutRight,HostSpaceType> aref;
        Kokkos::View<value_type***,Kokkos::LayoutRight,HostSpaceType>
          amat("amat", N*VectorLength, BlkSize, BlkSize);

        Random<value_type> random;

        for (int k=0;k<N*VectorLength;++k) {
          // use tridiagonal matrices; for now we just check elementwise l/u factors
          // do not allow pivots
          for (int i=0;i<BlkSize;++i) {
            amat(k, i, i) = random.value() + 10.0;
            if ((i+1) < BlkSize) {
              amat(k, i, i+1) = random.value() + 1.0;
              amat(k, i+1, i) = random.value() + 1.0;
            }
          }
        }

        typedef Vector<SIMD<value_type>,VectorLength> VectorType;
        Kokkos::View<VectorType***,Kokkos::LayoutRight,HostSpaceType>
          amat_simd("amat_simd", N, BlkSize, BlkSize), a("a", N, BlkSize, BlkSize);
      
        Kokkos::parallel_for
          (Kokkos::RangePolicy<HostSpaceType>(0, N*VectorLength),
           KOKKOS_LAMBDA(const int k) {
            const int k0 = k/VectorLength, k1 = k%VectorLength;
            for (int i=0;i<BlkSize;++i)
              for (int j=0;j<BlkSize;++j) {
                amat_simd(k0, i, j)[k1] = amat(k0*VectorLength+k1, i, j);
              }
          });

        // for KNL
        constexpr size_t LLC_CAPACITY = 34*1024*1024;
        Flush<LLC_CAPACITY> flush;

        ///
        /// Reference version using MKL DGETRF
        ///
#if defined(__KOKKOSBATCHED_INTEL_MKL__)
        {
          Kokkos::View<value_type***,Kokkos::LayoutRight,HostSpaceType> a("a", N*VectorLength, BlkSize, BlkSize);
          Kokkos::View<int**,Kokkos::LayoutRight,HostSpaceType> p("p", N*VectorLength, BlkSize);
          {
            double tavg = 0, tmin = tmax;
            for (int iter=iter_begin;iter<iter_end;++iter) {
              // flush
              flush.run();

              // initialize matrix
              Kokkos::deep_copy(a, amat);

              HostSpaceType::fence();
              timer.reset();

              Kokkos::RangePolicy<HostSpaceType,ScheduleType> policy(0, N*VectorLength);
              Kokkos::parallel_for
                (policy,
                 KOKKOS_LAMBDA(const int k) {
                  auto aa = Kokkos::subview(a, k, Kokkos::ALL(), Kokkos::ALL());
                  auto pp = Kokkos::subview(p, k, Kokkos::ALL());
                  LAPACKE_dgetrf(LAPACK_ROW_MAJOR,
                                 BlkSize, BlkSize,
                                 (double*)aa.data(), aa.stride_0(),
                                 (int*)pp.data());
                });

              HostSpaceType::fence();
              const double t = timer.seconds();
              tmin = std::min(tmin, t);
              tavg += (iter >= 0)*t;
            }
            tavg /= iter_end;

            std::cout << std::setw(10) << "MKL LU"
                      << " BlkSize = " << std::setw(3) << BlkSize
                      << " time = " << std::scientific << tmin
                      << " avg flop/s = " << (flop/tavg)
                      << " max flop/s = " << (flop/tmin)
                      << std::endl;
          }

          aref = a;
        }

#if defined(__KOKKOSBATCHED_INTEL_MKL_BATCHED__)
#endif

#if defined(__KOKKOSBATCHED_INTEL_MKL_COMPACT_BATCHED__)
        {
          Kokkos::View<VectorType***,Kokkos::LayoutRight,HostSpaceType>
            a("a", N, BlkSize, BlkSize);
        
          {
            double tavg = 0, tmin = tmax;

            MKL_INT blksize[1] = { BlkSize };
            MKL_INT lda[1] = { a.stride_1() };
            MKL_INT size_per_grp[1] = { N*VectorLength };

            compact_t A_p;
            A_p.layout = CblasRowMajor; 
            A_p.rows = blksize;
            A_p.cols = blksize;
            A_p.stride = lda;
            A_p.group_count = 1;
            A_p.size_per_group = size_per_grp;
            A_p.format = VectorLength;
            A_p.mat = (double*)a.data();

            for (int iter=iter_begin;iter<iter_end;++iter) {
              // flush
              flush.run();

              // initialize matrix
              Kokkos::deep_copy(a, amat_simd);

              HostSpaceType::fence();
              timer.reset();

              LAPACKE_dgetrf_compute_batch(&A_p);
            
              HostSpaceType::fence();
              const double t = timer.seconds();
              tmin = std::min(tmin, t);
              tavg += (iter >= 0)*t;
            }
            tavg /= iter_end;

            double diff = 0;
            for (int i=0;i<aref.dimension(0);++i)
              for (int j=0;j<aref.dimension(1);++j)
                for (int k=0;k<aref.dimension(2);++k)
                  diff += abs(aref(i,j,k) - a(i/VectorLength,j,k)[i%VectorLength]);

            std::cout << std::setw(10) << "MKL Cmpt"
                      << " BlkSize = " << std::setw(3) << BlkSize
                      << " time = " << std::scientific << tmin
                      << " avg flop/s = " << (flop/tavg)
                      << " max flop/s = " << (flop/tmin)
                      << " diff to ref = " << diff
                      << std::endl;
          }
        }
#endif

#endif
        ///
        /// Plain version (comparable to micro BLAS version)
        ///

        {
          Kokkos::View<value_type***,Kokkos::LayoutRight,HostSpaceType>
            a("a", N*VectorLength, BlkSize, BlkSize);

          {
            double tavg = 0, tmin = tmax;
            for (int iter=iter_begin;iter<iter_end;++iter) {
              // flush
              flush.run();

              // initialize matrix
              Kokkos::deep_copy(a, amat);

              HostSpaceType::fence();
              timer.reset();

              Kokkos::RangePolicy<HostSpaceType,ScheduleType> policy(0, N*VectorLength);
              Kokkos::parallel_for
                (policy,
                 KOKKOS_LAMBDA(const int k) {
                  auto aa = Kokkos::subview(a, k, Kokkos::ALL(), Kokkos::ALL());

                  SerialLU<AlgoTagType>::invoke(aa);
                });

              HostSpaceType::fence();
              const double t = timer.seconds();
              tmin = std::min(tmin, t);
              tavg += (iter >= 0)*t;
            }
            tavg /= iter_end;

            double diff = 0;
            for (int i=0;i<aref.dimension(0);++i)
              for (int j=0;j<aref.dimension(1);++j)
                for (int k=0;k<aref.dimension(2);++k)
                  diff += abs(aref(i,j,k) - a(i,j,k));

            std::cout << std::setw(10) << "Plain"
                      << " BlkSize = " << std::setw(3) << BlkSize
                      << " time = " << std::scientific << tmin
                      << " avg flop/s = " << (flop/tavg)
                      << " max flop/s = " << (flop/tmin)
                      << " diff to ref = " << diff
                      << std::endl;
          }
        }

        ///
        /// SIMD with appropriate data layout
        ///

        {
          Kokkos::View<VectorType***,Kokkos::LayoutRight,HostSpaceType>
            a("a", N, BlkSize, BlkSize);
        
          {
            double tavg = 0, tmin = tmax;
            for (int iter=iter_begin;iter<iter_end;++iter) {
              // flush
              flush.run();

              // initialize matrix
              Kokkos::deep_copy(a, amat_simd);

              HostSpaceType::fence();
              timer.reset();

              Kokkos::RangePolicy<HostSpaceType,ScheduleType > policy(0, N);
              Kokkos::parallel_for
                (policy,
                 KOKKOS_LAMBDA(const int k) {
                  auto aa = Kokkos::subview(a, k, Kokkos::ALL(), Kokkos::ALL());

                  SerialLU<AlgoTagType>::invoke(aa);
                });

              HostSpaceType::fence();
              const double t = timer.seconds();
              tmin = std::min(tmin, t);
              tavg += (iter >= 0)*t;
            }
            tavg /= iter_end;

            double diff = 0;
            for (int i=0;i<aref.dimension(0);++i)
              for (int j=0;j<aref.dimension(1);++j)
                for (int k=0;k<aref.dimension(2);++k)
                  diff += abs(aref(i,j,k) - a(i/VectorLength,j,k)[i%VectorLength]);
            std::cout << std::setw(10) << "SIMD"
                      << " BlkSize = " << std::setw(3) << BlkSize
                      << " time = " << std::scientific << tmin
                      << " avg flop/s = " << (flop/tavg)
                      << " max flop/s = " << (flop/tmin)
                      << " diff to ref = " << diff
                      << std::endl;
          }
        }
      }
    }
  }
}


