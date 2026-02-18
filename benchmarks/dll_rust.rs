
use std::collections::LinkedList;
use std::time::Instant;

struct RawNode {
    next: *mut RawNode,
    prev: *mut RawNode,
    data: i32,
}

struct RawList {
    head: *mut RawNode,
    tail: *mut RawNode,
}

impl RawList {
    fn new() -> Self {
        Self { head: std::ptr::null_mut(), tail: std::ptr::null_mut() }
    }
    fn push_back(&mut self, data: i32) {
        unsafe {
            let node = Box::into_raw(Box::new(RawNode {
                next: std::ptr::null_mut(),
                prev: self.tail,
                data,
            }));
            if self.tail.is_null() {
                self.head = node;
                self.tail = node;
            } else {
                (*self.tail).next = node;
                self.tail = node;
            }
        }
    }
}

fn main() {
    let iterations = 1_000_000;

    // 1. Rust Safe (LinkedList)
    let mut list = LinkedList::new();
    let start = Instant::now();
    for i in 0..iterations {
        list.push_back(i);
    }
    println!("Rust Safe (LinkedList): {} ms", start.elapsed().as_millis());

    // 2. Rust Unsafe (Raw Pointers)
    let mut raw_list = RawList::new();
    let start = Instant::now();
    for i in 0..iterations {
        raw_list.push_back(i);
    }
    println!("Rust Unsafe (Raw): {} ms", start.elapsed().as_millis());
}
