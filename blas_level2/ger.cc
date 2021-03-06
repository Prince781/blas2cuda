#include <cublas_v2.h>
#include "../common.h"
#include "../cblas.h"
#include "../blas.h"
#include "../conversions.h"
#include "level2.h"
#include "../blas2cuda.h"

extern cublasHandle_t b2c_handle;

template <typename T>
static void _cblas_ger(const CBLAS_LAYOUT Layout,
        const int m, const int n,
        const T alpha,
        const T *x, const int incx,
        const T *y, const int incy,
        T *a, const int lda,
        cublasStatus_t ger_func(cublasHandle_t,
            int, int,
            const T *,
            const T *, int,
            const T *, int,
            T *, int),
        geam_t<T> geam_func)
{
    const T *gpu_x, *gpu_y;
    T *gpu_a;
    const int size_x = size(1, m - 1, incx, sizeof(*x));
    const int size_y = size(1, m - 1, incy, sizeof(*y));
    const int size_a = size(0, n, lda, sizeof(*a));
    const struct objinfo *x_info, *y_info, *a_info;
    int rows_a, cols_a;

    if (Layout == CblasRowMajor) {
        a_info = NULL;
        rows_a = m;
        cols_a = n;

        gpu_a = transpose(a, size_a, &rows_a, &cols_a, rows_a, geam_func);
    } else {
        rows_a = m;
        cols_a = n;
        gpu_a = (T *) b2c_place_on_gpu((void *) a, size_a, &a_info, NULL);
    }

    gpu_x = (const T *) b2c_place_on_gpu((void *) x, size_x, &x_info,
            (void *) gpu_a, a_info,
            NULL);
    gpu_y = (const T *) b2c_place_on_gpu((void *) y, size_y, &y_info,
            (void *) gpu_a, a_info,
            (void *) gpu_x, x_info,
            NULL);

    call_kernel(
        ger_func(b2c_handle, m, n,
                &alpha, 
                gpu_x, incx,
                gpu_y, incy,
                gpu_a, lda)
    );

    
    runtime_fatal_errmsg(cudaGetLastError(), __func__);

    if (!a_info) {
        if (Layout == CblasRowMajor)
            transpose(gpu_a, size_a, &rows_a, &cols_a, lda, geam_func);
        b2c_copy_from_gpu(a, gpu_a, size_a);
    }

    b2c_cleanup_gpu_ptr((void *) gpu_a, a_info);
    b2c_cleanup_gpu_ptr((void *) gpu_x, x_info);
    b2c_cleanup_gpu_ptr((void *) gpu_y, y_info);
}

DECLARE_CBLAS__GER(s, float) {
    _cblas_ger(Layout, m, n,
            alpha,
            x, incx,
            y, incy,
            a, lda,
            &cublasSger,
            &cublasSgeam);
}

DECLARE_CBLAS__GER(d, double) {
    _cblas_ger(Layout, m, n,
            alpha,
            x, incx,
            y, incy,
            a, lda,
            &cublasDger,
            &cublasDgeam);
}

DECLARE_CBLAS__GERC(c, float _Complex) {
    _cblas_ger(Layout, m, n,
            *(cuComplex *) alpha,
            (cuComplex *) x, incx,
            (cuComplex *) y, incy,
            (cuComplex *) a, lda,
            &cublasCgerc,
            &cublasCgeam);
}

DECLARE_CBLAS__GERC(z, double _Complex) {
    _cblas_ger(Layout, m, n,
            *(cuDoubleComplex *) alpha,
            (cuDoubleComplex *) x, incx,
            (cuDoubleComplex *) y, incy,
            (cuDoubleComplex *) a, lda,
            &cublasZgerc,
            &cublasZgeam);
}

DECLARE_CBLAS__GERU(c, float _Complex) {
    _cblas_ger(Layout, m, n,
            *(cuComplex *) alpha,
            (cuComplex *) x, incx,
            (cuComplex *) y, incy,
            (cuComplex *) a, lda,
            &cublasCgeru,
            &cublasCgeam);
}

DECLARE_CBLAS__GERU(z, double _Complex) {
    _cblas_ger(Layout, m, n,
            *(cuDoubleComplex *) alpha,
            (cuDoubleComplex *) x, incx,
            (cuDoubleComplex *) y, incy,
            (cuDoubleComplex *) a, lda,
            &cublasZgeru,
            &cublasZgeam);
}
