# 最精簡的嘸蝦米輸入法

None IME 是一款 Windows 繁體中文嘸蝦米輸入法，使用社群維護的嘸蝦米字表，透過 Windows Text Services Framework（TSF）提供輸入服務。專注於碼表選字與必要的候選字操作，讓嘸蝦米輸入保持簡單直接。

本專案因個人使用需求與興趣而開發，目標是滿足作者目前的輸入需求，並非要完整實作嘸蝦米輸入法或其他輸入法的所有功能。未來的功能增修也會以作者個人需求為主，不承諾涵蓋所有使用情境。

## 功能

- 依嘸蝦米字根查字、選字
- 支援 TSF 組字、候選字視窗、語言列及輸入模式切換

## 建置

請在 Windows 安裝 Visual Studio 的「使用 C++ 的桌面開發」工具及 Windows SDK，並於專案根目錄執行：

```powershell
./build.ps1 -Configuration Release -Platform x64
```

腳本會尋找 MSBuild 並建置 `NoneIME.sln`。DLL 及 `NoneIME.ini` 會輸出至 `bin/<Platform>/<Configuration>/`，中間檔案則位於 `obj/<Platform>/<Configuration>/`。

可在 DLL 同層的 INI 檔 `[CandidateWindow]` 區段調整 `CandidateFontSize`（字級，點）及 `HighlightColor`（RGB 色碼，例如 `0078D7`）。候選視窗使用固定最小寬度，較長的候選內容會自動加寬。設定會在輸入法初始化時讀取。建置不會自動安裝或註冊輸入法。

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

輸入法不再於執行時期讀取 `.cin` 文字檔，而是預先編譯為二進位碼表檔 `boshiamy.bin` 並以 `RCDATA` 資源（`IDR_DICTIONARY_DATA`）直接內嵌於 DLL 中：
1. **二進位碼表檔案結構 (`boshiamy.bin`)**：
   - **檔頭 (`DictHeader`)**：包含 Magic Code (`NIME`)、版本號、索引項數與候選字總數。
   - **索引表 (`DictIndexEntry[]`)**：將所有字根 Key 依 ASCII 順序排序，每個項目包含字根字集（`key`）、候選字陣列起始偏移量（`phraseOffset`）及數量（`phraseCount`）。
   - **候選字表 (`wchar_t[]`)**：緊密排列的 16-bit Unicode UTF-16 候選字元。
2. **記憶體零複製與高效二分搜尋**：輸入法載入 DLL 時，透過 `LockResource` 直接取得進程記憶體中的二進位資源指標，無須額外分配堆記憶體或動態字串解析。使用者輸入字根時，直接在二進位索引區進行 $O(\log N)$ 階層的二分搜尋（21,949 個獨立字根 Key 僅需約 14 次比較），達到極高檢索效率與零延遲反應。

## 授權

本專案由作者持有著作權且有權授權的原創部分採 MIT License；第三方程式碼、字典及資料不包含在此授權範圍內。目前儲存庫未附 MIT 授權全文，正式以 MIT 發行時應一併附上授權本文及權利人資訊，並確認只涵蓋有權授權的內容。

TSF 實作改編自 [Microsoft Windows-classic-samples](https://github.com/microsoft/Windows-classic-samples) 的範例；該專案採 MIT License。本專案保留範例程式碼中的 Microsoft 著作權與免責聲明，散布改編部分時亦須保留 MIT 授權要求的著作權及許可聲明。嘸蝦米字表來自 [cin-tables](https://github.com/chinese-opendesktop/cin-tables)：上游 README 宣告 GPLv3-or-later（GPLv3+），字表不屬於 MIT 授權，重新散布時須遵守上游授權及保留相關聲明。
