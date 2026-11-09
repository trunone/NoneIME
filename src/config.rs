// Configuration loader for NoneIME.ini

use std::fs;
use std::path::{Path, PathBuf};
use std::sync::RwLock;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ColorRef(pub u32);

impl ColorRef {
    pub const fn from_rgb(r: u8, g: u8, b: u8) -> Self {
        ColorRef((r as u32) | ((g as u32) << 8) | ((b as u32) << 16))
    }

    pub fn r(self) -> u8 {
        (self.0 & 0xFF) as u8
    }

    pub fn g(self) -> u8 {
        ((self.0 >> 8) & 0xFF) as u8
    }

    pub fn b(self) -> u8 {
        ((self.0 >> 16) & 0xFF) as u8
    }
}

#[derive(Clone, Debug)]
pub struct Config {
    pub candidate_font_size: u32,
    pub highlight_color: ColorRef,
    pub hide_candidate_window: bool,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            candidate_font_size: 14,
            highlight_color: ColorRef::from_rgb(0x00, 0x78, 0xD7), // default #0078D7
            hide_candidate_window: false,
        }
    }
}

impl Config {
    pub fn parse_ini(content: &str) -> Self {
        let mut config = Config::default();
        let mut in_candidate_section = false;

        for line in content.lines() {
            let line = line.trim();
            if line.starts_with(';') || line.starts_with('#') || line.is_empty() {
                continue;
            }

            if line.starts_with('[') && line.ends_with(']') {
                let section = &line[1..line.len() - 1].trim();
                in_candidate_section = section.eq_ignore_ascii_case("CandidateWindow");
                continue;
            }

            if !in_candidate_section {
                continue;
            }

            if let Some((key, val)) = line.split_once('=') {
                let key = key.trim();
                let val = val.trim();

                if key.eq_ignore_ascii_case("CandidateFontSize") {
                    if let Ok(size) = val.parse::<u32>() {
                        config.candidate_font_size = size.clamp(6, 72);
                    }
                } else if key.eq_ignore_ascii_case("HighlightColor") {
                    let hex_str = val.trim_start_matches('#').trim_start_matches("0x").trim_start_matches("0X");
                    if let Ok(num) = u32::from_str_radix(hex_str, 16) {
                        if num <= 0xFFFFFF {
                            let r = ((num >> 16) & 0xFF) as u8;
                            let g = ((num >> 8) & 0xFF) as u8;
                            let b = (num & 0xFF) as u8;
                            config.highlight_color = ColorRef::from_rgb(r, g, b);
                        }
                    }
                } else if key.eq_ignore_ascii_case("HideCandidateWindow") || key.eq_ignore_ascii_case("HideWindow") {
                    config.hide_candidate_window = val == "1" || val.eq_ignore_ascii_case("true");
                }
            }
        }

        config
    }

    pub fn load_from_file(path: &Path) -> Self {
        if let Ok(content) = fs::read_to_string(path) {
            Self::parse_ini(&content)
        } else {
            Self::default()
        }
    }
}

static CONFIG: RwLock<Option<Config>> = RwLock::new(None);

pub fn get_config() -> Config {
    if let Ok(guard) = CONFIG.read() {
        if let Some(ref cfg) = *guard {
            return cfg.clone();
        }
    }

    // Default configuration
    Config::default()
}

pub fn set_config(config: Config) {
    if let Ok(mut guard) = CONFIG.write() {
        *guard = Some(config);
    }
}

pub fn load_dll_directory_config(dll_path: &Path) {
    let ini_path: PathBuf = if dll_path.is_file() {
        dll_path.parent().map(|p| p.join("NoneIME.ini")).unwrap_or_else(|| PathBuf::from("NoneIME.ini"))
    } else {
        dll_path.join("NoneIME.ini")
    };

    let config = Config::load_from_file(&ini_path);
    set_config(config);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_ini() {
        let ini = r#"
[CandidateWindow]
CandidateFontSize=18
HighlightColor=FF5500
HideCandidateWindow=1
"#;
        let cfg = Config::parse_ini(ini);
        assert_eq!(cfg.candidate_font_size, 18);
        assert_eq!(cfg.highlight_color, ColorRef::from_rgb(0xFF, 0x55, 0x00));
        assert!(cfg.hide_candidate_window);
    }

    #[test]
    fn test_parse_ini_clamping() {
        let ini = r#"
[CandidateWindow]
CandidateFontSize=100
HighlightColor=#00FF00
"#;
        let cfg = Config::parse_ini(ini);
        assert_eq!(cfg.candidate_font_size, 72);
        assert_eq!(cfg.highlight_color, ColorRef::from_rgb(0x00, 0xFF, 0x00));
        assert!(!cfg.hide_candidate_window);
    }
}
