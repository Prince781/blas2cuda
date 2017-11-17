#include "level1.h"

template <typename T>
static void _cblas_dotc_sub(const int n,
        const T *x,
        const int incx,
        const T *y,
        const int incy,
        T *dotc,
        cublasStatus_t dotc_func(cublasHandle_t, int, const T *, int, const T *, int, T *))
{
    const T *gpu_x, *gpu_y;
    const int size_x = size(n, incx, sizeof(*x));
    const int size_y = size(n, incy, sizeof(*y));
    const struct objinfo *x_info, *y_info;

    gpu_x = (const T *) b2c_place_on_gpu((void *) x, size_x, &x_info, NULL);
    gpu_y = (T *) b2c_place_on_gpu((void *) y, size_y, &y_info, 
            (void *) gpu_x, x_info,
            NULL);

    dotc_func(b2c_handle, n, gpu_x, incx, gpu_y, incy, dotc);

    if (cudaPeekAtLastError() != cudaSuccess)
        b2c_fatal_error(cudaGetLastError(), __func__);

    b2c_cleanup_gpu_ptr((void *) gpu_x, x_info);
    b2c_cleanup_gpu_ptr((void *) gpu_y, y_info);
}

DECLARE_CBLAS__DOTC(c, float _Complex) {
    _cblas_dotc_sub(n, (const cuComplex *) x, incx,
            (const cuComplex *) y, incy, (cuComplex *) dotc,
            &cublasCdotc);
}

DECLARE_CBLAS__DOTC(z, double _Complex) {
    _cblas_dotc_sub(n, (const cuDoubleComplex *) x, incx,
            (const cuDoubleComplex *) y, incy, (cuDoubleComplex *) dotc,
            &cublasZdotc);
}
