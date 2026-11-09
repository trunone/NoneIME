// Cross-platform IME runner using imekit

use imekit::{InputMethod, InputMethodEvent, KeyState};
use crate::composition::{CompositionEngine, CompositionResult};

pub struct ImekitRunner {
    pub im: InputMethod,
    pub engine: CompositionEngine,
}

impl ImekitRunner {
    pub fn new() -> Result<Self, imekit::Error> {
        #[allow(unused_mut)]
        let mut im = InputMethod::new()?;

        #[cfg(target_os = "linux")]
        {
            if let InputMethod::Wayland(ref mut wim) = im {
                if let Err(e) = wim.grab_keyboard() {
                    eprintln!("[NoneIME] Wayland keyboard grab failed: {}", e);
                } else {
                    println!("[NoneIME] Wayland keyboard grab activated successfully.");
                }
            }
        }

        Ok(Self {
            im,
            engine: CompositionEngine::new(),
        })
    }

    /// Update preedit text in the IME based on the composition engine state
    pub fn update_preedit(&self, serial: u32) -> Result<(), imekit::Error> {
        if self.engine.is_composing() {
            let mut preedit_display = self.engine.composition_buffer.clone();

            if self.engine.has_candidates() {
                preedit_display.push(' ');
                let cands = self.engine.current_page_candidates();
                for (i, cand) in cands.iter().enumerate() {
                    let num = (i + 1) % 10;
                    preedit_display.push_str(&format!("{}.{} ", num, cand.value));
                }
                let total_pages = self.engine.page_count();
                if total_pages > 1 {
                    preedit_display.push_str(&format!("[{}/{}]", self.engine.current_page + 1, total_pages));
                }
            }

            let len = preedit_display.chars().count() as i32;
            self.im.set_preedit_string(&preedit_display, len, len)?;
            self.im.commit(serial)?;
        } else {
            self.im.set_preedit_string("", 0, 0)?;
            self.im.commit(serial)?;
        }
        Ok(())
    }

    /// Commit candidate text to the client
    pub fn commit_text(&mut self, text: &str, serial: u32) -> Result<(), imekit::Error> {
        self.im.commit_string(text)?;
        self.im.commit(serial)?;
        self.engine.clear();
        self.im.set_preedit_string("", 0, 0)?;
        self.im.commit(serial)?;
        Ok(())
    }

    /// Map XKB keysym to Windows-style VK code and char
    pub fn map_keysym_to_vk(keysym: u32) -> (u32, char) {
        match keysym {
            // Escape
            0xff1b => (0x1B, '\x1B'),
            // BackSpace
            0xff08 => (0x08, '\x08'),
            // Space
            0x0020 => (0x20, ' '),
            // Prior / Page_Up
            0xff55 => (0x21, '\0'),
            // Next / Page_Down
            0xff56 => (0x22, '\0'),
            // Numbers 0..9
            0x0030..=0x0039 => {
                let code = keysym;
                (code, (keysym as u8) as char)
            }
            // Lowercase a..z
            0x0061..=0x007a => {
                let vk = 0x41 + (keysym - 0x0061);
                (vk, (keysym as u8) as char)
            }
            // Uppercase A..Z
            0x0041..=0x005a => {
                let vk = keysym;
                (vk, (keysym as u8) as char)
            }
            // Comma
            0x002c => (0xBC, ','),
            // Period
            0x002e => (0xBE, '.'),
            // Semicolon
            0x003b => (0xBA, ';'),
            // Single quote / apostrophe
            0x0027 => (0xDE, '\''),
            // Left bracket
            0x005b => (0xDB, '['),
            // Right bracket
            0x005d => (0xDD, ']'),
            _ => (0, '\0'),
        }
    }

    /// Run the main IME event loop
    pub fn run(&mut self) -> Result<(), imekit::Error> {
        println!("[NoneIME] None IME initialized and waiting for events...");

        let mut current_serial: u32 = 0;

        loop {
            while let Some(event) = self.im.next_event() {
                match event {
                    InputMethodEvent::Activate { serial } => {
                        current_serial = serial;
                        println!("[NoneIME] IME Activated (serial: {})", serial);
                    }
                    InputMethodEvent::Deactivate => {
                        println!("[NoneIME] IME Deactivated");
                        self.engine.clear();
                    }
                    InputMethodEvent::KeyEvent {
                        keycode,
                        keysym,
                        state,
                        modifiers,
                    } => {
                        #[allow(unused_variables)]
                        let _ = keycode;
                        if state == KeyState::Pressed {
                            let (vk, ch) = Self::map_keysym_to_vk(keysym);
                            let ctrl_down = modifiers.ctrl;
                            let alt_down = modifiers.alt;

                            let result = self.engine.on_key_down(vk, ch, ctrl_down, alt_down);
                            match result {
                                CompositionResult::DoNothing => {
                                    #[cfg(target_os = "linux")]
                                    {
                                        if let InputMethod::Wayland(ref wim) = self.im {
                                            let _ = wim.forward_key(keycode, state);
                                        }
                                    }
                                }
                                CompositionResult::Eaten => {
                                    // Keystroke consumed by IME
                                }
                                CompositionResult::UpdateUI => {
                                    let _ = self.update_preedit(current_serial);
                                }
                                CompositionResult::CommitText(text) => {
                                    let _ = self.commit_text(&text, current_serial);
                                }
                            }
                        } else {
                            #[cfg(target_os = "linux")]
                            {
                                if !self.engine.is_composing() {
                                    if let InputMethod::Wayland(ref wim) = self.im {
                                        let _ = wim.forward_key(keycode, state);
                                    }
                                }
                            }
                        }
                    }
                    InputMethodEvent::Unavailable => {
                        eprintln!("[NoneIME] Input method protocol unavailable!");
                        return Ok(());
                    }
                    _ => {}
                }
            }

            std::thread::sleep(std::time::Duration::from_millis(10));
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_map_keysym_to_vk() {
        assert_eq!(ImekitRunner::map_keysym_to_vk(0x0061), (0x41, 'a'));
        assert_eq!(ImekitRunner::map_keysym_to_vk(0x0020), (0x20, ' '));
        assert_eq!(ImekitRunner::map_keysym_to_vk(0xff08), (0x08, '\x08'));
        assert_eq!(ImekitRunner::map_keysym_to_vk(0x0031), (0x31, '1'));
        assert_eq!(ImekitRunner::map_keysym_to_vk(0x002c), (0xBC, ','));
    }
}
