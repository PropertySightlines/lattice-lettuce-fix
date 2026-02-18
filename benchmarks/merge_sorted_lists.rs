
struct ListNode {
    val: i32,
    next: Option<Box<ListNode>>,
}

impl ListNode {
    fn new(val: i32) -> Self {
        ListNode { val, next: None }
    }
}

fn create_list(len: i32, start: i32, step: i32) -> Option<Box<ListNode>> {
    if len <= 0 {
        return None;
    }
    let mut node = ListNode::new(start);
    node.next = create_list(len - 1, start + step, step);
    Some(Box::new(node))
}

fn merge_two_lists(l1: Option<Box<ListNode>>, l2: Option<Box<ListNode>>) -> Option<Box<ListNode>> {
    match (l1, l2) {
        (None, None) => None,
        (Some(n), None) => Some(n),
        (None, Some(n)) => Some(n),
        (Some(mut n1), Some(mut n2)) => {
            if n1.val < n2.val {
                n1.next = merge_two_lists(n1.next, Some(n2));
                Some(n1)
            } else {
                n2.next = merge_two_lists(Some(n1), n2.next);
                Some(n2)
            }
        }
    }
}

fn main() {
    let mut checksum = 0;
    for _ in 0..5000 {
        let l1 = create_list(100, 0, 2);
        let l2 = create_list(100, 1, 2);
        let merged = merge_two_lists(l1, l2);
        
        let mut curr = &merged;
        while let Some(node) = curr {
            checksum += node.val;
            curr = &node.next;
        }
    }
    println!("Checksum: {}", checksum);
    // std::hint::black_box(checksum);
}
