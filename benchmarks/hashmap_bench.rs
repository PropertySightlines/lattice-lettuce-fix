// hashmap_bench.rs - HashMap Benchmark
// Uses std::collections::HashMap for comparison

use std::collections::HashMap;

fn main() {
    let mut checksum: i64 = 0;
    
    // Benchmark: Insert, lookup, remove cycles
    for _ in 0..1000 {
        let mut map: HashMap<i64, i64> = HashMap::with_capacity(16);
        
        // Insert 1000 elements
        for i in 0..1000i64 {
            map.insert(i, i * 7);
        }
        
        // Lookup pattern
        for i in 0..1000i64 {
            checksum += map.get(&i).copied().unwrap_or(0);
        }
        
        // Remove half
        for i in 0..500i64 {
            map.remove(&(i * 2));
        }
        
        // Re-insert
        for i in 0..500i64 {
            map.insert(i * 2, i * 11);
        }
        
        // Final lookups
        for i in 0..1000i64 {
            checksum += map.get(&i).copied().unwrap_or(0);
        }
    }
    
    println!("Checksum: {}", checksum);
}
