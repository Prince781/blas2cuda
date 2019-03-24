#include "blas2cuda.h"
#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>

#define BLAS2CUDA_OPTIONS "BLAS2CUDA_OPTIONS"

#include "lib/oracle.h"

static bool cublas_initialized = false;

static void *alloc_managed(size_t request);
static void *calloc_managed(size_t nmemb, size_t size);
static void *realloc_managed(void *managed_ptr, size_t request);
static void free_managed(void *managed_ptr);
static size_t get_size_managed(void *managed_ptr);

static bool b2c_initialized = false;

struct objmngr blas2cuda_manager = {
    .ctor = alloc_managed,
    .cctor = calloc_managed,
    .realloc = realloc_managed,
    .dtor = free_managed,
    .get_size = get_size_managed
};

static size_t hits = 0;
static size_t misses = 0;
static size_t total_managed_mem = 0;    /* in bytes */

#if USE_CUDA
cublasHandle_t b2c_handle;
#endif

bool b2c_must_synchronize = false;

struct b2c_options b2c_options = { false, false, false };

void b2c_print_help(void) {
    writef(STDERR_FILENO, 
            "blas2cuda options (set "BLAS2CUDA_OPTIONS"):\n"
            "   You can chain these options with a semicolon (;)\n"
            "   help            -- print help\n"
            "   debug_execfail  -- debug kernel failures\n"
            "   debug_exec      -- debug kernel invocations\n"
            "   trace_copy      -- trace copies between CPU and GPU\n"
            "   heuristic=<val> -- one of: 'random', 'true', 'false', or:\n"
            "                      'oracle:<filename>', where <filename> is\n"
            "                      the name of an object trace\n");
}

static void set_options(void) {
    /* TODO: use secure_getenv() ? */
    char *options = getenv(BLAS2CUDA_OPTIONS);
    char *saveptr = NULL;
    char *option = NULL;
    bool help = false;

    if (!options)
        return;

    option = strtok_r(options, ";", &saveptr);
    while (option != NULL) {
        if (strcmp(option, "help") == 0) {
            if (!help) {
                b2c_print_help();
                help = true;
            }
        } 
        else if (strcmp(option, "debug_execfail") == 0)
            b2c_options.debug_execfail = true;
        else if (strcmp(option, "debug_exec") == 0)
            b2c_options.debug_exec = true;
        else if (strcmp(option, "trace_copy") == 0)
            b2c_options.trace_copy = true;
        else if (strncmp(option, "heuristic=", 10) == 0) {
            char *hnum = strchr(option, '=');
            if (hnum) {
                bool set = false;

                hnum++;
                if (strncmp(hnum, "random", sizeof("random") - 1) == 0) {
                    hfunc = H_RANDOM;
                    set = true;
                } else if (strncmp(hnum, "true", sizeof("true") - 1) == 0) {
                    hfunc = H_TRUE;
                    set = true;
                } else if (strncmp(hnum, "false", sizeof("false") - 1) == 0) {
                    hfunc = H_FALSE;
                    set = true;
                } else if (strncmp(hnum, "oracle:", sizeof("oracle:") - 1) == 0) {
                    hfunc = H_ORACLE;
                    set = true;
                }

                if (set) {
                    writef(STDERR_FILENO, "blas2cuda: selecting heuristic %s\n", hnum);
                } else {
                    writef(STDERR_FILENO, "blas2cuda: unsupported heuristic '%s'\n", hnum);
                    abort();
                }

                if (hfunc == H_ORACLE) {
                    char *filename = strchr(hnum, ':') + 1;
                    
                    if (!oracle_load_file(filename)) {
                        writef(STDERR_FILENO, "blas2cuda:oracle: failed to load '%s':%m\n", filename);
                        abort();
                    }
                }
            }
        } else {
            writef(STDERR_FILENO, "blas2cuda: unknown option '%s'. Set "BLAS2CUDA_OPTIONS"=help.\n", option);
        }
        option = strtok_r(NULL, ";", &saveptr);
    }
}

#if USE_CUDA
void init_cublas(void) {
    static bool inside = false;
    if (!cublas_initialized && !inside) {
        inside = true;
        switch (cublasCreate(&b2c_handle)) {
            case CUBLAS_STATUS_SUCCESS:
                {
                    pid_t tid = syscall(SYS_gettid);
                    writef(STDOUT_FILENO, "blas2cuda: initialized cuBLAS on thread %d\n", tid);
                }
                break;
            case CUBLAS_STATUS_ALLOC_FAILED:
                writef(STDERR_FILENO, "blas2cuda: failed to allocate resources\n");
                break;
            case CUBLAS_STATUS_NOT_INITIALIZED:
            default:
                writef(STDERR_FILENO, "blas2cuda: failed to initialize cuBLAS\n");
                abort();
                break;
        }
        cublas_initialized = true;
        inside = false;
    }
}
#endif

void *b2c_copy_to_gpu(const void *hostbuf, size_t size)
{
    void *gpubuf = NULL;
    runtime_error_t err;

    obj_tracker_internal_enter();
    err = runtime_malloc(&gpubuf, size);

    if (!gpubuf || runtime_is_error(err)) {
        obj_tracker_internal_leave();
        return NULL;
    }

    err = runtime_memcpy_htod(gpubuf, hostbuf, size);

    if (b2c_options.trace_copy || runtime_is_error(err)) {
        writef(runtime_is_error(err) ? STDERR_FILENO : STDOUT_FILENO, 
                "blas2cuda: %s: %zu B : CPU ---> GPU%s\n", __func__, size, 
                runtime_is_error(err) ? " (failed)" : "");
        if (runtime_is_error(err))
            abort();
    }

    obj_tracker_internal_leave();
    return gpubuf;
}

void b2c_copy_from_gpu(void *hostbuf, const void *gpubuf, size_t size)
{
    runtime_error_t err;
    obj_tracker_internal_enter();
    err = runtime_memcpy_dtoh(hostbuf, gpubuf, size);

    if (b2c_options.trace_copy || runtime_is_error(err)) {
        writef(runtime_is_error(err) ? STDERR_FILENO : STDOUT_FILENO, 
                "blas2cuda: %s: %zu B : GPU ---> CPU%s\n", __func__, size,
                runtime_is_error(err) ? " (failed)" : "");
        if (runtime_is_error(err))
            abort();
    }

    obj_tracker_internal_leave();
}

void *b2c_place_on_gpu(void *hostbuf, 
                       size_t size,
                       const struct objinfo **info_in,
                       void *gpubuf2,
                       ...)
{
    void *gpubuf;
    const struct objinfo *gpubuf2_info;
    runtime_error_t err = RUNTIME_ERROR_SUCCESS;

    obj_tracker_internal_enter();
    if (!hostbuf) {
        err = runtime_malloc(&gpubuf, size);
    } else if ((*info_in = obj_tracker_objinfo_subptr(hostbuf))) {
        assert ((*info_in)->ptr == hostbuf);
        err = runtime_svm_unmap(hostbuf);
        gpubuf = hostbuf;
        hits++;
    } else {
        gpubuf = b2c_copy_to_gpu(hostbuf, size);
        misses++;
    }

    if (!gpubuf || runtime_is_error(err)) {
        va_list ap;

        va_start(ap, gpubuf2);

        writef(STDERR_FILENO, "blas2cuda: %s: failed to copy to GPU\n", __func__);

        if (gpubuf2) {
            do {
                gpubuf2_info = va_arg(ap, typeof(gpubuf2_info));
                b2c_cleanup_gpu_ptr(gpubuf2, gpubuf2_info);
            } while ((gpubuf2 = va_arg(ap, typeof(gpubuf2))));
        }

        va_end(ap);
        obj_tracker_internal_leave();
        return NULL;
    }

    obj_tracker_internal_leave();
    return gpubuf;
}

void b2c_cleanup_gpu_ptr(void *gpubuf, const struct objinfo *info)
{
    runtime_error_t err;
    obj_tracker_internal_enter();
    if (!info)
        err = runtime_free(gpubuf);
    else
        err = runtime_svm_map(gpubuf, info->size);
    obj_tracker_internal_leave();
    if (runtime_is_error(err)) {
        writef(STDERR_FILENO, "blas2cuda: %s: failed to cleanup %p: %s\n", __func__,
                gpubuf, runtime_error_name(err));
        abort();
    }
}


/* memory management */
static void *alloc_managed(size_t request)
{
    runtime_error_t err;
    void *ptr;

    obj_tracker_internal_enter();
    err = runtime_malloc_shared(&ptr, sizeof(size_t) + request);
    if (!runtime_is_error(err))
        err = runtime_svm_map(ptr + sizeof(size_t), request);
    if (runtime_is_error(err)) {
        writef(STDERR_FILENO, "blas2cuda: %s @ %s, line %d: failed to allocate %zu B: %s - %s\n", 
                __func__, __FILE__, __LINE__, sizeof(size_t) + request, 
                runtime_error_name(err), runtime_error_string(err));
        obj_tracker_internal_leave();
        abort();
    }

    total_managed_mem += sizeof(size_t) + request;
    *((size_t *)ptr) = request;
    obj_tracker_internal_leave();
    return ptr + sizeof(size_t);
}

static void *calloc_managed(size_t nmemb, size_t size) {
    void *ptr = alloc_managed(nmemb * size);
    memset(ptr, 0, nmemb * size);
    return ptr;
}

static void *realloc_managed(void *managed_ptr, size_t request)
{
    void *new_ptr;
    const size_t old_size = get_size_managed(managed_ptr);

    new_ptr = alloc_managed(request);

    memcpy(new_ptr, managed_ptr, old_size);
    free_managed(managed_ptr);
    return new_ptr;
}

static void free_managed(void *managed_ptr) {
    total_managed_mem -= sizeof(size_t) + get_size_managed(managed_ptr);
    runtime_free(managed_ptr - sizeof(size_t));
}

static size_t get_size_managed(void *managed_ptr) {
    return *(size_t *)(managed_ptr - sizeof(size_t));
}
/* memory management */

__attribute__((constructor))
void blas2cuda_init(void)
{
    static bool inside = false;
    pid_t tid;
    runtime_error_t rerr;
    runtime_init_info_t init_info = RUNTIME_INIT_INFO_DEFAULT;

    if (!b2c_initialized && !inside) {
        inside = true;
        tid = syscall(SYS_gettid);
        writef(STDOUT_FILENO, "blas2cuda: initializing on thread %d\n", tid);
        writef(STDOUT_FILENO, "blas2cuda: initializing cuBLAS...\n");
        rerr = runtime_init(init_info);
        if (runtime_is_error(rerr)) {
            writef(STDERR_FILENO, "blas2cuda: failed to initialize runtime\n");
            abort();
        }
#if USE_CUDA
        init_cublas();
        writef(STDOUT_FILENO, "blas2cuda: initialized cuBLAS\n");

        /* get device properties */
        int num_devices;
        cudaGetDeviceCount(&num_devices);
        for (int i=0; i < num_devices; i++) {
            struct cudaDeviceProp prop;
            cudaGetDeviceProperties(&prop, i);
            writef(STDOUT_FILENO, "CUDA device #%d {\n", i+1);
            writef(STDOUT_FILENO, "  name: %s\n", prop.name);
            writef(STDOUT_FILENO, "  total global memory: %zu\n", prop.totalGlobalMem);
            writef(STDOUT_FILENO, "  supports managed memory?: %s\n", prop.managedMemory ? "true" : "false");
            writef(STDOUT_FILENO, "  concurrent managed access?: %s\n", prop.concurrentManagedAccess ? "true" : "false");
            writef(STDOUT_FILENO, "}\n");

            b2c_must_synchronize |= !prop.concurrentManagedAccess;
        }

#else
        b2c_must_synchronize = true;
#endif

        /* initialize object tracker */
        obj_tracker_init(false);
        set_options();
        obj_tracker_set_tracking(true);

        /* add excluded regions */
#if USE_CUDA
        if (obj_tracker_find_excluded_regions("libcuda", "nvidia", NULL) < 0)
#elif USE_OPENCL
        if (obj_tracker_find_excluded_regions("libOpenCL", NULL) < 0)
#endif
            writef(STDERR_FILENO, "blas2cuda: error while finding excluded regions: %m\n");

        writef(STDOUT_FILENO, "blas2cuda: initialized on thread %d\n", tid);
        b2c_initialized = true;
        inside = false;
    }
}

__attribute__((destructor))
void blas2cuda_fini(void)
{
    static bool inside = false;
    pid_t tid;
    runtime_error_t rerr;

    if (!inside && b2c_initialized) {
        inside = true;
#if USE_CUDA
        if (cublas_initialized 
                && cublasDestroy(b2c_handle) == CUBLAS_STATUS_NOT_INITIALIZED)
            writef(STDERR_FILENO, "blas2cuda: failed to destroy. Not initialized\n");
#endif
        rerr = runtime_fini();
        if (runtime_is_error(rerr))
            writef(STDERR_FILENO, "blas2cuda: WARNING: failed to finalize runtime\n");

        tid = syscall(SYS_gettid);
        obj_tracker_fini();
        inside = false;
        b2c_initialized = false;

        int fd = open("statistics.csv", O_RDWR | O_CREAT, 0644);
        if (fd > -1) {
            writef(fd, "Hits, Misses\n");
            writef(fd, "%zu, %zu", hits, misses);
            close(fd);
        } else {
            writef(STDERR_FILENO, "blas2cuda: failed to write to statistics file: %s\n", strerror(errno));
        }

        writef(STDOUT_FILENO, "blas2cuda: decommissioned on thread %d\n", tid);
    }
}
