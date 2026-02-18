static mut COUNTER: i32 = 0;
static mut SCRATCH: [i32; 256] = [0; 256];

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let argc = args.len() as i32;

    unsafe {
        // Initialize with dynamic values
        for i in 0..256 {
            SCRATCH[i] = i as i32 + argc;
        }
        
        COUNTER = argc;
        
        // Loop with data-dependent memory access - can't be strength-reduced
        for _ in 0..50_000_000i64 {
            let idx = (COUNTER & 0xFF) as usize;
            COUNTER = COUNTER + SCRATCH[idx];
            SCRATCH[idx] = COUNTER;
        }
        
        // Prevent DCE
        if COUNTER != 0 {
            std::process::exit(0);
        }
        std::process::exit(1);
    }
}
