// Boshiamy binary table dictionary engine

pub const DICT_MAGIC: [u8; 4] = *b"NIME";
pub const DICT_VERSION: u32 = 1;

#[repr(C, packed)]
#[derive(Clone, Copy, Debug)]
pub struct DictHeader {
    pub magic: [u8; 4],
    pub version: u32,
    pub index_count: u32,
    pub phrase_count: u32,
}

#[repr(C, packed)]
#[derive(Clone, Copy, Debug)]
pub struct DictIndexEntry {
    pub key: [u8; 6],
    pub phrase_offset: u16,
    pub phrase_count: u16,
}

pub struct TableDictionaryEngine {
    data: &'static [u8],
    index_offset: usize,
    index_count: usize,
    phrases_offset: usize,
    phrase_count: usize,
}

impl TableDictionaryEngine {
    /// Create engine from a binary slice in memory
    pub fn from_bytes(data: &'static [u8]) -> Option<Self> {
        if data.len() < std::mem::size_of::<DictHeader>() {
            return None;
        }

        let magic = &data[0..4];
        if magic != DICT_MAGIC {
            return None;
        }

        let version = u32::from_le_bytes(data[4..8].try_into().ok()?);
        if version != DICT_VERSION {
            return None;
        }

        let index_count = u32::from_le_bytes(data[8..12].try_into().ok()?) as usize;
        let phrase_count = u32::from_le_bytes(data[12..16].try_into().ok()?) as usize;

        let header_size = std::mem::size_of::<DictHeader>();
        let index_size = index_count * std::mem::size_of::<DictIndexEntry>();
        let phrase_size = phrase_count * std::mem::size_of::<u16>();

        if data.len() < header_size + index_size + phrase_size {
            return None;
        }

        Some(Self {
            data,
            index_offset: header_size,
            index_count,
            phrases_offset: header_size + index_size,
            phrase_count,
        })
    }

    #[inline]
    fn get_entry(&self, idx: usize) -> ([u8; 6], u16, u16) {
        let entry_start = self.index_offset + idx * 10;
        let mut key = [0u8; 6];
        key.copy_from_slice(&self.data[entry_start..entry_start + 6]);
        let offset = u16::from_le_bytes(self.data[entry_start + 6..entry_start + 8].try_into().unwrap());
        let count = u16::from_le_bytes(self.data[entry_start + 8..entry_start + 10].try_into().unwrap());
        (key, offset, count)
    }

    #[inline]
    fn get_phrase_u16(&self, phrase_idx: usize) -> u16 {
        let p_start = self.phrases_offset + phrase_idx * 2;
        u16::from_le_bytes(self.data[p_start..p_start + 2].try_into().unwrap())
    }

    /// Search for candidate characters by key (e.g., "a", "c", "ed", etc.)
    pub fn search(&self, key: &str) -> Vec<String> {
        let key_bytes = key.as_bytes();
        if key_bytes.is_empty() || key_bytes.len() >= 6 {
            return Vec::new();
        }

        let mut low = 0isize;
        let mut high = (self.index_count as isize) - 1;

        while low <= high {
            let mid = low + (high - low) / 2;
            let (entry_key_buf, phrase_offset, phrase_count) = self.get_entry(mid as usize);

            let entry_key = match entry_key_buf.iter().position(|&b| b == 0) {
                Some(len) => &entry_key_buf[..len],
                None => &entry_key_buf[..],
            };

            match key_bytes.cmp(entry_key) {
                std::cmp::Ordering::Equal => {
                    let offset = phrase_offset as usize;
                    let count = phrase_count as usize;
                    let mut results = Vec::with_capacity(count);
                    for i in 0..count {
                        if offset + i < self.phrase_count {
                            let utf16_val = self.get_phrase_u16(offset + i);
                            if let Ok(ch) = String::from_utf16(&[utf16_val]) {
                                results.push(ch);
                            }
                        }
                    }
                    return results;
                }
                std::cmp::Ordering::Less => {
                    high = mid - 1;
                }
                std::cmp::Ordering::Greater => {
                    low = mid + 1;
                }
            }
        }

        Vec::new()
    }
}

pub static EMBEDDED_DICT_DATA: &[u8] = include_bytes!("../Dictionary/boshiamy.bin");

static DICT_ENGINE: std::sync::OnceLock<TableDictionaryEngine> = std::sync::OnceLock::new();

pub fn get_dictionary_engine() -> &'static TableDictionaryEngine {
    DICT_ENGINE.get_or_init(|| {
        TableDictionaryEngine::from_bytes(EMBEDDED_DICT_DATA)
            .expect("Failed to parse embedded Boshiamy dictionary")
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_dictionary_lookup() {
        let engine = get_dictionary_engine();
        // Test single root 'a'
        let res_a = engine.search("a");
        assert!(!res_a.is_empty(), "Candidates for 'a' should not be empty");
        assert!(res_a.contains(&"對".to_string()), "Candidates for 'a' should contain '對'");

        // Test root 'c'
        let res_c = engine.search("c");
        assert!(!res_c.is_empty(), "Candidates for 'c' should not be empty");

        // Test root ',' (comma key in Boshiamy)
        let res_comma = engine.search(",");
        assert!(!res_comma.is_empty(), "Candidates for ',' should not be empty");

        // Non-existent key
        let res_empty = engine.search("zzzzz");
        assert!(res_empty.is_empty());
    }
}
