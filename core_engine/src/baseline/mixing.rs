pub fn is_ascii_word_char(ch: char) -> bool {
    ch.is_ascii_alphanumeric() || ch == '_'
}

pub fn ends_with_ascii_word(context_before_cursor: &str) -> bool {
    context_before_cursor
        .chars()
        .rev()
        .next()
        .is_some_and(is_ascii_word_char)
}

pub fn ends_with_cjk(context_before_cursor: &str) -> bool {
    // 粗略：把 CJK Unified Ideographs 当作中文
    // U+4E00..U+9FFF
    context_before_cursor
        .chars()
        .rev()
        .next()
        .is_some_and(|ch| ('\u{4E00}'..='\u{9FFF}').contains(&ch))
}

pub fn maybe_prepend_space_for_mixed(
    context_before_cursor: &str,
    mut suggestion: String,
) -> String {
    // 规则：中文后面接英文 token，建议前加空格；避免 “中文word” 粘连。
    if ends_with_cjk(context_before_cursor) {
        if suggestion
            .chars()
            .next()
            .is_some_and(|ch| ch.is_ascii_alphanumeric())
        {
            if !suggestion.starts_with(' ') {
                suggestion.insert(0, ' ');
            }
        }
    }
    suggestion
}

