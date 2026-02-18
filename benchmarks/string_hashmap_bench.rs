// string_hashmap_bench.rs - String Key HashMap Benchmark
// Uses std::collections::HashMap with String keys for comparison

use std::collections::HashMap;

fn main() {
    let mut checksum: i64 = 0;

    for _ in 0..100 {
        let mut map: HashMap<String, i64> = HashMap::with_capacity(16);

        // Insert 1000 string keys
        for i in 0..1000i64 {
            let key = format!("key_{}", i);
            map.insert(key, i * 7);
        }

        // Lookup all keys
        for i in 0..1000i64 {
            let key = format!("key_{}", i);
            checksum += map.get(&key).copied().unwrap_or(0);
        }

        // Remove half
        for i in 0..500i64 {
            let key = format!("key_{}", i * 2);
            map.remove(&key);
        }

        // Re-insert
        for i in 0..500i64 {
            let key = format!("key_{}", i * 2);
            map.insert(key, i * 11);
        }

        // Final lookups
        for i in 0..1000i64 {
            let key = format!("key_{}", i);
            checksum += map.get(&key).copied().unwrap_or(0);
        }
    }

    println!("Checksum: {}", checksum);
}
