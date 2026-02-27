#!/bin/bash

# Update test_compositor.salt to include GPU FFI and new logic
sed -i '' 's/extern fn facet_window_sleep_ms(ms: i32);/extern fn facet_window_sleep_ms(ms: i32);\nextern fn facet_gpu_init() -> i64;\nextern fn facet_gpu_create_buffer(device: i64, size_bytes: i64) -> i64;\nextern fn facet_gpu_buffer_contents(buffer: i64) -> Ptr<u8>;\nextern fn facet_gpu_compile_shader(device: i64, msl_source: Ptr<u8>, fn_name: Ptr<u8>) -> i64;\nextern fn facet_gpu_dispatch(device: i64, pipeline: i64, buffers: Ptr<i64>, buffer_count: i32, thread_count: i32);\nextern fn facet_gpu_destroy_buffer(buffer: i64);\nextern fn facet_gpu_destroy(device: i64);/g' test_compositor.salt

