#if USE_CUDA
#include <cublas_v2.h>
#endif
#include "../common.h"
#include "../cblas.h"
#include "../blas.h"
#include "../conversions.h"
#include "level3.h"
#include "../runtime-blas.h"
#include "../runtime-mem.hpp"

#if USE_CUDA
extern cublasHandle_t b2c_cublas_handle;
#else
extern cl_command_queue opencl_cmd_queue;
#endif


template <typename T, typename S>
#if USE_CUDA
using herk_t = cublasStatus_t (*)(cublasHandle_t,
                cublasFillMode_t, cublasOperation_t,
                int, int,
                const S *,
                const T *, int,
                const S *,
                T *, int);
#else
using herk_t = clblasStatus (*)(clblasOrder order, 
        clblasUplo uplo, 
        clblasTranspose transA, 
        size_t N, size_t K, 
        S alpha, 
        const cl_mem A, size_t offa, size_t lda, 
        S beta, cl_mem C, size_t offc, size_t ldc, 
        cl_uint numCommandQueues, cl_command_queue *commandQueues, 
        cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *events);
#endif

template <typename T, typename S>
void _b2c_herk(const CBLAS_UPLO uplo,
        const CBLAS_TRANSPOSE trans,
        const int n, const int k,
        const S alpha,
        const T *a, const int lda,
        const S beta,
        T *c, const int ldc,
        herk_t<T,S> herk_func)
{
    gpuptr<const T> gpu_a(a, size(0, lda, trans == CblasNoTrans ? k : n, sizeof *a));
    gpuptr<T> gpu_c(c, size(0, ldc, n, sizeof *c));

    call_kernel(
#if USE_CUDA
        herk_func(b2c_cublas_handle,
                cu(uplo), cu(trans),
                n, k,
                &alpha,
                gpu_a, lda,
                &beta,
                gpu_c, ldc)
#else
        herk_func(clblasColumnMajor,
            clb(uplo), clb(trans),
            n, k,
            alpha,
            gpu_a, 0, lda,
            beta,
            gpu_c, 0, ldc,
            1, &opencl_cmd_queue, 0, NULL, NULL)
#endif
    );
}

#define herk_check()\
do {\
    int nrowa = runtime_blas_lsame(trans, "N") ? *n : *k;\
    int upper = runtime_blas_lsame(uplo, "U");\
    int info = 0;\
\
    if (!upper && !runtime_blas_lsame(uplo, "L"))\
        info = 1;\
    else if (!runtime_blas_lsame(trans, "N") && !runtime_blas_lsame(trans, "C"))\
        info = 2;\
    else if (*n < 0)\
        info = 3;\
    else if (*k < 0)\
        info = 4;\
    else if (*lda < std::max(1, nrowa))\
        info = 7;\
    else if (*ldc < std::max(1,*n))\
        info = 10;\
    if (info != 0) {\
        runtime_blas_xerbla(__func__, info);\
        return;\
    }\
\
    if (*n == 0 || (((*alpha == 0) || (*k == 0)) && (*beta == 1)))\
        return;\
\
    if (*alpha == 0 || *n == 0 || *k == 0) {\
        if (upper) {\
            if (*beta == 0) {\
                for (int j=1; j<=*n; ++j)\
                    for (int i=1; i<=j; ++i)\
                        c[IDX2F(i,j,*ldc)] = 0;\
            } else {\
                for (int j=1; j<=*n; ++j) {\
                    for (int i=1; i<=j-1; ++i)\
                        c[IDX2F(i,j,*ldc)] *= *beta;\
                    c[IDX2F(j,j,*ldc)] = *beta * creal(c[IDX2F(j,j,*ldc)]);\
                }\
            }\
        } else {\
            if (*beta == 0) {\
                for (int j=1; j<=*n; ++j)\
                    for (int i=j; i<=*n; ++i)\
                        c[IDX2F(i,j,*ldc)] = 0;\
            } else {\
                for (int j=1; j<=*n; ++j) {\
                    c[IDX2F(j,j,*ldc)] = *beta * creal(c[IDX2F(j,j,*ldc)]);\
                    for (int i=j+1; i<=*n; ++i)\
                        c[IDX2F(i,j,*ldc)] *= *beta;\
                }\
            }\
        }\
        return;\
    }\
} while (0)

F77_herk(c, float, float _Complex) {
    herk_check();
    _b2c_herk(c_uplo(*uplo), c_trans(*trans),
            *n, *k,
            *alpha,
            cmplx_ptr(a), *lda,
            *beta,
            cmplx_ptr(c), *ldc,
#if USE_CUDA
            &cublasCherk
#else
            &clblasCherk
#endif
            );
}

F77_herk(z, double, double _Complex) {
    herk_check();
    _b2c_herk(c_uplo(*uplo), c_trans(*trans),
            *n, *k,
            *alpha,
            cmplx_ptr(a), *lda,
            *beta,
            cmplx_ptr(c), *ldc,
#if USE_CUDA
            &cublasZherk
#else
            &clblasZherk
#endif
            );
}
