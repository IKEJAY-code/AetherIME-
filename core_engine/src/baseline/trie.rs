use std::collections::HashMap;

#[derive(Debug, Default, Clone)]
struct Node {
    children: HashMap<char, Node>,
    terminal_freq: u32,
}

#[derive(Debug, Default, Clone)]
pub struct Trie {
    root: Node,
}

impl Trie {
    pub fn insert(&mut self, word: &str, freq: u32) {
        let mut node = &mut self.root;
        for ch in word.chars() {
            node = node.children.entry(ch).or_default();
        }
        node.terminal_freq = node.terminal_freq.saturating_add(freq.max(1));
    }

    pub fn best_completion(&self, prefix: &str) -> Option<(String, u32)> {
        let mut node = &self.root;
        for ch in prefix.chars() {
            node = node.children.get(&ch)?;
        }

        let mut best: Option<(String, u32)> = None;
        let mut buf = prefix.to_string();
        self.dfs_best(node, &mut buf, &mut best);
        best
    }

    fn dfs_best(&self, node: &Node, buf: &mut String, best: &mut Option<(String, u32)>) {
        if node.terminal_freq > 0 {
            let better = match best {
                None => true,
                Some((_, freq)) => node.terminal_freq > *freq,
            };
            if better {
                *best = Some((buf.clone(), node.terminal_freq));
            }
        }
        for (ch, child) in node.children.iter() {
            buf.push(*ch);
            self.dfs_best(child, buf, best);
            buf.pop();
        }
    }
}

