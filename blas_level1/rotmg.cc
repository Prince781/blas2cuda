#include <cublas_v2.h>
#include "../common.h"
#include "../cblas.h"
#include "../blas.h"
#include "../conversions.h"
#include "level1.h"
#include "../blas2cuda.h"

extern cublasHandle_t b2c_handle;

void cblas_srotmg (float *d1, 
        float *d2, 
        float *x1, 
        const float y1, 
        float *param)
{
    call_kernel(cublasSrotmg(b2c_handle, d1, d2, x1, &y1, param));
}

void cblas_drotmg (double *d1, 
        double *d2, 
        double *x1, 
        const double y1, 
        double *param)
{
    call_kernel(cublasDrotmg(b2c_handle, d1, d2, x1, &y1, param));
}

