// Writer Protocol Performance Benchmark - Rust
// Measures direct-to-buffer formatting using byte-by-byte loops
// Matches Salt's current loop-based write_str implementation for fair comparison

const ITERATIONS: usize = 10_000_000;
const BUFFER_SIZE: usize = 4096;

struct Buffer {
    data: Vec<u8>,
    len: usize,
}

impl Buffer {
    fn new(capacity: usize) -> Self {
        Buffer {
            data: vec![0u8; capacity],
            len: 0,
        }
    }
    
    fn clear(&mut self) {
        self.len = 0;
    }
    
    // Byte-by-byte push (matches Salt's push_byte)
    fn push_byte(&mut self, b: u8) {
        if self.len < self.data.len() {
            self.data[self.len] = b;
            self.len += 1;
        }
    }
    
    // Loop-based write_str (matches Salt's approach)
    fn write_str(&mut self, s: &[u8]) {
        for &b in s {
            self.push_byte(b);
        }
    }
    
    // Integer formatting with byte-by-byte push
    fn write_i32(&mut self, val: i32) {
        if val == 0 {
            self.push_byte(b'0');
            return;
        }
        
        let mut tmp = [0u8; 12];
        let mut count = 0;
        let mut n = val;
        let neg = n < 0;
        if neg { n = -n; }
        
        while n > 0 {
            tmp[count] = b'0' + (n % 10) as u8;
            count += 1;
            n /= 10;
        }
        
        if neg { self.push_byte(b'-'); }
        
        for i in (0..count).rev() {
            self.push_byte(tmp[i]);
        }
    }
    
    fn write_i64(&mut self, val: i64) {
        if val == 0 {
            self.push_byte(b'0');
            return;
        }
        
        let mut tmp = [0u8; 20];
        let mut count = 0;
        let mut n = val;
        let neg = n < 0;
        if neg { n = -n; }
        
        while n > 0 {
            tmp[count] = b'0' + (n % 10) as u8;
            count += 1;
            n /= 10;
        }
        
        if neg { self.push_byte(b'-'); }
        
        for i in (0..count).rev() {
            self.push_byte(tmp[i]);
        }
    }
    
    fn len(&self) -> usize {
        self.len
    }
}

fn main() {
    let mut buf = Buffer::new(BUFFER_SIZE);
    let mut total_len: i64 = 0;
    let mut checksum: u8 = 0;
    
    for i in 0..ITERATIONS {
        buf.clear();
        
        // Direct write calls using byte-by-byte approach
        buf.write_str(b"Item ");
        buf.write_i32(i as i32);
        buf.write_str(b": val = ");
        buf.write_i64((i as i64) * 1000);
        
        total_len += buf.len() as i64;
        
        // [FAIR COMPARISON] XOR first AND last byte to prevent Dead Store Elimination
        // This creates a data dependency forcing ALL buffer writes including integer formatting
        checksum ^= buf.data[0];
        checksum ^= buf.data[buf.len - 1];
    }
    
    // Prevent DCE - use both length and checksum
    if total_len == 0 || checksum == 255 {
        std::process::exit(1);
    }
}

