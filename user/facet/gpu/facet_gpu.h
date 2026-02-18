/*
 * Facet GPU Bridge — Metal Compute API
 *
 * Provides C-callable functions for Salt to create Metal GPU resources,
 * compile shaders, and dispatch compute workloads.
 *
 * Compile: clang -ObjC -framework Metal -fobjc-arc
 */

#ifndef FACET_GPU_H
#define FACET_GPU_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialize Metal GPU device and command queue.
 * Returns an opaque handle (cast to i64 in Salt), 0 on failure.
 */
int64_t facet_gpu_init(void);

/*
 * Allocate a shared-memory Metal buffer of the given size in bytes.
 * Returns an opaque buffer handle, 0 on failure.
 */
int64_t facet_gpu_create_buffer(int64_t device, int64_t size_bytes);

/*
 * Get CPU-visible pointer to buffer contents (shared memory mode).
 * Returns a pointer that Salt can read/write directly.
 */
void *facet_gpu_buffer_contents(int64_t buffer);

/*
 * Get the size of a buffer in bytes.
 */
int64_t facet_gpu_buffer_length(int64_t buffer);

/*
 * Compile MSL source into a compute pipeline.
 * msl_source: null-terminated MSL source string.
 * fn_name: null-terminated name of the kernel function entry point.
 * Returns an opaque pipeline handle, 0 on failure.
 */
int64_t facet_gpu_compile_shader(int64_t device, const char *msl_source,
                                 const char *fn_name);

/*
 * Dispatch a compute shader.
 * pipeline: handle from facet_gpu_compile_shader.
 * buffers: array of buffer handles to bind as [[buffer(0)]], [[buffer(1)]], ...
 * buffer_count: number of buffers.
 * thread_count: total number of threads to launch.
 * Blocks until GPU execution completes.
 */
void facet_gpu_dispatch(int64_t device, int64_t pipeline,
                        const int64_t *buffers, int32_t buffer_count,
                        int32_t thread_count);

/*
 * Release a Metal buffer.
 */
void facet_gpu_destroy_buffer(int64_t buffer);

/*
 * Release the GPU device and command queue.
 */
void facet_gpu_destroy(int64_t device);

#ifdef __cplusplus
}
#endif

#endif /* FACET_GPU_H */
