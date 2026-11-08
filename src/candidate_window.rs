// Candidate Window Win32 UI implementation

#[cfg(windows)]
mod win32 {
    use std::sync::atomic::{AtomicPtr, Ordering};
    use windows_core::{w, PCWSTR};
    use windows::Win32::Foundation::{COLORREF, HWND, LPARAM, LRESULT, RECT, WPARAM};
    use windows::Win32::Graphics::Gdi::*;
    use windows::Win32::UI::WindowsAndMessaging::*;

    use crate::composition::{CompositionEngine, PAGE_SIZE};
    use crate::config::get_config;

    const WINDOW_CLASS_NAME: PCWSTR = w!("NoneIMECandidateWindow");
    static CANDIDATE_WINDOW_HWND: AtomicPtr<std::ffi::c_void> = AtomicPtr::new(std::ptr::null_mut());

    const CANDWND_ROW_HEIGHT: i32 = 24;
    const CANDWND_BORDER_WIDTH: i32 = 1;
    const CANDWND_MARGIN: i32 = 4;
    const STRING_POSITION: i32 = 4; // characters offset for candidate text

    pub struct CandidateWindow;

    impl CandidateWindow {
        pub fn register_class(instance: windows::Win32::Foundation::HINSTANCE) -> bool {
            unsafe {
                let wnd_class = WNDCLASSEXW {
                    cbSize: std::mem::size_of::<WNDCLASSEXW>() as u32,
                    style: CS_HREDRAW | CS_VREDRAW | CS_IME,
                    lpfnWndProc: Some(candidate_wnd_proc),
                    cbClsExtra: 0,
                    cbWndExtra: 0,
                    hInstance: instance,
                    hIcon: HICON::default(),
                    hCursor: LoadCursorW(None, IDC_ARROW).unwrap_or_default(),
                    hbrBackground: HBRUSH((COLOR_WINDOW.0 + 1) as _),
                    lpszMenuName: PCWSTR::null(),
                    lpszClassName: WINDOW_CLASS_NAME,
                    hIconSm: HICON::default(),
                };

                RegisterClassExW(&wnd_class) != 0
            }
        }

        pub fn get_or_create(parent: Option<HWND>) -> Option<HWND> {
            let existing = CANDIDATE_WINDOW_HWND.load(Ordering::SeqCst);
            if !existing.is_null() && unsafe { IsWindow(Some(HWND(existing))).as_bool() } {
                return Some(HWND(existing));
            }

            unsafe {
                let hwnd = CreateWindowExW(
                    WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                    WINDOW_CLASS_NAME,
                    w!("NoneIME Candidates"),
                    WS_POPUP | WS_BORDER,
                    0,
                    0,
                    150,
                    200,
                    parent,
                    None,
                    None,
                    None,
                );

                if let Ok(hwnd) = hwnd {
                    if !hwnd.0.is_null() {
                        CANDIDATE_WINDOW_HWND.store(hwnd.0, Ordering::SeqCst);
                        return Some(hwnd);
                    }
                }
            }
            None
        }

        pub fn hide() {
            let hwnd_ptr = CANDIDATE_WINDOW_HWND.load(Ordering::SeqCst);
            if !hwnd_ptr.is_null() {
                unsafe {
                    let hwnd = HWND(hwnd_ptr);
                    if IsWindow(Some(hwnd)).as_bool() {
                        let _ = ShowWindow(hwnd, SW_HIDE);
                    }
                }
            }
        }

        pub fn update_and_show(engine: &CompositionEngine, x: i32, y: i32) {
            let cfg = get_config();
            if cfg.hide_candidate_window || !engine.has_candidates() {
                Self::hide();
                return;
            }

            let hwnd = match Self::get_or_create(None) {
                Some(h) => h,
                None => return,
            };

            // Calculate window size based on candidate list
            let candidates = engine.current_page_candidates();
            let count = candidates.len();
            if count == 0 {
                Self::hide();
                return;
            }

            let row_height = (cfg.candidate_font_size as i32 + 8).max(CANDWND_ROW_HEIGHT);
            let total_height = count as i32 * row_height + (CANDWND_BORDER_WIDTH + CANDWND_MARGIN) * 2;
            let total_width = 160.max(STRING_POSITION * (cfg.candidate_font_size as i32) + 80);

            unsafe {
                let _ = SetWindowPos(
                    hwnd,
                    Some(HWND_TOPMOST),
                    x,
                    y,
                    total_width,
                    total_height,
                    SWP_NOACTIVATE | SWP_SHOWWINDOW,
                );
                let _ = InvalidateRect(Some(hwnd), None, true);
                let _ = UpdateWindow(hwnd);
            }
        }
    }

    unsafe extern "system" fn candidate_wnd_proc(
        hwnd: HWND,
        msg: u32,
        wparam: WPARAM,
        lparam: LPARAM,
    ) -> LRESULT {
        match msg {
            WM_PAINT => {
                let mut ps = PAINTSTRUCT::default();
                let hdc = BeginPaint(hwnd, &mut ps);
                if !hdc.0.is_null() {
                    paint_candidates(hwnd, hdc);
                }
                let _ = EndPaint(hwnd, &ps);
                LRESULT(0)
            }
            WM_LBUTTONDOWN => {
                // Click to select candidate item
                let y = (lparam.0 >> 16) as i16 as i32;
                let cfg = get_config();
                let row_height = (cfg.candidate_font_size as i32 + 8).max(CANDWND_ROW_HEIGHT);
                let clicked_idx = ((y - CANDWND_MARGIN) / row_height) as usize;
                crate::tsf::select_active_candidate(clicked_idx);
                LRESULT(0)
            }
            WM_MOUSEACTIVATE => {
                // Do not activate window when clicked
                LRESULT(MA_NOACTIVATE as _)
            }
            WM_SETCURSOR => {
                let _ = SetCursor(LoadCursorW(None, IDC_HAND).ok());
                LRESULT(1)
            }
            WM_DESTROY => {
                CANDIDATE_WINDOW_HWND.store(std::ptr::null_mut(), Ordering::SeqCst);
                LRESULT(0)
            }
            _ => DefWindowProcW(hwnd, msg, wparam, lparam),
        }
    }

    unsafe fn paint_candidates(hwnd: HWND, hdc: HDC) {
        let mut client_rect = RECT::default();
        let _ = GetClientRect(hwnd, &mut client_rect);

        // Fill background
        let bg_brush = GetSysColorBrush(COLOR_WINDOW);
        let _ = FillRect(hdc, &client_rect, bg_brush);

        let cfg = get_config();

        // Create font
        let hfont = CreateFontW(
            cfg.candidate_font_size as i32,
            0,
            0,
            0,
            FW_NORMAL.0 as i32,
            0,
            0,
            0,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY,
            DEFAULT_PITCH.0 as u32,
            w!("Microsoft JhengHei UI"),
        );
        let old_font = SelectObject(hdc, hfont.into());

        let _ = SetBkMode(hdc, TRANSPARENT);

        // Query active engine candidates via thread-safe getter
        if let Some(engine) = crate::tsf::get_active_composition_engine() {
            let candidates = engine.current_page_candidates();
            let row_height = (cfg.candidate_font_size as i32 + 8).max(CANDWND_ROW_HEIGHT);

            for (idx, item) in candidates.iter().enumerate() {
                let y = CANDWND_MARGIN + idx as i32 * row_height;
                let is_selected = (engine.current_page * PAGE_SIZE + idx) == engine.selected_index;

                let row_rect = RECT {
                    left: CANDWND_MARGIN,
                    top: y,
                    right: client_rect.right - CANDWND_MARGIN,
                    bottom: y + row_height,
                };

                if is_selected {
                    let hl_color = COLORREF(cfg.highlight_color.0);
                    let hl_brush = CreateSolidBrush(hl_color);
                    let _ = FillRect(hdc, &row_rect, hl_brush);
                    let _ = DeleteObject(hl_brush.into());
                    let _ = SetTextColor(hdc, COLORREF(0x00FFFFFF)); // white text for highlight
                } else {
                    let _ = SetTextColor(hdc, COLORREF(0x00000000)); // black text
                }

                // Render number index
                let num_str = format!("{}.", (idx + 1) % 10);
                let num_wide: Vec<u16> = num_str.encode_utf16().collect();
                let _ = TextOutW(hdc, row_rect.left + 4, y + 2, &num_wide);

                // Render candidate value
                let val_wide: Vec<u16> = item.value.encode_utf16().collect();
                let _ = TextOutW(hdc, row_rect.left + 32, y + 2, &val_wide);
            }
        }

        let _ = SelectObject(hdc, old_font);
        let _ = DeleteObject(hfont.into());

        // Draw border
        let border_pen = CreatePen(PS_SOLID, CANDWND_BORDER_WIDTH, COLORREF(0x00A0A0A0));
        let old_pen = SelectObject(hdc, border_pen.into());
        let old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        let _ = Rectangle(hdc, 0, 0, client_rect.right, client_rect.bottom);
        let _ = SelectObject(hdc, old_brush);
        let _ = SelectObject(hdc, old_pen);
        let _ = DeleteObject(border_pen.into());
    }
}

#[cfg(windows)]
pub use win32::CandidateWindow;

#[cfg(not(windows))]
pub struct CandidateWindow;

#[cfg(not(windows))]
impl CandidateWindow {
    pub fn hide() {}
    pub fn update_and_show(_engine: &crate::composition::CompositionEngine, _x: i32, _y: i32) {}
}
