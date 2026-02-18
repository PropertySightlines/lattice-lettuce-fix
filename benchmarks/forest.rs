use std::time::Instant;

struct Node {
    left: Option<Box<Node>>,
    right: Option<Box<Node>>,
    val: i32,
}

fn make_tree(depth: i32) -> Option<Box<Node>> {
    if depth == 0 {
        return None;
    }
    Some(Box::new(Node {
        val: depth,
        left: make_tree(depth - 1),
        right: make_tree(depth - 1),
    }))
}

fn main() {
    let t0 = Instant::now();
    
    // Depth 22 -> ~4M nodes (2^22 - 1)
    let root = make_tree(22);
    
    let t1 = Instant::now();
    let build_time = t1.duration_since(t0).as_nanos();
    println!("Build Time: {} ns", build_time);
    
    let t2 = Instant::now();
    
    // Drop triggers recursive free
    drop(root);
    
    let t3 = Instant::now();
    let free_time = t3.duration_since(t2).as_nanos();
    let total = t3.duration_since(t0).as_nanos();
    
    println!("Free Time: {} ns", free_time);
    println!("Total Churn: {} ns", total);
}
