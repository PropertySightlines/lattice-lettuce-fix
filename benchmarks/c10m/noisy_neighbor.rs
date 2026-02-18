// C10M Benchmark: Noisy Neighbor (Rust Baseline)
// Tests scheduler fairness - simulates 1000Hz vs 60Hz interleaved work

use std::collections::HashMap;
use std::time::Instant;

const RING_BUFFER_SIZE: usize = 1024;
const BENCHMARK_DURATION_MS: u64 = 5000;
const HASHMAP_SIZE: usize = 100_000;
const LOOKUPS_PER_FRAME: usize = 5_000_000;

#[derive(Clone, Copy, Default)]
struct SensorData {
    timestamp: i64,
    gyro_x: f32,
    gyro_y: f32,
    gyro_z: f32,
}

struct RingBuffer {
    data: [SensorData; RING_BUFFER_SIZE],
    head: usize,
    tail: usize,
    count: usize,
}

impl RingBuffer {
    fn new() -> Self {
        Self {
            data: [SensorData::default(); RING_BUFFER_SIZE],
            head: 0,
            tail: 0,
            count: 0,
        }
    }
    
    fn push(&mut self, item: SensorData) {
        self.data[self.head] = item;
        self.head = (self.head + 1) % RING_BUFFER_SIZE;
        if self.count < RING_BUFFER_SIZE {
            self.count += 1;
        } else {
            self.tail = (self.tail + 1) % RING_BUFFER_SIZE;
        }
    }
    
    fn drain(&mut self) -> usize {
        let drained = self.count;
        self.count = 0;
        self.head = 0;
        self.tail = 0;
        drained
    }
}

fn pseudo_random_f32(seed: i64) -> f32 {
    let mut x = seed ^ (seed >> 21);
    x ^= x << 35;
    x ^= x >> 4;
    (x % 1000) as f32 / 1000.0
}

fn main() {
    println!("=== C10M Noisy Neighbor Benchmark (Rust) ===");
    println!("Testing: 1000Hz sensor vs 60Hz heavy compute");
    println!("Duration: {} ms\n", BENCHMARK_DURATION_MS);
    
    // Pre-create hashmap with data
    let mut map: HashMap<i64, i64> = HashMap::with_capacity(HASHMAP_SIZE);
    for i in 0..HASHMAP_SIZE as i64 {
        map.insert(i, i * 2);
    }
    
    let mut buffer = RingBuffer::new();
    let mut max_jitter_us: i64 = 0;
    let mut sensor_tick_count: i64 = 0;
    let mut last_sensor_ns: i64 = 0;
    
    let start = Instant::now();
    let duration_ns = BENCHMARK_DURATION_MS as i64 * 1_000_000;
    
    let mut next_sensor_ns: i64 = 0;
    let mut next_compute_ns: i64 = 0;
    let sensor_interval_ns: i64 = 1_000_000;   // 1ms for 1000Hz
    let compute_interval_ns: i64 = 16_666_666; // ~16.67ms for 60Hz
    
    let mut compute_sum: i64 = 0;
    let mut compute_frames = 0;
    
    loop {
        let now = start.elapsed().as_nanos() as i64;
        if now >= duration_ns {
            break;
        }
        
        // Check if sensor task is due (higher priority)
        if now >= next_sensor_ns {
            // Track jitter
            if last_sensor_ns > 0 {
                let delta_ns = now - last_sensor_ns;
                let expected_ns: i64 = 1_000_000;
                let jitter_ns = if delta_ns > expected_ns {
                    delta_ns - expected_ns
                } else {
                    expected_ns - delta_ns
                };
                let jitter_us = jitter_ns / 1000;
                if jitter_us > max_jitter_us {
                    max_jitter_us = jitter_us;
                }
            }
            last_sensor_ns = now;
            sensor_tick_count += 1;
            
            let data = SensorData {
                timestamp: now,
                gyro_x: pseudo_random_f32(now),
                gyro_y: pseudo_random_f32(now + 1),
                gyro_z: pseudo_random_f32(now + 2),
            };
            buffer.push(data);
            next_sensor_ns += sensor_interval_ns;
        }
        
        // Check if compute task is due
        if now >= next_compute_ns {
            // Perform lookups
            let mut sum: i64 = 0;
            for i in 0..LOOKUPS_PER_FRAME {
                let key = (i % HASHMAP_SIZE) as i64;
                if let Some(val) = map.get(&key) {
                    sum += *val;
                }
            }
            
            // Drain sensor buffer
            let drained = buffer.drain();
            compute_sum += sum + drained as i64;
            compute_frames += 1;
            next_compute_ns += compute_interval_ns;
        }
    }
    
    let elapsed_ms = start.elapsed().as_millis();
    
    println!("=== Results ===");
    println!("Sensor ticks: {}", sensor_tick_count);
    println!("Expected ticks: ~{}", BENCHMARK_DURATION_MS);
    println!("Compute frames: {} (expected: ~{})", compute_frames, BENCHMARK_DURATION_MS / 16);
    println!("Max jitter: {}μs", max_jitter_us);
    println!("Total time: {}ms", elapsed_ms);
    
    // Prevent DCE
    if compute_sum == 0 {
        println!("Unexpected zero sum");
    }
    
    // Success criteria: jitter < 1000μs (1ms)
    if max_jitter_us < 1000 {
        println!("✅ PASS: Jitter within 1ms tolerance");
    } else {
        println!("❌ FAIL: Jitter exceeded 1ms");
    }
}
