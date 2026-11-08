// COM Registration and unregistration logic for None IME

#[cfg(windows)]
mod win32_register {
    use std::path::Path;
    use windows_core::{w, Interface, PCWSTR, GUID};
    use windows::Win32::System::Com::*;
    use windows::Win32::System::Registry::*;
    use windows::Win32::UI::Input::KeyboardAndMouse::HKL;
    use windows::Win32::UI::TextServices::*;

    pub const CLSID_NONE_IME: GUID = GUID::from_u128(0xd2291a80_84d8_4641_9ab2_bdd1472c846b);
    pub const GUID_PROFILE_NONE_IME: GUID = GUID::from_u128(0x83955c0e_2c09_47a5_bcf3_f2b98e11ee8b);

    pub const LANG_TRADITIONAL_CHINESE: u16 = 0x0404;

    pub unsafe fn register_server(module_path: &Path) -> Result<(), windows_core::Error> {
        let clsid_str = format!("{{{:?}}}", CLSID_NONE_IME);
        let subkey = format!("CLSID\\{}\\InprocServer32", clsid_str);

        // Register InprocServer32 in HKEY_CLASSES_ROOT
        let subkey_wide: Vec<u16> = subkey.encode_utf16().chain(std::iter::once(0)).collect();
        let mut hkey = HKEY::default();

        let res = RegCreateKeyW(
            HKEY_CLASSES_ROOT,
            PCWSTR(subkey_wide.as_ptr()),
            &mut hkey,
        );

        if res.is_ok() {
            let path_str = module_path.to_string_lossy();
            let path_wide: Vec<u16> = path_str.encode_utf16().chain(std::iter::once(0)).collect();

            let _ = RegSetValueExW(
                hkey,
                PCWSTR::null(),
                Some(0),
                REG_SZ,
                Some(std::slice::from_raw_parts(
                    path_wide.as_ptr() as *const u8,
                    path_wide.len() * 2,
                )),
            );

            let threading_model: Vec<u16> = "Apartment".encode_utf16().chain(std::iter::once(0)).collect();
            let _ = RegSetValueExW(
                hkey,
                w!("ThreadingModel"),
                Some(0),
                REG_SZ,
                Some(std::slice::from_raw_parts(
                    threading_model.as_ptr() as *const u8,
                    threading_model.len() * 2,
                )),
            );

            let _ = RegCloseKey(hkey);
        }

        // Register TSF Profile
        let profiles: ITfInputProcessorProfiles = CoCreateInstance(&CLSID_TF_InputProcessorProfiles, None, CLSCTX_INPROC_SERVER)?;
        let name: Vec<u16> = "None IME (Traditional Chinese)".encode_utf16().collect();
        let icon_file: Vec<u16> = module_path.to_string_lossy().encode_utf16().collect();

        let _ = profiles.Register(&CLSID_NONE_IME);

        let profile_mgr: ITfInputProcessorProfileMgr = profiles.cast()?;
        let _ = profile_mgr.RegisterProfile(
            &CLSID_NONE_IME,
            LANG_TRADITIONAL_CHINESE,
            &GUID_PROFILE_NONE_IME,
            &name,
            &icon_file,
            0,
            HKL::default(),
            0,
            true,
            0,
        );

        // Register Categories
        let category_mgr: ITfCategoryMgr = CoCreateInstance(&CLSID_TF_CategoryMgr, None, CLSCTX_INPROC_SERVER)?;
        let _ = category_mgr.RegisterCategory(&CLSID_NONE_IME, &GUID_TFCAT_TIP_KEYBOARD, &CLSID_NONE_IME);
        let _ = category_mgr.RegisterCategory(&CLSID_NONE_IME, &GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER, &CLSID_NONE_IME);
        let _ = category_mgr.RegisterCategory(&CLSID_NONE_IME, &GUID_TFCAT_TIPCAP_UIELEMENTENABLED, &CLSID_NONE_IME);
        let _ = category_mgr.RegisterCategory(&CLSID_NONE_IME, &GUID_TFCAT_TIPCAP_SECUREMODE, &CLSID_NONE_IME);

        Ok(())
    }

    pub unsafe fn unregister_server() -> Result<(), windows_core::Error> {
        // Unregister categories
        if let Ok(category_mgr) = CoCreateInstance::<_, ITfCategoryMgr>(&CLSID_TF_CategoryMgr, None, CLSCTX_INPROC_SERVER) {
            let _ = category_mgr.UnregisterCategory(&CLSID_NONE_IME, &GUID_TFCAT_TIP_KEYBOARD, &CLSID_NONE_IME);
            let _ = category_mgr.UnregisterCategory(&CLSID_NONE_IME, &GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER, &CLSID_NONE_IME);
            let _ = category_mgr.UnregisterCategory(&CLSID_NONE_IME, &GUID_TFCAT_TIPCAP_UIELEMENTENABLED, &CLSID_NONE_IME);
            let _ = category_mgr.UnregisterCategory(&CLSID_NONE_IME, &GUID_TFCAT_TIPCAP_SECUREMODE, &CLSID_NONE_IME);
        }

        // Unregister profile
        if let Ok(profiles) = CoCreateInstance::<_, ITfInputProcessorProfiles>(&CLSID_TF_InputProcessorProfiles, None, CLSCTX_INPROC_SERVER) {
            if let Ok(profile_mgr) = profiles.cast::<ITfInputProcessorProfileMgr>() {
                let _ = profile_mgr.UnregisterProfile(&CLSID_NONE_IME, LANG_TRADITIONAL_CHINESE, &GUID_PROFILE_NONE_IME, 0);
            }
            let _ = profiles.Unregister(&CLSID_NONE_IME);
        }

        // Remove registry keys
        let clsid_str = format!("{{{:?}}}", CLSID_NONE_IME);
        let key_inproc = format!("CLSID\\{}\\InprocServer32", clsid_str);
        let key_clsid = format!("CLSID\\{}", clsid_str);

        let wide_inproc: Vec<u16> = key_inproc.encode_utf16().chain(std::iter::once(0)).collect();
        let wide_clsid: Vec<u16> = key_clsid.encode_utf16().chain(std::iter::once(0)).collect();

        let _ = RegDeleteKeyW(HKEY_CLASSES_ROOT, PCWSTR(wide_inproc.as_ptr()));
        let _ = RegDeleteKeyW(HKEY_CLASSES_ROOT, PCWSTR(wide_clsid.as_ptr()));

        Ok(())
    }
}

#[cfg(windows)]
pub use win32_register::*;
