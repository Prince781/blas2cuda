#include <cublas_v2.h>
#include "../common.h"
#include "../cblas.h"
#include "../blas.h"
#include "../conversions.h"
#include "level2.h"
#include "../blas2cuda.h"

extern cublasHandle_t b2c_handle;

template <typename T>
void _cblas_gbmv (const CBLAS_LAYOUT Layout,
        CBLAS_TRANSPOSE trans,
        const int m, const int n,
        const int kl, const int ku,
        const T alpha,
        const T *A, const int lda,
        const T *x, const int incx,
        const T beta,
        T *y, const int incy,
        cublasStatus_t gbmv_func(cublasHandle_t,
            cublasOperation_t,
            int, int, int, int,
            const T *,
            const T *, int,
            const T *, int,
            const T *,
            T *, int),
        geam_t<T> geam_func)
{
    const T *gpu_A, *gpu_x;
    T *gpu_y;
    const int size_A = size(0, n, lda, sizeof(*A));
    const int size_x = size(1, (trans == CblasNoTrans ? n : m) - 1, incx, sizeof(*x));
    const int size_y = size(1, (trans == CblasNoTrans ? n : m) - 1, incy, sizeof(*y));
    const struct objinfo *A_info, *x_info, *y_info;
    int rows_A, cols_A;
    cublasOperation_t op = cu(trans);

    if (Layout == CblasRowMajor) {
        /* create a new buffer that is the transpose matrix of A*/

        A_info = NULL;
        rows_A = m;
        cols_A = n;

        gpu_A = transpose(A, size_A, &rows_A, &cols_A, rows_A, geam_func);
    } else {
        gpu_A = (const T *) b2c_place_on_gpu((void *) A, size_A, &A_info, NULL);
    }

    gpu_x = (const T *) b2c_place_on_gpu((void *) x, size_x, &x_info,
            (void *) gpu_A, A_info,
            NULL);
    gpu_y = (T *) b2c_place_on_gpu(NULL, size_y, &y_info,
            (void *) gpu_A, A_info,
            (void *) gpu_x, x_info,
            NULL);

    call_kernel(
        gbmv_func(b2c_handle, op,
                m, n, kl, ku, 
                &alpha, 
                gpu_A, lda,
                gpu_x, incx,
                &beta,
                gpu_y, incy)
    );

    
    runtime_fatal_errmsg(cudaGetLastError(), __func__);

    if (!y_info)
        b2c_copy_from_gpu(y, gpu_y, size_y);

    b2c_cleanup_gpu_ptr((void *) gpu_A, A_info);
    b2c_cleanup_gpu_ptr((void *) gpu_x, x_info);
    b2c_cleanup_gpu_ptr((void *) gpu_y, y_info);
}

DECLARE_CBLAS__GBMV(s, float) {
    _cblas_gbmv(Layout, trans, 
            m, n, kl, ku, 
            alpha, 
            A, lda,
            x, incx,
            beta,
            y, incy, &cublasSgbmv, &cublasSgeam);
}

DECLARE_CBLAS__GBMV(d, double) {
    _cblas_gbmv(Layout, trans, 
            m, n, kl, ku, 
            alpha, 
            A, lda,
            x, incx,
            beta,
            y, incy, &cublasDgbmv, &cublasDgeam);
}

DECLARE_CBLAS__GBMV(c, float _Complex) {
    _cblas_gbmv(Layout, trans, 
            m, n, kl, ku, 
            cu(alpha), 
            (cuComplex *) A, lda,
            (cuComplex *) x, incx,
            cu(beta),
            (cuComplex *) y, incy,
            &cublasCgbmv, &cublasCgeam);
}

DECLARE_CBLAS__GBMV(z, double _Complex) {
    _cblas_gbmv(Layout, trans, 
            m, n, kl, ku, 
            cu(alpha), 
            (cuDoubleComplex *) A, lda,
            (cuDoubleComplex *) x, incx,
            cu(beta),
            (cuDoubleComplex *) y, incy, 
            &cublasZgbmv, &cublasZgeam);
}

