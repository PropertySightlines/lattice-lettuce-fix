import sys

with open('user/facet/compositor/test_compositor.salt', 'r') as f:
    content = f.read()

# Revert Canvas back to raw ptr 
content = content.replace(
    'struct Canvas { pixels: Ptr<u8>, width: i32, height: i32, stride: i32, gpu_buf: i64, device: i64, pipe_clear: i64, pipe_raster: i64 }',
    'struct Canvas { pixels: Ptr<u8>, width: i32, height: i32, stride: i32 }'
)
# Make RenderEdge exactly 20 bytes (no _pad)
content = content.replace(
    'struct RenderEdge { x0: f32, y0: f32, x1: f32, y1: f32, dir: f32, _pad: i32 }',
    'struct RenderEdge { x0: f32, y0: f32, x1: f32, y1: f32, dir: f32 }'
)
content = content.replace(
    'struct RenderEdge { x0: f32, y0: f32, x1: f32, y1: f32, dir: f32 }',
    'struct RenderEdge { x0: f32, y0: f32, x1: f32, y1: f32, dir: f32 }'
)

# Replace canvas_new to use malloc again
new_canvas_new = '''fn canvas_new(width: i32, height: i32) -> Canvas {
    let stride = width * 4;
    let bytes = (stride as i64) * (height as i64);
    let mem = malloc(bytes);
    return Canvas { pixels: mem, width: width, height: height, stride: stride };
}'''
import re
content = re.sub(r'fn canvas_new\([^)]*\)\s*->\s*Canvas\s*\{[^}]*\}', new_canvas_new, content)

# Replace canvas_clear to use CPU loop
new_canvas_clear = '''fn canvas_clear(c: &mut Canvas, r: u8, g: u8, b: u8, a: u8) {
    let mut y: i32 = 0;
    while y < c.height {
        let mut x: i32 = 0;
        while x < c.width {
            let off = ((y as i64) * (c.stride as i64)) + ((x as i64) * 4);
            c.pixels.offset(off).write(r);
            c.pixels.offset(off+1).write(g);
            c.pixels.offset(off+2).write(b);
            c.pixels.offset(off+3).write(a);
            x = x + 1;
        }
        y = y + 1;
    }
}'''
content = re.sub(r'fn canvas_clear\([^)]*\)\s*\{[^}]*\}', new_canvas_clear, content)

# Replace canvas_free
content = content.replace('fn canvas_free(c: &mut Canvas) { facet_gpu_destroy_buffer(c.gpu_buf); }', 'fn canvas_free(c: &mut Canvas) { free(c.pixels); }')

# Replace et_new capacity
content = content.replace('let mem = malloc(cap * 20);', 'let mem = malloc(cap * 20);')

# In main, remove facet_gpu_init logic and device tracking
import re
main_gpu_init = r'''let device = facet_gpu_init\(\);.*?let mut canvas = canvas_new\(device, pipe_clear, pipe_raster, width, height\);'''
content = re.sub(main_gpu_init, 'facet_gpu_compositor_init();\n    let mut canvas = canvas_new(width, height);', content, flags=re.DOTALL)
content = content.replace('facet_gpu_destroy(device);', '')

with open('user/facet/compositor/test_compositor.salt', 'w') as f:
    f.write(content)

with open('user/facet/gpu/facet_gpu.m', 'r') as f:
    m_content = f.read()

# Replace the facet_gpu_rasterize_edges implementation with the user's exactly!
target_c_api = '''typedef struct __attribute__((packed)) {
    float x0;
    float y0;
    float x1;
    float y1;
    float dir;
} RenderEdge;

typedef struct __attribute__((packed)) {
    int width;
    int height;
    int edge_count;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} RenderParams;

static id<MTLDevice> global_device = nil;
static id<MTLCommandQueue> global_queue = nil;
static id<MTLComputePipelineState> pso_rasterize = nil;

void facet_gpu_compositor_init(void) {
    if (global_device) return;
    
    global_device = MTLCreateSystemDefaultDevice();
    global_queue = [global_device newCommandQueue];
    
    NSString* msl_source = @"#include <metal_stdlib>\\n"
                           "using namespace metal;\\n"
                           "struct Edge { float x0; float y0; float x1; float y1; float dir; };\\n"
                           "struct RenderParams { int width; int height; int edge_count; uint8_t r; uint8_t g; uint8_t b; uint8_t a; };\\n"
                           "kernel void rasterize_edges(device uint8_t* canvas [[buffer(0)]], \\n"
                           "                            device const Edge* edges [[buffer(1)]], \\n"
                           "                            constant RenderParams& params [[buffer(2)]], \\n"
                           "                            uint2 tid [[thread_position_in_grid]]) {\\n"
                           "    if (tid.x >= (uint)params.width || tid.y >= (uint)params.height) return;\\n"
                           "    float py = float(tid.y) + 0.5;\\n"
                           "    float px = float(tid.x) + 0.5;\\n"
                           "    float winding = 0.0;\\n"
                           "    for (int i = 0; i < params.edge_count; i++) {\\n"
                           "        float y0 = edges[i].y0;\\n"
                           "        float y1 = edges[i].y1;\\n"
                           "        if (py >= y0 && py < y1) {\\n"
                           "            float t = (py - y0) / (y1 - y0);\\n"
                           "            float ix = edges[i].x0 + t * (edges[i].x1 - edges[i].x0);\\n"
                           "            if (ix <= px) { winding += edges[i].dir; }\\n"
                           "        }\\n"
                           "    }\\n"
                           "    if (abs(winding) > 0.001) {\\n"
                           "        int off = (tid.y * params.width + tid.x) * 4;\\n"
                           "        uint8_t a = params.a;\\n"
                           "        if (a == 255) {\\n"
                           "            canvas[off] = params.r; canvas[off+1] = params.g; canvas[off+2] = params.b; canvas[off+3] = a;\\n"
                           "        } else if (a > 0) {\\n"
                           "            int dst_r = canvas[off]; int dst_g = canvas[off+1]; int dst_b = canvas[off+2]; int dst_a = canvas[off+3];\\n"
                           "            int inv_a = 255 - a;\\n"
                           "            canvas[off] = (params.r + (dst_r * inv_a) / 255); canvas[off+1] = (params.g + (dst_g * inv_a) / 255);\\n"
                           "            canvas[off+2] = (params.b + (dst_b * inv_a) / 255); canvas[off+3] = (a + (dst_a * inv_a) / 255);\\n"
                           "        }\\n"
                           "    }\\n"
                           "}";

    NSError* error = nil;
    id<MTLLibrary> library = [global_device newLibraryWithSource:msl_source options:nil error:&error];
    if (!library) {
        NSLog(@"FATAL: Salt GPU Bridge failed to compile MSL: %@", error);
        exit(1);
    }
    
    id<MTLFunction> func = [library newFunctionWithName:@"rasterize_edges"];
    pso_rasterize = [global_device newComputePipelineStateWithFunction:func error:&error];
    if (!pso_rasterize) {
        NSLog(@"FATAL: Salt GPU Bridge failed to create pipeline state: %@", error);
        exit(1);
    }
}

void facet_gpu_rasterize_edges(uint8_t* canvas, RenderEdge* edges, RenderParams params) {
    if (!global_device) facet_gpu_compositor_init();

    id<MTLCommandBuffer> cmd_buffer = [global_queue commandBuffer];
    id<MTLComputeCommandEncoder> encoder = [cmd_buffer computeCommandEncoder];
    [encoder setComputePipelineState:pso_rasterize];

    size_t canvas_size = params.width * params.height * 4;
    size_t edges_size = params.edge_count * sizeof(RenderEdge);
    
    id<MTLBuffer> buf_canvas = [global_device newBufferWithBytesNoCopy:canvas 
                                                                  length:canvas_size 
                                                                 options:MTLResourceStorageModeShared 
                                                             deallocator:nil];
                                                             
    id<MTLBuffer> buf_edges = [global_device newBufferWithBytesNoCopy:edges 
                                                                 length:edges_size 
                                                                options:MTLResourceStorageModeShared 
                                                            deallocator:nil];

    [encoder setBuffer:buf_canvas offset:0 atIndex:0];
    [encoder setBuffer:buf_edges offset:0 atIndex:1];
    [encoder setBytes:&params length:sizeof(RenderParams) atIndex:2];

    MTLSize threads_per_group = MTLSizeMake(16, 16, 1);
    MTLSize threadgroups = MTLSizeMake((params.width + 15) / 16, 
                                       (params.height + 15) / 16, 
                                       1);
                                       
    [encoder dispatchThreadgroups:threadgroups threadsPerThreadgroup:threads_per_group];
    [encoder endEncoding];
    
    [cmd_buffer commit];
    [cmd_buffer waitUntilCompleted];
}'''

start_idx = m_content.find('typedef struct __attribute__((packed)) {\\n    float x0;')
if start_idx != -1:
    m_content = m_content[:start_idx] + target_c_api + '\\n'
    
with open('user/facet/gpu/facet_gpu.m', 'w') as f:
    f.write(m_content)

