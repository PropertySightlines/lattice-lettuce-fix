
use std::ptr;

struct Node {
    key: i32,
    val: i32,
    prev: *mut Node,
    next: *mut Node,
}

struct LRUCache {
    capacity: i32,
    count: i32,
    head: *mut Node,
    tail: *mut Node,
    map: Vec<*mut Node>,
}

impl LRUCache {
    fn new(capacity: i32) -> Self {
        unsafe {
            let head = Box::into_raw(Box::new(Node { key: 0, val: 0, prev: ptr::null_mut(), next: ptr::null_mut() }));
            let tail = Box::into_raw(Box::new(Node { key: 0, val: 0, prev: ptr::null_mut(), next: ptr::null_mut() }));
            (*head).next = tail;
            (*tail).prev = head;
            
            LRUCache {
                capacity,
                count: 0,
                head,
                tail,
                map: vec![ptr::null_mut(); 10000],
            }
        }
    }
    
    fn remove_node(&mut self, node: *mut Node) {
        unsafe {
            let prev = (*node).prev;
            let next = (*node).next;
            (*prev).next = next;
            (*next).prev = prev;
        }
    }
    
    fn add_to_head(&mut self, node: *mut Node) {
        unsafe {
            (*node).prev = self.head;
            (*node).next = (*self.head).next;
            (*(*self.head).next).prev = node;
            (*self.head).next = node;
        }
    }
    
    fn get(&mut self, key: i32) -> i32 {
        let node = self.map[key as usize];
        if !node.is_null() {
            self.remove_node(node);
            self.add_to_head(node);
            unsafe { return (*node).val; }
        }
        -1
    }
    
    fn put(&mut self, key: i32, val: i32) {
        let node = self.map[key as usize];
        unsafe {
            if !node.is_null() {
                (*node).val = val;
                self.remove_node(node);
                self.add_to_head(node);
            } else {
                let new_node = Box::into_raw(Box::new(Node { key, val, prev: ptr::null_mut(), next: ptr::null_mut() }));
                self.map[key as usize] = new_node;
                self.add_to_head(new_node);
                self.count += 1;
                if self.count > self.capacity {
                    let lru = (*self.tail).prev;
                    self.remove_node(lru);
                    self.map[(*lru).key as usize] = ptr::null_mut();
                    self.count -= 1;
                    // Box::from_raw(lru); // Leak to match others
                }
            }
        }
    }
}

fn main() {
    let mut checksum = 0;
    for _ in 0..1000 {
        let mut cache = LRUCache::new(100);
        for i in 0..1000 {
            cache.put(i, i * 10);
            if i % 2 == 0 {
                checksum += cache.get(i / 2);
            }
        }
    }
    println!("Checksum: {}", checksum);
    // std::hint::black_box(checksum);
}
