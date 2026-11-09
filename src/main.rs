// None IME standalone cross-platform IME executable using imekit

use NoneIME::imekit_engine::ImekitRunner;

fn main() {
    println!("None IME - Boshiamy Input Method (imekit runner)");
    println!("Starting input method service...");

    match ImekitRunner::new() {
        Ok(mut runner) => {
            if let Err(e) = runner.run() {
                eprintln!("[NoneIME] Error running IME engine: {}", e);
            }
        }
        Err(e) => {
            eprintln!("[NoneIME] Failed to initialize IME via imekit: {}", e);
        }
    }
}
