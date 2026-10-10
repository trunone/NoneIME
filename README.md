# 最精簡的嘸蝦米輸入法

None IME 是一款 Windows 繁體中文嘸蝦米輸入法，使用社群維護的嘸蝦米字表，透過 Windows Text Services Framework（TSF）提供輸入服務。專注於碼表選字與必要的候選字操作，讓嘸蝦米輸入保持簡單直接。

本專案因個人使用需求與興趣而開發，目標是滿足作者目前的輸入需求，並非要完整實作嘸蝦米輸入法或其他輸入法的所有功能。未來的功能增修也會以作者個人需求為主，不承諾涵蓋所有使用情境。

## 功能

- 依嘸蝦米字根查字、選字
- 單字候選提供國語同音字；輸入碼前加單引號可查同音字
- 支援 TSF 組字、候選字視窗、語言列及輸入模式切換

## 建置

請在 Windows 安裝 Visual Studio 的「使用 C++ 的桌面開發」工具及 Windows SDK，並於專案根目錄執行：

```powershell
./build.ps1 -Configuration Release -Platform x64
```

腳本會尋找 MSBuild 並建置 `NoneIME.sln`。DLL、執行時字典及 `NoneIME.ini` 會輸出至 `bin/<Platform>/<Configuration>/`，中間檔案則位於 `obj/<Platform>/<Configuration>/`。

可在 DLL 同層的 INI 檔 `[CandidateWindow]` 區段調整 `CandidateFontSize`（字級，點）及 `HighlightColor`（RGB 色碼，例如 `0078D7`）。候選視窗使用固定最小寬度，較長的候選內容會自動加寬。設定會在輸入法初始化時讀取。建置不會自動安裝或註冊輸入法。

可在 `NoneIME.ini` 的 `[Dictionary]` 區段設定 `CinPath`；相對路徑以 DLL/INI 所在目錄為基準，也可指定絕對路徑。預設值為 `Boshiamy.cin`，安裝器也會依此設定檢查碼表是否存在。

## 安裝與移除

預設將 x64 Debug 版本註冊給目前 Windows 使用者：

```powershell
./install.ps1
```

可用 `-Configuration Release -Platform x64` 或 `-Platform Win32` 指定其他版本；若要註冊其他位置的 DLL，請使用 `-DllPath`，並將 `Boshiamy.cin` 放在 DLL 同一目錄。同音字字典為選用檔案。移除時請使用對應的參數：

```powershell
./uninstall.ps1
```

腳本只註冊或取消註冊 TSF/COM 元件，不會複製或刪除建置檔案。若 Windows 拒絕註冊，請以系統管理員身分執行 PowerShell。安裝後，請到 Windows 的語言與鍵盤設定啟用 None IME。

## 字典資料

嘸蝦米碼表來源為 [jdh8/ibus-boshiamy](https://github.com/jdh8/ibus-boshiamy)，採用 commit `9a8f5dadd8dbb95b0376ac19c7b03826e64a1bbb` 的資料。將上游 UTF-8 `.cin` 檔命名為 `Boshiamy.cin` 放入 `Dictionary`，建置時會直接複製到輸出目錄並由輸入法載入，不需預先轉換：

```powershell
Copy-Item C:\data\boshiamy.cin Dictionary\Boshiamy.cin
./build.ps1 -Configuration Release -Platform x64
```

若要自行重建同音字資料，請先將 `Boshiamy.cin` 放入 `Dictionary`，再從教育部「辭典公眾授權網」下載《重編國語辭典修訂本》資料 ZIP，解壓後使用 `dict_revised_2015_*.xlsx`。產生器直接讀取 CIN 表格，依教育部資料中的注音建立字元到讀音及讀音到同音字清單的二層索引，只保留同時存在於嘸蝦米碼表及教育部資料中的字，並產生 `Boshiamy-Homophones.bin`：

```powershell
$moeDictionaryWorkbookPath = 'C:\data\dict_revised_2015_20260929.xlsx'
powershell -ExecutionPolicy Bypass -File Dictionary/build_boshiamy_homophones.ps1 `
	-MoeDictionaryWorkbookPath $moeDictionaryWorkbookPath
```

目前版本未附獨立的字表授權說明文件；字表授權標示及注意事項見下方「授權」。

## 授權

本專案由作者持有著作權且有權授權的原創部分採 MIT License；第三方程式碼、字典及資料不包含在此授權範圍內。目前儲存庫未附 MIT 授權全文，正式以 MIT 發行時應一併附上授權本文及權利人資訊，並確認只涵蓋有權授權的內容。

TSF 實作改編自 [Microsoft Windows-classic-samples](https://github.com/microsoft/Windows-classic-samples) 的範例；該專案採 MIT License。本專案保留範例程式碼中的 Microsoft 著作權與免責聲明，散布改編部分時亦須保留 MIT 授權要求的著作權及許可聲明。嘸蝦米字表來自 [jdh8/ibus-boshiamy](https://github.com/jdh8/ibus-boshiamy)：上游 README 宣告 GPLv3-or-later（GPLv3+），字表不屬於 MIT 授權，重新散布時須遵守上游授權及保留相關聲明；同音字資料使用外部來源資料，亦須遵守各資料來源的條款。

