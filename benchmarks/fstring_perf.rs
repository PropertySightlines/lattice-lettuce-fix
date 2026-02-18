// F-String Performance Benchmark - Rust
// Measures string formatting performance with format! macro
// Comparable to Salt f-strings and C sprintf

const ITERATIONS: usize = 10_000_000;

fn main() {
    let mut total_len: usize = 0;
    let base_val: f64 = 3.14159;
    
    for i in 0..ITERATIONS {
        let formatted = format!("Item {}: value = {:.2}", i, base_val + (i as f64) * 0.001);
        total_len += formatted.len();
    }
    
    // Prevent DCE
    if total_len == 0 {
        std::process::exit(1);
    }
}
