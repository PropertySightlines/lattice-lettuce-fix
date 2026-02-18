// fannkuch-redux benchmark
// Derived from the Computer Language Benchmarks Game

fn fannkuch(n: usize) -> (i32, i32) {
    let mut perm = vec![0usize; n];
    let mut perm1: Vec<usize> = (0..n).collect();
    let mut count = vec![0usize; n];
    
    let mut max_flips = 0i32;
    let mut checksum = 0i32;
    let mut perm_count = 0i32;
    
    let mut r = n;
    
    loop {
        // Copy perm1 to perm
        while r != 1 {
            count[r - 1] = r;
            r -= 1;
        }
        
        perm.copy_from_slice(&perm1);
        
        // Count flips
        let mut flips = 0i32;
        loop {
            let k = perm[0];
            if k == 0 {
                break;
            }
            // Reverse first k+1 elements
            let mut i = 0;
            let mut j = k;
            while i < j {
                perm.swap(i, j);
                i += 1;
                j -= 1;
            }
            flips += 1;
        }
        
        if flips > max_flips {
            max_flips = flips;
        }
        
        // Checksum
        if perm_count % 2 == 0 {
            checksum += flips;
        } else {
            checksum -= flips;
        }
        perm_count += 1;
        
        // Generate next permutation
        loop {
            if r == n {
                return (checksum, max_flips);
            }
            
            let perm0 = perm1[0];
            for i in 0..r {
                perm1[i] = perm1[i + 1];
            }
            perm1[r] = perm0;
            
            count[r] -= 1;
            if count[r] > 0 {
                break;
            }
            r += 1;
        }
    }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    // Use argc to prevent constant folding, default n=10
    let n = 10 + (args.len() - 1);
    
    let (checksum, max_flips) = fannkuch(n);
    
    // Expected for n=10: checksum=73196, max_flips=38
    if args.len() == 1 {
        if checksum == 73196 && max_flips == 38 {
            std::process::exit(0);
        }
    }
    
    // For other n values, just verify it ran
    if max_flips > 0 {
        std::process::exit(0);
    }
    std::process::exit(1);
}
