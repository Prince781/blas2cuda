#ifndef BLAS2CUDA_H
#define BLAS2CUDA_H

#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

#include <cuda.h>
#include <cuda_runtime.h>
#include <cublas_v2.h>

#include "lib/callinfo.h"
#include "lib/obj_tracker.h"

#define B2C_ERRORCHECK(name, status) \
do {\
    if (b2c_options.debug_execfail) {\
        switch (status) {\
            case CUBLAS_STATUS_EXECUTION_FAILED:\
                fprintf(stderr, "blas2cuda: failed to execute " #name "\n");\
                break;\
            case CUBLAS_STATUS_NOT_INITIALIZED:\
                fprintf(stderr, "blas2cuda: not initialized\n");\
                break;\
            case CUBLAS_STATUS_ARCH_MISMATCH:\
                fprintf(stderr, "blas2cuda:" #name " not supported\n");\
                break;\
            default:\
                break;\
        }\
    }\
    if (status == CUBLAS_STATUS_SUCCESS && b2c_options.debug_exec)\
        fprintf(stderr, "blas2cuda: calling %s()\n", #name);\
} while (0)

struct options {
    bool debug_execfail;
    bool debug_exec;
    bool trace_copy;
};

extern struct options b2c_options;
extern cublasHandle_t b2c_handle;

#ifdef __cplusplus
extern "C" {
#endif

void init_cublas(void);

/**
 * If status is not "cudaSuccess", prints the error message and exits.
 */
static inline void b2c_fatal_error(cudaError_t status, const char *domain)
{
    if (status != cudaSuccess) {
        fprintf(stderr, "%s: %s : %s\n", domain, cudaGetErrorName(status), cudaGetErrorString(status));
        abort();
    }
}

/**
 * Creates a new GPU buffer and copies the CPU buffer to it.
 * Returns the GPU buffer, which may be NULL on failure.
 * Free with cudaFree().
 */
void *b2c_copy_to_gpu(const void *cpubuf, size_t size);

/**
 * Creates a new CPU buffer and copies the GPU buffer to it.
 * Returns the CPU buffer, which may be NULL on failure.
 * Free with free().
 */
void *b2c_copy_to_cpu(const void *gpubuf, size_t size);

/**
 * Copies an existing GPU buffer to an existing CPU buffer.
 */
void b2c_copy_from_gpu(void *cpubuf, const void *gpubuf, size_t size);

/**
 * If the buffer is on the CPU, copies it to the GPU and returns a GPU pointer.
 * {*info_in} is set to NULL. If the buffer is NULL, returns a new GPU buffer
 * and {*info_in} is set to NULL.
 * If the buffer is shared, returns the same pointer. {*info_in} points to the
 * object tracking information for this buffer.
 * If there is a fatal error, this function will abort.
 * The remaining arguments are each a pair (gpubuf, gpubuf_info) to perform cleanup on
 * in the event of an error. The list is terminated by a NULL pointer for a
 * gpubuf.
 */
void *b2c_place_on_gpu(void *cpubuf, 
        size_t size,
        const struct objinfo **info_in,
        void *gpubuf2,
        ...);

/**
 * If {info} is NULL, the GPU buffer will be freed with cudaFree(). Otherwise
 * nothing will happen.
 * If there is a fatal error, this function will abort.
 */
void b2c_cleanup_gpu_ptr(void *gpubuf, const struct objinfo *info);

#ifdef __cplusplus
};
#endif

#endif
