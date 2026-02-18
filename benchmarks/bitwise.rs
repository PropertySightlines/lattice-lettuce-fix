use std::env;

fn main() {
    let argc = env::args().count() as u64;
    let iterations = 100_000_000u64;
    let mut total: u64 = 0;
    let base: u64 = argc | 0x123456789ABCDEF0;
    
    for i in 0..iterations {
        // Use built-in popcount for fair comparison
        total += (base ^ i).count_ones() as u64;
    }
    
    if total > 0 {
        std::process::exit(0);
    } else {
        std::process::exit(1);
    }
}
