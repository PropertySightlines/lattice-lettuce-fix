
struct TreeNode {
    val: i32,
    left: Option<Box<TreeNode>>,
    right: Option<Box<TreeNode>>,
}

impl TreeNode {
    fn new(val: i32) -> Self {
        TreeNode { val, left: None, right: None }
    }
}

fn build_tree(depth: i32) -> Option<Box<TreeNode>> {
    if depth == 0 {
        return None;
    }
    let mut node = TreeNode::new(depth);
    node.left = build_tree(depth - 1);
    node.right = build_tree(depth - 1);
    Some(Box::new(node))
}

fn dfs(root: &Option<Box<TreeNode>>, sum: i32) -> i32 {
    let node = match root {
        Some(n) => n,
        None => return 0,
    };
    
    let current = sum * 10 + node.val;
    
    if node.left.is_none() && node.right.is_none() {
        return current;
    }
    
    dfs(&node.left, current) + dfs(&node.right, current)
}

fn main() {
    let root = build_tree(12);
    let mut total = 0;
    for _ in 0..600 {
        total += dfs(&root, 0);
    }
    println!("Total: {}", total);
    // std::hint::black_box(total);
}
