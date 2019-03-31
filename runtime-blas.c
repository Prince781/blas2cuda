#include "runtime-blas.h"
#include <stdbool.h>

#if USE_CUDA
cublasStatus_t b2c_cublas_handle;
#endif

runtime_blas_error_t runtime_blas_init(void) {
#if USE_CUDA
    return cublasCreate(&b2c_cublas_handle);
#else
    return CL_SUCCESS;
#endif
}

runtime_blas_error_t runtime_blas_fini(void) {
#if USE_CUDA
    return cublasDestroy(b2c_cublas_handle);
#else
    return CL_SUCCESS;
#endif
}
