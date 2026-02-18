// longest_consecutive.rs - LeetCode 128: Longest Consecutive Sequence
// O(n) solution using std::collections::HashSet

use std::collections::HashSet;

fn longest_consecutive(nums: &[i64]) -> i64 {
    if nums.is_empty() { return 0; }
    
    let set: HashSet<i64> = nums.iter().copied().collect();
    let mut best = 0;
    
    for &num in &set {
        // Only start counting if this is the start of a sequence
        if !set.contains(&(num - 1)) {
            let mut len = 1;
            while set.contains(&(num + len)) { len += 1; }
            best = best.max(len);
        }
    }
    
    best
}

fn main() {
    // Benchmark: Multiple rounds with large random arrays
    let mut total: i64 = 0;
    let n = 100000;
    
    for round in 0..100 {
        // Generate pseudo-random array with some consecutive sequences
        let nums: Vec<i64> = (0..n)
            .map(|i| ((i as i64) * 1103515245 + 12345 + round) % (n * 10))
            .collect();
        total += longest_consecutive(&nums);
    }
    
    println!("Total: {}", total);
}
