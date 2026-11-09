// Boshiamy Composition & Candidates State Machine

use crate::dictionary::get_dictionary_engine;
use crate::key_handler::{classify_key, KeyAction};

pub const PAGE_SIZE: usize = 10;

#[derive(Clone, Debug)]
pub struct CandidateItem {
    pub value: String,
    pub key_code: String,
}

#[derive(Clone, Debug, Default)]
pub struct CompositionEngine {
    /// Currently typed root letters (e.g. "ed", "a", "c")
    pub composition_buffer: String,
    /// List of matched candidate characters
    pub candidates: Vec<CandidateItem>,
    /// Selected index within all candidates
    pub selected_index: usize,
    /// Active page offset (selected_index / PAGE_SIZE)
    pub current_page: usize,
    /// IME Mode: true = Chinese, false = English/alphanumeric
    pub ime_mode_chinese: bool,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum CompositionResult {
    DoNothing,
    Eaten,
    UpdateUI,
    CommitText(String),
}

impl CompositionEngine {
    pub fn new() -> Self {
        Self {
            composition_buffer: String::with_capacity(6),
            candidates: Vec::new(),
            selected_index: 0,
            current_page: 0,
            ime_mode_chinese: true,
        }
    }

    pub fn is_composing(&self) -> bool {
        !self.composition_buffer.is_empty()
    }

    pub fn has_candidates(&self) -> bool {
        !self.candidates.is_empty()
    }

    pub fn page_count(&self) -> usize {
        if self.candidates.is_empty() {
            0
        } else {
            (self.candidates.len() + PAGE_SIZE - 1) / PAGE_SIZE
        }
    }

    pub fn current_page_candidates(&self) -> &[CandidateItem] {
        if self.candidates.is_empty() {
            return &[];
        }
        let start = self.current_page * PAGE_SIZE;
        if start >= self.candidates.len() {
            return &[];
        }
        let end = (start + PAGE_SIZE).min(self.candidates.len());
        &self.candidates[start..end]
    }

    pub fn update_candidates(&mut self) {
        if self.composition_buffer.is_empty() {
            self.candidates.clear();
            self.selected_index = 0;
            self.current_page = 0;
            return;
        }

        let dict = get_dictionary_engine();
        let words = dict.search(&self.composition_buffer);
        let key = self.composition_buffer.clone();

        self.candidates = words
            .into_iter()
            .map(|w| CandidateItem {
                value: w,
                key_code: key.clone(),
            })
            .collect();

        self.selected_index = 0;
        self.current_page = 0;
    }

    pub fn clear(&mut self) {
        self.composition_buffer.clear();
        self.candidates.clear();
        self.selected_index = 0;
        self.current_page = 0;
    }

    pub fn select_next_page(&mut self) -> bool {
        let total_pages = self.page_count();
        if total_pages > 1 && self.current_page + 1 < total_pages {
            self.current_page += 1;
            self.selected_index = self.current_page * PAGE_SIZE;
            true
        } else {
            false
        }
    }

    pub fn select_prev_page(&mut self) -> bool {
        if self.current_page > 0 {
            self.current_page -= 1;
            self.selected_index = self.current_page * PAGE_SIZE;
            true
        } else {
            false
        }
    }

    /// Process keystroke
    pub fn on_key_down(
        &mut self,
        vk: u32,
        ch: char,
        ctrl_down: bool,
        alt_down: bool,
    ) -> CompositionResult {
        if !self.ime_mode_chinese {
            return CompositionResult::DoNothing;
        }

        let action = classify_key(
            vk,
            ch,
            ctrl_down,
            alt_down,
            self.is_composing(),
            self.has_candidates(),
        );

        match action {
            KeyAction::None => CompositionResult::DoNothing,
            KeyAction::EatOnly => CompositionResult::Eaten,
            KeyAction::AddChar(c) => {
                // Boshiamy root keys max length is typically 5
                if self.composition_buffer.len() < 5 {
                    self.composition_buffer.push(c);
                    self.update_candidates();
                    CompositionResult::UpdateUI
                } else {
                    CompositionResult::Eaten
                }
            }
            KeyAction::Backspace => {
                if !self.composition_buffer.is_empty() {
                    self.composition_buffer.pop();
                    self.update_candidates();
                    CompositionResult::UpdateUI
                } else {
                    CompositionResult::DoNothing
                }
            }
            KeyAction::Space => {
                if !self.candidates.is_empty() {
                    let commit_val = self.candidates[self.selected_index].value.clone();
                    self.clear();
                    CompositionResult::CommitText(commit_val)
                } else if !self.composition_buffer.is_empty() {
                    // No candidate found: commit raw buffer
                    let commit_val = self.composition_buffer.clone();
                    self.clear();
                    CompositionResult::CommitText(commit_val)
                } else {
                    CompositionResult::DoNothing
                }
            }
            KeyAction::CommitCandidate(idx) => {
                let actual_index = self.current_page * PAGE_SIZE + idx;
                if actual_index < self.candidates.len() {
                    let commit_val = self.candidates[actual_index].value.clone();
                    self.clear();
                    CompositionResult::CommitText(commit_val)
                } else {
                    CompositionResult::Eaten
                }
            }
            KeyAction::PrevPage => {
                if self.select_prev_page() {
                    CompositionResult::UpdateUI
                } else {
                    CompositionResult::Eaten
                }
            }
            KeyAction::NextPage => {
                if self.select_next_page() {
                    CompositionResult::UpdateUI
                } else {
                    CompositionResult::Eaten
                }
            }
            KeyAction::Escape => {
                self.clear();
                CompositionResult::UpdateUI
            }
            KeyAction::ToggleImeMode => {
                self.ime_mode_chinese = !self.ime_mode_chinese;
                self.clear();
                CompositionResult::UpdateUI
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_composition_workflow() {
        let mut engine = CompositionEngine::new();
        assert!(!engine.is_composing());

        // Type 'a'
        let res = engine.on_key_down(0x41, 'A', false, false);
        assert_eq!(res, CompositionResult::UpdateUI);
        assert!(engine.is_composing());
        assert!(engine.has_candidates());

        // Press Space to commit the first candidate (for 'a', it is "對")
        let res = engine.on_key_down(0x20, ' ', false, false);
        match res {
            CompositionResult::CommitText(s) => assert_eq!(s, "對"),
            other => panic!("Expected commit, got {:?}", other),
        }
        assert!(!engine.is_composing());
    }

    #[test]
    fn test_candidate_number_selection() {
        let mut engine = CompositionEngine::new();
        // Type 'ca' which has ["夕", "虫"]
        engine.on_key_down(0x43, 'C', false, false);
        engine.on_key_down(0x41, 'A', false, false);
        assert!(engine.candidates.len() >= 2);

        let candidate_2 = engine.candidates[1].value.clone();

        // Press '2' (0x32) to commit 2nd candidate
        let res = engine.on_key_down(0x32, '2', false, false);
        match res {
            CompositionResult::CommitText(s) => assert_eq!(s, candidate_2),
            other => panic!("Expected commit candidate 2, got {:?}", other),
        }
    }
}
