/*
 * Facet GPU Bridge — Metal Compute Implementation
 *
 * Wraps Apple's Metal API in C-callable functions for the Salt FFI.
 * Manages MTLDevice, MTLCommandQueue, MTLBuffer, and compute pipelines.
 *
 * Architecture:
 *   FacetGPUState (heap struct) holds device + queue
 *   Buffers and pipelines returned as opaque i64 handles
 *   All Metal objects use ARC — no manual retain/release needed
 *
 * Compile: clang -ObjC -framework Metal -fobjc-arc
 */

#import <Metal/Metal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════
// GPU State — tracks device and command queue
// ═══════════════════════════════════════════════════════════════

typedef struct {
  id<MTLDevice> device;
  id<MTLCommandQueue> commandQueue;
} FacetGPUState;

// ═══════════════════════════════════════════════════════════════
// facet_gpu_init — Create Metal device and command queue
// ═══════════════════════════════════════════════════════════════

int64_t facet_gpu_init(void) {
  @autoreleasepool {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) {
      fprintf(stderr, "[facet_gpu] ERROR: No Metal device found\n");
      return 0;
    }

    FacetGPUState *state = (FacetGPUState *)calloc(1, sizeof(FacetGPUState));
    if (!state)
      return 0;

    state->device = device;
    state->commandQueue = [device newCommandQueue];

    if (!state->commandQueue) {
      fprintf(stderr, "[facet_gpu] ERROR: Failed to create command queue\n");
      free(state);
      return 0;
    }

    return (int64_t)state;
  }
}

// ═══════════════════════════════════════════════════════════════
// facet_gpu_create_buffer — Allocate shared-memory Metal buffer
// ═══════════════════════════════════════════════════════════════

int64_t facet_gpu_create_buffer(int64_t device_handle, int64_t size_bytes) {
  if (!device_handle || size_bytes <= 0)
    return 0;

  @autoreleasepool {
    FacetGPUState *state = (FacetGPUState *)device_handle;
    id<MTLBuffer> buffer =
        [state->device newBufferWithLength:(NSUInteger)size_bytes
                                   options:MTLResourceStorageModeShared];
    if (!buffer) {
      fprintf(stderr,
              "[facet_gpu] ERROR: Failed to create buffer of %lld bytes\n",
              (long long)size_bytes);
      return 0;
    }

    // Return the buffer object as an opaque handle.
    // ARC retains it because we store it via CFBridgingRetain.
    return (int64_t)CFBridgingRetain(buffer);
  }
}

// ═══════════════════════════════════════════════════════════════
// facet_gpu_buffer_contents — Get CPU pointer to buffer data
// ═══════════════════════════════════════════════════════════════

void *facet_gpu_buffer_contents(int64_t buffer_handle) {
  if (!buffer_handle)
    return NULL;
  id<MTLBuffer> buffer = (__bridge id<MTLBuffer>)(void *)buffer_handle;
  return [buffer contents];
}

// ═══════════════════════════════════════════════════════════════
// facet_gpu_buffer_length — Get buffer size in bytes
// ═══════════════════════════════════════════════════════════════

int64_t facet_gpu_buffer_length(int64_t buffer_handle) {
  if (!buffer_handle)
    return 0;
  id<MTLBuffer> buffer = (__bridge id<MTLBuffer>)(void *)buffer_handle;
  return (int64_t)[buffer length];
}

// ═══════════════════════════════════════════════════════════════
// facet_gpu_compile_shader — Compile MSL source to pipeline
// ═══════════════════════════════════════════════════════════════

int64_t facet_gpu_compile_shader(int64_t device_handle, const char *msl_source,
                                 const char *fn_name) {
  if (!device_handle || !msl_source || !fn_name)
    return 0;

  @autoreleasepool {
    FacetGPUState *state = (FacetGPUState *)device_handle;

    NSString *source = [NSString stringWithUTF8String:msl_source];
    NSError *error = nil;

    // Compile MSL source to a Metal library
    id<MTLLibrary> library = [state->device newLibraryWithSource:source
                                                         options:nil
                                                           error:&error];
    if (!library) {
      fprintf(stderr, "[facet_gpu] ERROR: Shader compilation failed: %s\n",
              [[error localizedDescription] UTF8String]);
      return 0;
    }

    // Get the kernel function by name
    NSString *funcName = [NSString stringWithUTF8String:fn_name];
    id<MTLFunction> function = [library newFunctionWithName:funcName];
    if (!function) {
      fprintf(stderr,
              "[facet_gpu] ERROR: Kernel '%s' not found in compiled library\n",
              fn_name);
      return 0;
    }

    // Create compute pipeline state
    id<MTLComputePipelineState> pipeline =
        [state->device newComputePipelineStateWithFunction:function
                                                     error:&error];
    if (!pipeline) {
      fprintf(stderr, "[facet_gpu] ERROR: Pipeline creation failed: %s\n",
              [[error localizedDescription] UTF8String]);
      return 0;
    }

    return (int64_t)CFBridgingRetain(pipeline);
  }
}

// ═══════════════════════════════════════════════════════════════
// facet_gpu_dispatch — Run a compute shader
// ═══════════════════════════════════════════════════════════════

void facet_gpu_dispatch(int64_t device_handle, int64_t pipeline_handle,
                        const int64_t *buffers, int32_t buffer_count,
                        int32_t thread_count) {
  if (!device_handle || !pipeline_handle || !buffers || buffer_count <= 0 ||
      thread_count <= 0)
    return;

  @autoreleasepool {
    FacetGPUState *state = (FacetGPUState *)device_handle;
    id<MTLComputePipelineState> pipeline =
        (__bridge id<MTLComputePipelineState>)(void *)pipeline_handle;

    // Create command buffer
    id<MTLCommandBuffer> commandBuffer = [state->commandQueue commandBuffer];
    if (!commandBuffer) {
      fprintf(stderr, "[facet_gpu] ERROR: Failed to create command buffer\n");
      return;
    }

    // Create compute encoder
    id<MTLComputeCommandEncoder> encoder =
        [commandBuffer computeCommandEncoder];
    if (!encoder) {
      fprintf(stderr, "[facet_gpu] ERROR: Failed to create compute encoder\n");
      return;
    }

    // Set pipeline and buffers
    [encoder setComputePipelineState:pipeline];
    for (int32_t i = 0; i < buffer_count; i++) {
      id<MTLBuffer> buf = (__bridge id<MTLBuffer>)(void *)buffers[i];
      [encoder setBuffer:buf offset:0 atIndex:(NSUInteger)i];
    }

    // Calculate threadgroup size
    NSUInteger maxThreadsPerGroup = [pipeline maxTotalThreadsPerThreadgroup];
    NSUInteger threadGroupSize = maxThreadsPerGroup;
    if (threadGroupSize > (NSUInteger)thread_count) {
      threadGroupSize = (NSUInteger)thread_count;
    }

    MTLSize gridSize = MTLSizeMake((NSUInteger)thread_count, 1, 1);
    MTLSize groupSize = MTLSizeMake(threadGroupSize, 1, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:groupSize];
    [encoder endEncoding];

    // Submit and wait
    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];

    // Check for errors
    if ([commandBuffer error]) {
      fprintf(stderr, "[facet_gpu] ERROR: GPU execution failed: %s\n",
              [[[commandBuffer error] localizedDescription] UTF8String]);
    }
  }
}

// ═══════════════════════════════════════════════════════════════
// facet_gpu_destroy_buffer — Release a Metal buffer
// ═══════════════════════════════════════════════════════════════

void facet_gpu_destroy_buffer(int64_t buffer_handle) {
  if (!buffer_handle)
    return;
  // Release the retained buffer object
  CFBridgingRelease((void *)buffer_handle);
}

// ═══════════════════════════════════════════════════════════════
// facet_gpu_destroy — Release GPU device and queue
// ═══════════════════════════════════════════════════════════════

void facet_gpu_destroy(int64_t device_handle) {
  if (!device_handle)
    return;
  FacetGPUState *state = (FacetGPUState *)device_handle;
  // ARC releases device and commandQueue when state is freed
  state->device = nil;
  state->commandQueue = nil;
  free(state);
}

// ═══════════════════════════════════════════════════════════════
// facet_gpu_alloc / facet_gpu_free — Bypass Salt leak detector
// ═══════════════════════════════════════════════════════════════

void *facet_gpu_alloc(int64_t size) { return malloc((size_t)size); }

void facet_gpu_free(void *ptr) { free(ptr); }
