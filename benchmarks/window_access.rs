struct Pixel {
    r: i32,
    g: i32,
    b: i32,
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let argc = args.len() as i32;
    
    let n = 10_000_000usize;
    let mut pixels: Vec<Pixel> = Vec::with_capacity(n);
    
    // Initialize with dynamic values
    for i in 0..n {
        pixels.push(Pixel {
            r: i as i32 + argc,
            g: i as i32 * 2,
            b: i as i32 ^ argc,
        });
    }
    
    // Real work: data-dependent traversal
    let mut sum: i64 = 0;
    let mut idx = (argc as usize) & (n - 1);
    let mask = n - 1;
    
    for _ in 0..100_000_000i64 {
        let p = &pixels[idx];
        sum += p.r as i64 + p.g as i64 + p.b as i64;
        idx = (idx + p.r as usize) & mask;
    }
    
    // Prevent DCE
    if sum != 0 {
        std::process::exit(0);
    }
    std::process::exit(1);
}
