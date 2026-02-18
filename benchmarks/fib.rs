fn fib(n: i32) -> i32 {
    if n <= 1 { 
        return n; 
    }
    fib(n - 1) + fib(n - 2)
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let n = 39 + args.len() as i32;
    let res = fib(n);
    if res != 102334155 {
        std::process::exit(1);
    }
}
