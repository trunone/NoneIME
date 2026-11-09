// Boshiamy key mappings and IME keystroke handler

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum KeyAction {
    None,
    EatOnly,
    AddChar(char),
    Backspace,
    Space,
    CommitCandidate(usize), // Selection index 0..9
    PrevPage,
    NextPage,
    Escape,
    ToggleImeMode, // Shift or hotkey toggle
}

/// Convert virtual key and character to Boshiamy keystroke action
pub fn classify_key(
    vk: u32,
    ch: char,
    ctrl_down: bool,
    alt_down: bool,
    is_composing: bool,
    has_candidates: bool,
) -> KeyAction {
    // If Ctrl or Alt is held, do not process IME keystrokes
    if ctrl_down || alt_down {
        return KeyAction::None;
    }

    match vk {
        0x1B => { // VK_ESCAPE
            if is_composing || has_candidates {
                KeyAction::Escape
            } else {
                KeyAction::None
            }
        }
        0x08 => { // VK_BACK
            if is_composing {
                KeyAction::Backspace
            } else {
                KeyAction::None
            }
        }
        0x20 => { // VK_SPACE
            if is_composing || has_candidates {
                KeyAction::Space
            } else {
                KeyAction::None
            }
        }
        0x21 => { // VK_PRIOR (Page Up)
            if has_candidates {
                KeyAction::PrevPage
            } else {
                KeyAction::None
            }
        }
        0x22 => { // VK_NEXT (Page Down)
            if has_candidates {
                KeyAction::NextPage
            } else {
                KeyAction::None
            }
        }
        // Number selection: '1'..='9', '0'
        0x30..=0x39 => {
            if has_candidates {
                // 1 -> index 0, 2 -> 1, ..., 9 -> 8, 0 -> 9
                let idx = match vk {
                    0x30 => 9,
                    other => (other - 0x31) as usize,
                };
                KeyAction::CommitCandidate(idx)
            } else if is_composing {
                KeyAction::None
            } else {
                KeyAction::None
            }
        }
        // Boshiamy root keys: A-Z, comma, period, bracket, semicolon, etc.
        0x41..=0x5A => { // 'A'..='Z'
            let lower = if ch.is_ascii_alphabetic() {
                ch.to_ascii_lowercase()
            } else {
                (b'a' + (vk - 0x41) as u8) as char
            };
            KeyAction::AddChar(lower)
        }
        0xBC => { // VK_OEM_COMMA (',')
            KeyAction::AddChar(',')
        }
        0xBE => { // VK_OEM_PERIOD ('.')
            KeyAction::AddChar('.')
        }
        0xBA => { // VK_OEM_1 (';')
            KeyAction::AddChar(';')
        }
        0xDE => { // VK_OEM_7 ('\'')
            KeyAction::AddChar('\'')
        }
        0xDB => { // VK_OEM_4 ('[')
            KeyAction::AddChar('[')
        }
        0xDD => { // VK_OEM_6 (']')
            KeyAction::AddChar(']')
        }
        _ => KeyAction::None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_classify_key() {
        assert_eq!(classify_key(0x41, 'A', false, false, false, false), KeyAction::AddChar('a'));
        assert_eq!(classify_key(0x41, '\0', false, false, false, false), KeyAction::AddChar('a'));
        assert_eq!(classify_key(0xBC, '\0', false, false, false, false), KeyAction::AddChar(','));
        assert_eq!(classify_key(0x20, ' ', false, false, true, true), KeyAction::Space);
        assert_eq!(classify_key(0x08, '\x08', false, false, true, false), KeyAction::Backspace);
        assert_eq!(classify_key(0x31, '1', false, false, true, true), KeyAction::CommitCandidate(0));
        assert_eq!(classify_key(0x30, '0', false, false, true, true), KeyAction::CommitCandidate(9));
        assert_eq!(classify_key(0x41, 'A', true, false, true, false), KeyAction::None);
    }
}
