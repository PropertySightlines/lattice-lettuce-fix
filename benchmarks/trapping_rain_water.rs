
fn trap(height: &[i32], height_size: usize) -> i32 {
    if height_size <= 2 {
        return 0;
    }

    let mut left = vec![0; height_size];
    let mut right = vec![0; height_size];

    left[0] = height[0];
    for i in 1..height_size {
        let h = height[i];
        let prev = left[i - 1];
        left[i] = if h > prev { h } else { prev };
    }

    right[height_size - 1] = height[height_size - 1];
    for i in (0..height_size - 1).rev() {
        let h = height[i];
        let next = right[i + 1];
        right[i] = if h > next { h } else { next };
    }

    let mut ans = 0;
    for i in 0..height_size {
        let l = left[i];
        let r = right[i];
        let min_h = if l < r { l } else { r };
        ans += min_h - height[i];
    }

    ans
}

fn main() {
    let size = 100000;
    let mut height = vec![0; size];
    
    // Generate zigzag pattern
    for i in 0..size {
        height[i] = (i % 10) as i32;
        if (i / 10) % 2 == 1 {
            height[i] = 9 - (i % 10) as i32;
        }
    }

    let mut total = 0;
    for _ in 0..1000 {
        total += trap(&height, size);
    }
    println!("Total: {}", total);
    // std::hint::black_box(total);
}
