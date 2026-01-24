mod mixing;
mod scoring;
mod trie;

use mixing::maybe_prepend_space_for_mixed;
use scoring::{confidence_from_freq, gate_suggestion};
use trie::Trie;

#[derive(Debug, Clone)]
pub struct Suggestion {
    pub suggestion: String,
    pub confidence: f32,
    pub replace_range: [usize; 2],
}

#[derive(Debug, Default, Clone)]
pub struct BaselineSuggestor {
    words: Trie,
}

impl BaselineSuggestor {
    pub fn new() -> Self {
        let mut words = Trie::default();

        // Phase 1: 内置极小词表（可替换为本地词频/最近使用）。
        // 目标是：结构可跑通、延迟极低、可快速扩展。
        let seed: &[(&str, u32)] = &[
            ("world", 50),
            ("would", 30),
            ("work", 30),
            ("thanks", 40),
            ("thank", 25),
            ("there", 35),
            ("their", 20),
            ("because", 25),
            ("please", 30),
            ("tomorrow", 20),
            ("regards", 25),
        ];
        for (w, f) in seed {
            words.insert(w, *f);
        }

        Self { words }
    }

    pub fn suggest_utf16(&self, context: &str, cursor_utf16: usize, max_len: usize) -> Option<Suggestion> {
        // 注意：cursor 以 UTF-16 code units 为单位（与 TSF 对齐）。
        let ctx16: Vec<u16> = context.encode_utf16().collect();
        let cursor = cursor_utf16.min(ctx16.len());
        let before = String::from_utf16_lossy(&ctx16[..cursor]);

        // 仅对英文 last-token 进行补全（Phase 1）。
        let (token_prefix, token_prefix_len) = last_ascii_token_prefix(&before);
        if token_prefix.is_empty() {
            return None;
        }

        let (best_word, freq) = self.words.best_completion(&token_prefix.to_ascii_lowercase())?;
        if best_word.len() < token_prefix.len() {
            return None;
        }
        let mut suffix = best_word[token_prefix.len()..].to_string();
        if suffix.len() > max_len {
            suffix.truncate(max_len);
        }

        let mut confidence = confidence_from_freq(freq);
        // 更长前缀加一点点 confidence（让短前缀更谨慎）
        confidence = (confidence + (token_prefix_len as f32 * 0.02)).min(0.99);

        if !gate_suggestion(token_prefix_len, suffix.len(), confidence) {
            return None;
        }

        suffix = maybe_prepend_space_for_mixed(&before, suffix);

        Some(Suggestion {
            // replace_range 以 context UTF-16 为坐标；Phase 1 只做插入。
            suggestion: suffix,
            confidence,
            replace_range: [cursor_utf16, cursor_utf16],
        })
    }
}

fn last_ascii_token_prefix(before: &str) -> (String, usize) {
    // 返回最后一个 ASCII token（含数字/下划线）的前缀，用于前缀补全。
    // e.g. "hello wor" -> ("wor", 3)
    let mut start = before.len();
    for (i, ch) in before.char_indices().rev() {
        if ch.is_ascii_alphanumeric() || ch == '_' {
            start = i;
        } else {
            break;
        }
    }
    let token = before[start..].to_string();
    let token_len = token.len();
    (token, token_len)
}

