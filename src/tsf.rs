// Windows Text Services Framework (TSF) COM interfaces implementation

#[cfg(windows)]
mod win32_tsf {
    use std::cell::RefCell;
    use std::rc::Rc;
    use std::sync::atomic::{AtomicI32, Ordering};
use std::mem::ManuallyDrop;
use windows_core::{implement, Interface, IUnknownImpl, Ref, Result, BOOL, GUID};
    use windows::Win32::Foundation::*;
    use windows::Win32::System::Com::*;
    use windows::Win32::UI::Input::KeyboardAndMouse::{GetKeyState, VK_CONTROL, VK_MENU, VK_SHIFT};
    use windows::Win32::UI::TextServices::*;

    use crate::candidate_window::CandidateWindow;
    use crate::composition::{CompositionEngine, CompositionResult};

    pub static DLL_REF_COUNT: AtomicI32 = AtomicI32::new(0);

    pub const GUID_PRESERVED_KEY_IME_MODE: GUID = GUID::from_u128(0x4b62b54b_f828_43b5_9095_a96df9cbdf38);
    pub const TF_MOD_ON_KEYUP_SHIFT_ONLY: u32 = 0x00010000 | TF_MOD_ON_KEYUP;

struct ActiveService {
    client_id: u32,
    thread_mgr: ITfThreadMgr,
    engine: Rc<RefCell<CompositionEngine>>,
    composition: Rc<RefCell<Option<ITfComposition>>>,
    text_processor: ITfTextInputProcessor,
}

    thread_local! {
    static ACTIVE_SERVICE: RefCell<Option<ActiveService>> = RefCell::new(None);
    }

    pub fn get_active_composition_engine() -> Option<CompositionEngine> {
    ACTIVE_SERVICE.with(|cell| {
        cell.borrow().as_ref().map(|s| s.engine.borrow().clone())
        })
    }

    pub fn select_active_candidate(idx: usize) {
    let service = ACTIVE_SERVICE.with(|c| c.borrow().as_ref().map(|s| {
        (s.client_id, s.thread_mgr.clone(), s.engine.clone(), s.composition.clone(), s.text_processor.clone())
    }));

    if let Some((tid, tm, engine_rc, comp_rc, processor)) = service {
            let text = {
                let mut engine = engine_rc.borrow_mut();
                let actual_idx = engine.current_page * crate::composition::PAGE_SIZE + idx;
                if actual_idx < engine.candidates.len() {
                    let val = engine.candidates[actual_idx].value.clone();
                    engine.clear();
                    Some(val)
                } else {
                    None
                }
            };

            if let Some(text) = text {
                unsafe {
                    if let Ok(doc_mgr) = tm.GetFocus() {
                        if let Ok(context) = doc_mgr.GetTop() {
                        let session = EditSession::new_commit(text, context.clone(), processor, comp_rc.clone());
                            let edit_session: ITfEditSession = session.into();
                            let _ = context.RequestEditSession(tid, &edit_session, TF_ES_SYNC | TF_ES_READWRITE);
                        }
                    }
                }
            *comp_rc.borrow_mut() = None;
                CandidateWindow::hide();
            }
        }
    }

    #[implement(
        ITfTextInputProcessor,
        ITfTextInputProcessorEx,
        ITfThreadMgrEventSink,
        ITfKeyEventSink,
        ITfCompositionSink
    )]
    pub struct NoneIME {
        client_id: RefCell<u32>,
        thread_mgr: RefCell<Option<ITfThreadMgr>>,
        pub engine: Rc<RefCell<CompositionEngine>>,
    pub composition: Rc<RefCell<Option<ITfComposition>>>,
    }

    impl NoneIME {
        pub fn new() -> Self {
            DLL_REF_COUNT.fetch_add(1, Ordering::SeqCst);
            Self {
                client_id: RefCell::new(0),
                thread_mgr: RefCell::new(None),
                engine: Rc::new(RefCell::new(CompositionEngine::new())),
                composition: Rc::new(RefCell::new(None)),
            }
        }
    }

    impl Drop for NoneIME {
        fn drop(&mut self) {
            DLL_REF_COUNT.fetch_sub(1, Ordering::SeqCst);
        }
    }

    impl ITfTextInputProcessor_Impl for NoneIME_Impl {
        fn Activate(&self, ptim: Ref<'_, ITfThreadMgr>, tid: u32) -> Result<()> {
            self.ActivateEx(ptim, tid, 0)
        }

        fn Deactivate(&self) -> Result<()> {
            if let Some(ref tm) = *self.thread_mgr.borrow() {
                if let Ok(key_mgr) = tm.cast::<ITfKeystrokeMgr>() {
                    let tid = *self.client_id.borrow();
                    unsafe {
                        let _ = key_mgr.UnadviseKeyEventSink(tid);
                        let preserved = TF_PRESERVEDKEY {
                            uVKey: VK_SHIFT.0 as u32,
                            uModifiers: TF_MOD_ON_KEYUP_SHIFT_ONLY,
                        };
                        let _ = key_mgr.UnpreserveKey(&GUID_PRESERVED_KEY_IME_MODE, &preserved);
                    }
                }
            }

            CandidateWindow::hide();
            *self.composition.borrow_mut() = None;
            *self.thread_mgr.borrow_mut() = None;
            *self.client_id.borrow_mut() = 0;
            self.engine.borrow_mut().clear();

            ACTIVE_SERVICE.with(|cell| {
                *cell.borrow_mut() = None;
            });

            Ok(())
        }
    }

    impl ITfTextInputProcessorEx_Impl for NoneIME_Impl {
        fn ActivateEx(&self, ptim: Ref<'_, ITfThreadMgr>, tid: u32, _flags: u32) -> Result<()> {
            *self.client_id.borrow_mut() = tid;
            let tm = ptim.ok()?;
            *self.thread_mgr.borrow_mut() = Some(tm.clone());

            // Advise key event sink
            if let Ok(key_mgr) = tm.cast::<ITfKeystrokeMgr>() {
                unsafe {
                    let sink: ITfKeyEventSink = self.to_interface();
                    key_mgr.AdviseKeyEventSink(tid, &sink, true)?;

                    // Register Shift key as PreservedKey to toggle IME Mode
                    let preserved = TF_PRESERVEDKEY {
                        uVKey: VK_SHIFT.0 as u32,
                        uModifiers: TF_MOD_ON_KEYUP_SHIFT_ONLY,
                    };
                    let desc: Vec<u16> = "Chinese/English input (Shift)".encode_utf16().collect();
                    let _ = key_mgr.PreserveKey(tid, &GUID_PRESERVED_KEY_IME_MODE, &preserved, &desc);
                }
            }

            // Sync shared service pointer with thread local
            ACTIVE_SERVICE.with(|cell| {
                *cell.borrow_mut() = Some(ActiveService {
                    client_id: tid,
                    thread_mgr: tm.clone(),
                    engine: self.engine.clone(),
                    composition: self.composition.clone(),
                    text_processor: self.to_interface(),
                });
            });

            Ok(())
        }
    }

    impl ITfThreadMgrEventSink_Impl for NoneIME_Impl {
        fn OnInitDocumentMgr(&self, _pdim: Ref<'_, ITfDocumentMgr>) -> Result<()> {
            Ok(())
        }
        fn OnUninitDocumentMgr(&self, _pdim: Ref<'_, ITfDocumentMgr>) -> Result<()> {
            Ok(())
        }
        fn OnSetFocus(&self, _pdimfocus: Ref<'_, ITfDocumentMgr>, _pdimprevfocus: Ref<'_, ITfDocumentMgr>) -> Result<()> {
            Ok(())
        }
        fn OnPushContext(&self, _pic: Ref<'_, ITfContext>) -> Result<()> {
            Ok(())
        }
        fn OnPopContext(&self, _pic: Ref<'_, ITfContext>) -> Result<()> {
            Ok(())
        }
    }

    impl ITfKeyEventSink_Impl for NoneIME_Impl {
        fn OnSetFocus(&self, _fforeground: BOOL) -> Result<()> {
            Ok(())
        }

        fn OnTestKeyDown(&self, _pic: Ref<'_, ITfContext>, wparam: WPARAM, _lparam: LPARAM) -> Result<BOOL> {
            let vk = wparam.0 as u32;
            let ch = char::from_u32(vk).unwrap_or('\0');
            let is_comp = self.engine.borrow().is_composing();
            let has_cand = self.engine.borrow().has_candidates();

            let ctrl_down = unsafe { (GetKeyState(VK_CONTROL.0 as i32) as u16 & 0x8000) != 0 };
            let alt_down = unsafe { (GetKeyState(VK_MENU.0 as i32) as u16 & 0x8000) != 0 };

            let action = crate::key_handler::classify_key(vk, ch, ctrl_down, alt_down, is_comp, has_cand);
            Ok(BOOL(if action != crate::key_handler::KeyAction::None { 1 } else { 0 }))
        }

        fn OnKeyDown(&self, pic: Ref<'_, ITfContext>, wparam: WPARAM, _lparam: LPARAM) -> Result<BOOL> {
            let context = pic.ok()?;
            let vk = wparam.0 as u32;
            let ch = char::from_u32(vk).unwrap_or('\0');

            let ctrl_down = unsafe { (GetKeyState(VK_CONTROL.0 as i32) as u16 & 0x8000) != 0 };
            let alt_down = unsafe { (GetKeyState(VK_MENU.0 as i32) as u16 & 0x8000) != 0 };

            let mut engine = self.engine.borrow_mut();
            let result = engine.on_key_down(vk, ch, ctrl_down, alt_down);

            match result {
                CompositionResult::DoNothing => Ok(BOOL(0)),
                CompositionResult::Eaten => Ok(BOOL(1)),
                CompositionResult::UpdateUI => {
                    drop(engine);
                    self.update_composition_ui(context)?;
                    Ok(BOOL(1))
                }
                CompositionResult::CommitText(text) => {
                    drop(engine);
                    self.commit_text(context, &text)?;
                    Ok(BOOL(1))
                }
            }
        }

        fn OnTestKeyUp(&self, _pic: Ref<'_, ITfContext>, _wparam: WPARAM, _lparam: LPARAM) -> Result<BOOL> {
            Ok(BOOL(0))
        }

        fn OnKeyUp(&self, _pic: Ref<'_, ITfContext>, _wparam: WPARAM, _lparam: LPARAM) -> Result<BOOL> {
            Ok(BOOL(0))
        }

        fn OnPreservedKey(&self, _pic: Ref<'_, ITfContext>, _rguid: *const GUID) -> Result<BOOL> {
            let mut engine = self.engine.borrow_mut();
            engine.ime_mode_chinese = !engine.ime_mode_chinese;
            engine.clear();
            CandidateWindow::hide();
            Ok(BOOL(1))
        }
    }

    impl ITfCompositionSink_Impl for NoneIME_Impl {
        fn OnCompositionTerminated(&self, _ecwrite: u32, _pcomposition: Ref<'_, ITfComposition>) -> Result<()> {
            *self.composition.borrow_mut() = None;
            self.engine.borrow_mut().clear();
            CandidateWindow::hide();
            Ok(())
        }
    }

    impl NoneIME_Impl {
        fn update_composition_ui(&self, context: &ITfContext) -> Result<()> {
            let engine = self.engine.borrow();
            if engine.is_composing() {
                let text = engine.composition_buffer.clone();
                let tid = *self.client_id.borrow();

                let session = EditSession::new_composition(
                    text,
                    context.clone(),
                    self.to_interface(),
                    self.composition.clone(),
                );
                let edit_session: ITfEditSession = session.into();
                unsafe {
                    let _ = context.RequestEditSession(tid, &edit_session, TF_ES_SYNC | TF_ES_READWRITE);
                }

                // If candidate window position was updated during edit session, update_and_show will use it
                CandidateWindow::update_and_show(&engine);
            } else {
                CandidateWindow::hide();
            }
            Ok(())
        }

        pub fn commit_text(&self, context: &ITfContext, text: &str) -> Result<()> {
            let tid = *self.client_id.borrow();
            let session = EditSession::new_commit(
                text.to_string(),
                context.clone(),
                self.to_interface(),
                self.composition.clone(),
            );
            let edit_session: ITfEditSession = session.into();
            unsafe {
                let _ = context.RequestEditSession(tid, &edit_session, TF_ES_SYNC | TF_ES_READWRITE);
            }
            *self.composition.borrow_mut() = None;
            CandidateWindow::hide();
            Ok(())
        }
    }

    #[implement(ITfEditSession)]
    pub struct EditSession {
        pub text: String,
        pub is_commit: bool,
        pub context: ITfContext,
        pub text_service: ITfTextInputProcessor,
        pub composition: Rc<RefCell<Option<ITfComposition>>>,
    }

    impl EditSession {
        pub fn new_composition(
            text: String,
            context: ITfContext,
            text_service: ITfTextInputProcessor,
            composition: Rc<RefCell<Option<ITfComposition>>>,
        ) -> Self {
            Self {
                text,
                is_commit: false,
                context,
                text_service,
                composition,
            }
        }

        pub fn new_commit(
            text: String,
            context: ITfContext,
            text_service: ITfTextInputProcessor,
            composition: Rc<RefCell<Option<ITfComposition>>>,
        ) -> Self {
            Self {
                text,
                is_commit: true,
                context,
                text_service,
                composition,
            }
        }
    }

    impl ITfEditSession_Impl for EditSession_Impl {
        fn DoEditSession(&self, ec: u32) -> Result<()> {
            let text_wide: Vec<u16> = self.text.encode_utf16().collect();
            unsafe {
                let active_comp = self.composition.borrow().clone();

                if let Some(comp) = active_comp {
                    if let Ok(range) = comp.GetRange() {
                        let _ = range.SetText(ec, 0, &text_wide);

                        if !self.is_commit {
                            // Update selection caret to end of composition range
                            if let Ok(selection_range) = range.Clone() {
                                let _ = selection_range.Collapse(ec, TF_ANCHOR_END);
                                let tf_sel = TF_SELECTION {
                                    range: ManuallyDrop::new(Some(selection_range.clone())),
                                    style: TF_SELECTIONSTYLE {
                                        ase: TF_AE_NONE,
                                        fInterimChar: BOOL(0),
                                    },
                                };
                                let _ = self.context.SetSelection(ec, &[tf_sel]);

                                // Query caret position for candidate window
                                if let Ok(view) = self.context.GetActiveView() {
                                    let mut rc = RECT::default();
                                    let mut clipped = BOOL(0);
                                    if view.GetTextExt(ec, &selection_range, &mut rc, &mut clipped).is_ok() {
                                        CandidateWindow::set_target_position(rc.left, rc.bottom + 2);
                                    }
                                }
                            }
                        } else {
                            let _ = comp.EndComposition(ec);
                            *self.composition.borrow_mut() = None;
                            let _ = range.Collapse(ec, TF_ANCHOR_END);
                            let tf_sel = TF_SELECTION {
                                range: ManuallyDrop::new(Some(range)),
                                style: TF_SELECTIONSTYLE {
                                    ase: TF_AE_NONE,
                                    fInterimChar: BOOL(0),
                                },
                            };
                            let _ = self.context.SetSelection(ec, &[tf_sel]);
                        }
                    }
                } else if let Ok(insert) = self.context.cast::<ITfInsertAtSelection>() {
                    if let Ok(range) = insert.InsertTextAtSelection(ec, INSERT_TEXT_AT_SELECTION_FLAGS(0), &text_wide) {
                        if !self.is_commit {
                            if let Ok(ctx_comp) = self.context.cast::<ITfContextComposition>() {
                                if let Ok(sink) = self.text_service.cast::<ITfCompositionSink>() {
                                    if let Ok(new_comp) = ctx_comp.StartComposition(ec, &range, &sink) {
                                        *self.composition.borrow_mut() = Some(new_comp);

                                        // Set selection and query position
                                        if let Ok(selection_range) = range.Clone() {
                                            let _ = selection_range.Collapse(ec, TF_ANCHOR_END);
                                            let tf_sel = TF_SELECTION {
                                                range: ManuallyDrop::new(Some(selection_range.clone())),
                                                style: TF_SELECTIONSTYLE {
                                                    ase: TF_AE_NONE,
                                                    fInterimChar: BOOL(0),
                                                },
                                            };
                                            let _ = self.context.SetSelection(ec, &[tf_sel]);

                                            if let Ok(view) = self.context.GetActiveView() {
                                                let mut rc = RECT::default();
                                                let mut clipped = BOOL(0);
                                                if view.GetTextExt(ec, &selection_range, &mut rc, &mut clipped).is_ok() {
                                                    CandidateWindow::set_target_position(rc.left, rc.bottom + 2);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            let _ = range.Collapse(ec, TF_ANCHOR_END);
                            let tf_sel = TF_SELECTION {
                                range: ManuallyDrop::new(Some(range)),
                                style: TF_SELECTIONSTYLE {
                                    ase: TF_AE_NONE,
                                    fInterimChar: BOOL(0),
                                },
                            };
                            let _ = self.context.SetSelection(ec, &[tf_sel]);
                        }
                    }
                }
            }
            Ok(())
        }
    }

    #[implement(IClassFactory)]
    pub struct ClassFactory;

    impl IClassFactory_Impl for ClassFactory_Impl {
        fn CreateInstance(&self, punkouter: Ref<'_, windows_core::IUnknown>, riid: *const GUID, ppvobject: *mut *mut std::ffi::c_void) -> Result<()> {
            if punkouter.is_some() {
                return Err(windows_core::Error::from_hresult(CLASS_E_NOAGGREGATION));
            }
            let processor: ITfTextInputProcessor = NoneIME::new().into();
            unsafe { processor.query(riid, ppvobject).ok() }
        }

        fn LockServer(&self, flock: BOOL) -> Result<()> {
            if flock.as_bool() {
                DLL_REF_COUNT.fetch_add(1, Ordering::SeqCst);
            } else {
                DLL_REF_COUNT.fetch_sub(1, Ordering::SeqCst);
            }
            Ok(())
        }
    }
}

#[cfg(windows)]
pub use win32_tsf::*;
