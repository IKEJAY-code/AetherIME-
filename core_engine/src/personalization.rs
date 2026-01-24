// Phase 2+：这里可以接入 LRU + SQLite（或其它轻量 KV）做个性化频次。
// Phase 1 保持空实现，避免把持久化引入最小可运行路径。

#[derive(Debug, Default, Clone)]
pub struct PersonalizationStore;

impl PersonalizationStore {
    pub fn bump(&self, _text: &str) {
        // TODO(phase2): 记录用户接受的补全文本（仅统计/匿名化）
    }
}

