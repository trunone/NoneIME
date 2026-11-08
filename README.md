# 最精簡的嘸蝦米輸入法

None IME 是一款以 Rust 語言重寫的 Windows 繁體中文嘸蝦米輸入法，使用社群維護的嘸蝦米字表，透過 Windows Text Services Framework（TSF）提供高效、簡潔的輸入服務。專注於碼表選字與必要的候選字操作，讓嘸蝦米輸入保持簡單直接。

本專案因個人使用需求與興趣而開發，目標是滿足作者目前的輸入需求，並非要完整實作嘸蝦米輸入法或其他輸入法的所有功能。未來的功能增修也會以作者個人需求為主，不承諾涵蓋所有使用情境。

## 功能

- 依嘸蝦米字根查字、選字
- 支援 TSF 組字、候選字視窗、語言列及輸入模式切換
- 全 Rust 現代化重構，記憶體安全與極小程式庫佔用

## 建置

請安裝 [Rust](https://rustup.rs/)（支援 MSVC 或 GNU 工具鏈），並於專案根目錄執行：

```powershell
./build.ps1 -Configuration Release -Platform x64
```

腳本會透過 Cargo 建置 `NoneIME.dll`。DLL 及 `NoneIME.ini` 會輸出至 `bin/<Platform>/<Configuration>/`。

亦可直接使用 Cargo 進行建置與測試：
```bash
cargo test
cargo build --release --target x86_64-pc-windows-msvc
```

可在 DLL 同層的 INI 檔 `[CandidateWindow]` 區段調整 `CandidateFontSize`（字級，點）、`HighlightColor`（RGB 色碼，例如 `0078D7`）及 `HideCandidateWindow`（設為 `1` 可隱藏候選視窗）。設定會在輸入法初始化時讀取。建置不會自動安裝或註冊輸入法。

## 安裝與移除

預設將 x64 Debug 版本註冊給目前 Windows 使用者：

```powershell
./install.ps1
```

可用 `-Configuration Release -Platform x64` 或 `-Platform Win32` 指定其他版本；若要註冊其他位置的 DLL，請使用 `-DllPath`。移除時請使用對應的參數：

```powershell
./uninstall.ps1
```

腳本只註冊或取消註冊 TSF/COM 元件，不會複製或刪除建置檔案。若 Windows 拒絕註冊，請以系統管理員身分執行 PowerShell。安裝後，請到 Windows 的語言與鍵盤設定啟用 None IME。

## 字典資料與檢索演算法

字表來源為 [chinese-opendesktop/cin-tables](https://raw.githubusercontent.com/chinese-opendesktop/cin-tables/refs/heads/master/boshiamy.cin) 的 `boshiamy.cin`。

二進位碼表檔 `boshiamy.bin` 直接嵌入於 Rust 模組中並以 Windows `RCDATA` 資源內聯：
1. **二進位碼表檔案結構 (`boshiamy.bin`)**：
   - **檔頭 (`DictHeader`)**：包含 Magic Code (`NIME`)、版本號、索引項數與候選字總數。
   - **索引表 (`DictIndexEntry[]`)**：將所有字根 Key 依 ASCII 順序排序，每個項目包含字根字集（`key`）、候選字陣列起始偏移量（`phrase_offset`）及數量（`phrase_count`）。
   - **候選字表 (`u16[]`)**：緊密排列的 16-bit Unicode UTF-16 候選字元。
2. **記憶體零複製與高效二分搜尋**：輸入法載入時，直接對內嵌的二進位索引區進行 $O(\log N)$ 階層的二分搜尋（21,949 個獨立字根 Key 僅需約 14 次比較），無執行期文字 I/O，達到極高檢索效率與零延遲反應。

## 授權

本專案採 MIT License。
嘸蝦米字表來自 [chinese-opendesktop/cin-tables](https://github.com/chinese-opendesktop/cin-tables)（`boshiamy.cin`）。
