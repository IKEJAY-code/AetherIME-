pub fn confidence_from_freq(freq: u32) -> f32 {
    // 简单可单调的映射：freq 越高越接近 1
    1.0 - 1.0 / (freq as f32 + 1.0)
}

pub fn gate_suggestion(prefix_len: usize, suggestion_len: usize, confidence: f32) -> bool {
    if prefix_len < 2 {
        return false;
    }
    if suggestion_len == 0 {
        return false;
    }
    if confidence < 0.50 {
        return false;
    }
    true
}

