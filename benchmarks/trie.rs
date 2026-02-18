
#[derive(Default)]
struct TrieNode {
    children: [Option<Box<TrieNode>>; 26],
    is_word: bool,
}

impl TrieNode {
    fn new() -> Self {
        TrieNode {
            children: Default::default(),
            is_word: false,
        }
    }
}

fn insert(root: &mut TrieNode, word: &[u8]) {
    let mut curr = root;
    for &c in word {
        let idx = (c - b'a') as usize;
        if curr.children[idx].is_none() {
            curr.children[idx] = Some(Box::new(TrieNode::new()));
        }
        curr = curr.children[idx].as_mut().unwrap();
    }
    curr.is_word = true;
}

fn search(root: &TrieNode, word: &[u8]) -> bool {
    let mut curr = root;
    for &c in word {
        let idx = (c - b'a') as usize;
        if let Some(ref node) = curr.children[idx] {
            curr = node;
        } else {
            return false;
        }
    }
    curr.is_word
}

fn main() {
    let mut root = TrieNode::new();
    let mut word = [0u8; 5];

    println!("Inserting 700k words...");
    for i in 0..700000u32 {
        word[0] = ((i % 26) + 97) as u8;
        word[1] = (((i / 26) % 26) + 97) as u8;
        word[2] = (((i / 676) % 26) + 97) as u8;
        word[3] = (((i / 17576) % 26) + 97) as u8;
        word[4] = ((i % 7) + 97) as u8;
        insert(&mut root, &word);
    }

    println!("Searching 700k words...");
    let mut found = 0i64;
    for i in 0..700000u32 {
        word[0] = ((i % 26) + 97) as u8;
        word[1] = (((i / 26) % 26) + 97) as u8;
        word[2] = (((i / 676) % 26) + 97) as u8;
        word[3] = (((i / 17576) % 26) + 97) as u8;
        word[4] = ((i % 7) + 97) as u8;
        if search(&root, &word) {
            found += 1;
        }
    }
    println!("Found: {}", found);
}
