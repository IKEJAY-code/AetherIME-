mod baseline;
mod personalization;
mod protocol;
mod util;

use baseline::BaselineSuggestor;
use protocol::{ClientMessage, EngineMessage};
use std::collections::HashSet;
use std::io::{BufRead, BufReader, BufWriter, Write};
use std::net::{TcpListener, TcpStream};
use std::sync::Arc;
use std::thread;

fn main() -> std::io::Result<()> {
    let port = util::env_u16("SHURUFA_ENGINE_PORT", 48080);
    let addr = format!("127.0.0.1:{port}");

    let listener = TcpListener::bind(&addr)?;
    eprintln!("core_engine listening on {addr} (JSONL over TCP)");

    let suggestor = Arc::new(BaselineSuggestor::new());

    for incoming in listener.incoming() {
        match incoming {
            Ok(stream) => {
                let s = suggestor.clone();
                thread::spawn(move || {
                    if let Err(e) = handle_client(stream, s) {
                        eprintln!("client ended: {e}");
                    }
                });
            }
            Err(e) => eprintln!("accept error: {e}"),
        }
    }
    Ok(())
}

fn handle_client(stream: TcpStream, suggestor: Arc<BaselineSuggestor>) -> std::io::Result<()> {
    let peer = stream.peer_addr().ok();
    eprintln!("client connected: {peer:?}");

    let mut canceled: HashSet<String> = HashSet::new();

    let reader = BufReader::new(stream.try_clone()?);
    let mut writer = BufWriter::new(stream);

    for line_res in reader.lines() {
        let line = match line_res {
            Ok(line) => line,
            Err(e) => {
                eprintln!("read error: {e}");
                break;
            }
        };
        if line.trim().is_empty() {
            continue;
        }

        let msg = match serde_json::from_str::<ClientMessage>(&line) {
            Ok(m) => m,
            Err(e) => {
                eprintln!("bad json: {e}; line={line}");
                continue;
            }
        };

        match msg {
            ClientMessage::Cancel { request_id } => {
                canceled.insert(request_id);
            }
            ClientMessage::Suggest {
                request_id,
                context,
                cursor,
                language_hint: _,
                max_len,
                client: _,
            } => {
                if canceled.remove(&request_id) {
                    continue;
                }

                let sug = suggestor.suggest_utf16(&context, cursor, max_len);
                let (suggestion, confidence, replace_range) = match sug {
                    Some(s) => (s.suggestion, s.confidence, s.replace_range),
                    None => (String::new(), 0.0, [cursor, cursor]),
                };

                let out = EngineMessage::Suggestion {
                    request_id,
                    suggestion,
                    confidence,
                    replace_range,
                };
                let mut json = serde_json::to_string(&out).unwrap_or_else(|_| "{}".to_string());
                json.push('\n');
                writer.write_all(json.as_bytes())?;
                writer.flush()?;
            }
        }
    }

    eprintln!("client disconnected: {peer:?}");
    Ok(())
}

