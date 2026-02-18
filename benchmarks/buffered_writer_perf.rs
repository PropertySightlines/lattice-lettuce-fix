// Buffered Writer Performance Benchmark - Rust
// Tests buffered file I/O throughput with formatted output

use std::fs::File;
use std::io::{Write, BufWriter};

const ITERATIONS: i64 = 1000000;

fn main() {
    // Open output file
    let file = File::create("/tmp/rust_writer_test.txt").expect("Failed to open file");
    let mut writer = BufWriter::new(file);
    
    // Write formatted output
    for i in 0..ITERATIONS {
        write!(writer, "Item {}: value = {}\n", i as i32, i * 1000).unwrap();
    }
    
    // Flush remaining buffer (automatic on drop, but explicit for fairness)
    writer.flush().unwrap();
}
