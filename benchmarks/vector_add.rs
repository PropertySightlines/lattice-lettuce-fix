#[derive(Copy, Clone)]
struct SaltVec<T> {
    data: T,
}

fn add_generic<T: std::ops::Add<Output = T>>(a: SaltVec<T>, b: SaltVec<T>) -> SaltVec<T> {
    SaltVec { data: a.data + b.data }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let argc = args.len() as i32;
    
    let mut sum: i32 = 0;
    
    // Loop-carried dependency: each iteration depends on previous sum
    for i in 0..100_000_000i32 {
        let val1 = (i ^ (i >> 3)) + argc + (sum & 0xFF);
        let val2 = ((i * 17) ^ (i >> 5)) + argc;
        
        let v1 = SaltVec { data: val1 };
        let v2 = SaltVec { data: val2 };
        let res = add_generic(v1, v2);
        sum = sum.wrapping_add(res.data);
    }
    
    // Prevent DCE
    if sum != 0 {
        std::process::exit(0);
    }
    std::process::exit(1);
}
