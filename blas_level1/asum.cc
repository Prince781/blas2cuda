#include <cublas_v2.h>
#include "../common.h"
#include "../cblas.h"
#include "../blas.h"
#include "../conversions.h"
#include "level1.h"
#include "../blas2cuda.h"

template <typename T, typename R>
static void _cblas_asum (const int n,
        const T *x, 
        const int incx, 
        R *result,
        cublasStatus_t asum_func(cublasHandle_t, const int, const T *, const int, R *))
{
    const T *gpu_x;
    const int size_x = size(1, n-1, incx, sizeof(*x));
    const struct objinfo *x_info;
    extern cublasHandle_t b2c_handle;

    gpu_x = (const T *) b2c_place_on_gpu((void *) x, size_x, &x_info, NULL);

    call_kernel(asum_func(b2c_handle, n, gpu_x, incx, result));

    
    runtime_fatal_errmsg(cudaGetLastError(), __func__);

    b2c_cleanup_gpu_ptr((void *) gpu_x, x_info);
}

DECLARE_CBLAS__ASUM(s, float) {
    float result;

    _cblas_asum(n, x, incx, &result, &cublasSasum);
    return result;
}

DECLARE_CBLAS__ASUM(sc, float _Complex) {
    float result;

    _cblas_asum(n, (cuComplex *) x, incx, &result, &cublasScasum);
    return result;
}

DECLARE_CBLAS__ASUM(d, double) {
    double result;

    _cblas_asum(n, x, incx, &result, &cublasDasum);
    return result;
}

DECLARE_CBLAS__ASUM(dz, double _Complex) {
    double result;

    _cblas_asum(n, (cuDoubleComplex *) x, incx, &result, &cublasDzasum);
    return result;
}
