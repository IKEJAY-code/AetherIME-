pub fn env_u16(key: &str, default_val: u16) -> u16 {
    std::env::var(key)
        .ok()
        .and_then(|s| s.parse::<u16>().ok())
        .unwrap_or(default_val)
}

