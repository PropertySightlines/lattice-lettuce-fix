// C10M Benchmark: Arena Exhaustion (Rust Baseline)
// Tests tiered slab allocation under work stealing

use std::time::Instant;

const SLAB_64_COUNT: usize = 8192;
const SLAB_512_COUNT: usize = 2048;
const WORK_STEALING_ROUNDS: usize = 1_000_000;
const WORKERS: usize = 4;

struct TieredArena {
    slab_64: Vec<[u8; 64]>,
    slab_512: Vec<[u8; 512]>,
    bitmap_64: Vec<u64>,
    bitmap_512: Vec<u64>,
    alloc_count_64: i64,
    alloc_count_512: i64,
    free_count: i64,
}

impl TieredArena {
    fn new() -> Self {
        Self {
            slab_64: vec![[0u8; 64]; SLAB_64_COUNT],
            slab_512: vec![[0u8; 512]; SLAB_512_COUNT],
            bitmap_64: vec![!0u64; SLAB_64_COUNT / 64],
            bitmap_512: vec![!0u64; SLAB_512_COUNT / 64],
            alloc_count_64: 0,
            alloc_count_512: 0,
            free_count: 0,
        }
    }
    
    fn alloc_64(&mut self) -> Option<usize> {
        for i in 0..self.bitmap_64.len() {
            if self.bitmap_64[i] != 0 {
                let bit = self.bitmap_64[i].trailing_zeros() as usize;
                self.bitmap_64[i] &= !(1u64 << bit);
                self.alloc_count_64 += 1;
                return Some(i * 64 + bit);
            }
        }
        None
    }
    
    fn alloc_512(&mut self) -> Option<usize> {
        for i in 0..self.bitmap_512.len() {
            if self.bitmap_512[i] != 0 {
                let bit = self.bitmap_512[i].trailing_zeros() as usize;
                self.bitmap_512[i] &= !(1u64 << bit);
                self.alloc_count_512 += 1;
                return Some(i * 64 + bit);
            }
        }
        None
    }
    
    fn free_64(&mut self, idx: usize) {
        if idx < SLAB_64_COUNT {
            self.bitmap_64[idx / 64] |= 1u64 << (idx % 64);
            self.free_count += 1;
        }
    }
    
    fn free_512(&mut self, idx: usize) {
        if idx < SLAB_512_COUNT {
            self.bitmap_512[idx / 64] |= 1u64 << (idx % 64);
            self.free_count += 1;
        }
    }
    
    fn touch_64(&mut self, idx: usize, val: u8) {
        for b in &mut self.slab_64[idx] {
            *b = val;
        }
    }
    
    fn touch_512(&mut self, idx: usize, val: u8) {
        for b in &mut self.slab_512[idx] {
            *b = val;
        }
    }
}

fn work_stealing_simulation(arena: &mut TieredArena, worker_id: u8) {
    let mut frames_64 = Vec::with_capacity(32);
    let mut frames_512 = Vec::with_capacity(16);
    
    for _ in 0..32 {
        if let Some(idx) = arena.alloc_64() {
            arena.touch_64(idx, worker_id);
            frames_64.push(idx);
        }
    }
    
    for _ in 0..16 {
        if let Some(idx) = arena.alloc_512() {
            arena.touch_512(idx, worker_id);
            frames_512.push(idx);
        }
    }
    
    for idx in frames_64 {
        arena.free_64(idx);
    }
    for idx in frames_512 {
        arena.free_512(idx);
    }
}

fn main() {
    println!("=== C10M Arena Exhaustion Benchmark (Rust) ===");
    println!("Testing: Tiered slab allocation under work stealing");
    println!("Workers: {}, Rounds: {}\n", WORKERS, WORK_STEALING_ROUNDS);
    
    let mut arenas: Vec<TieredArena> = (0..WORKERS).map(|_| TieredArena::new()).collect();
    
    let start = Instant::now();
    
    for _ in 0..WORK_STEALING_ROUNDS {
        for w in 0..WORKERS {
            work_stealing_simulation(&mut arenas[w], w as u8);
        }
    }
    
    let elapsed = start.elapsed();
    
    let total_allocs: i64 = arenas.iter()
        .map(|a| a.alloc_count_64 + a.alloc_count_512)
        .sum();
    let total_frees: i64 = arenas.iter()
        .map(|a| a.free_count)
        .sum();
    
    let elapsed_ms = elapsed.as_secs_f64() * 1000.0;
    let allocs_per_sec = total_allocs as f64 / elapsed.as_secs_f64();
    let ns_per_alloc = elapsed.as_nanos() as f64 / total_allocs as f64;
    
    println!("=== Results ===");
    println!("Total allocations: {}", total_allocs);
    println!("Total frees: {}", total_frees);
    println!("Time: {:.2}ms", elapsed_ms);
    println!("Allocs/second: {:.0}", allocs_per_sec);
    println!("ns/alloc: {:.1}", ns_per_alloc);
    
    if ns_per_alloc < 50.0 {
        println!("✅ PASS: Allocation overhead acceptable");
    } else {
        println!("⚠️ WARN: Allocation overhead high (target: <50ns)");
    }
}
