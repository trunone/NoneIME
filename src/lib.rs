#![allow(non_snake_case)]

// NoneIME root library exports and COM server implementation

pub mod config;
pub mod dictionary;
pub mod key_handler;
pub mod composition;
pub mod imekit_engine;

#[cfg(windows)]
pub mod candidate_window;
#[cfg(windows)]
pub mod register;
#[cfg(windows)]
pub mod tsf;

#[cfg(windows)]
use std::sync::atomic::{AtomicIsize, Ordering};
#[cfg(windows)]
use windows_core::{BOOL, GUID, HRESULT};
#[cfg(windows)]
use windows::Win32::Foundation::{CLASS_E_CLASSNOTAVAILABLE, HMODULE, HINSTANCE, S_FALSE, S_OK};
#[cfg(windows)]
use windows::Win32::System::Com::IClassFactory;
#[cfg(windows)]
use windows::Win32::System::SystemServices::{DLL_PROCESS_ATTACH, DLL_PROCESS_DETACH};
#[cfg(windows)]
use windows_core::Interface;

#[cfg(windows)]
static DLL_INSTANCE: AtomicIsize = AtomicIsize::new(0);

#[cfg(windows)]
#[no_mangle]
pub unsafe extern "system" fn DllMain(
    hinst: HINSTANCE,
    reason: u32,
    _reserved: *mut std::ffi::c_void,
) -> BOOL {
    match reason {
        DLL_PROCESS_ATTACH => {
            DLL_INSTANCE.store(hinst.0 as isize, Ordering::SeqCst);
            // Register candidate window class
            let _ = candidate_window::CandidateWindow::register_class(hinst);

            // Load configuration from NoneIME.ini
            let mut path_buf = [0u16; 260];
            let len = windows::Win32::System::LibraryLoader::GetModuleFileNameW(
                Some(HMODULE(hinst.0)),
                &mut path_buf,
            );
            if len > 0 {
                let dll_path = String::from_utf16_lossy(&path_buf[..len as usize]);
                config::load_dll_directory_config(std::path::Path::new(&dll_path));
            }
            BOOL(1)
        }
        DLL_PROCESS_DETACH => {
            candidate_window::CandidateWindow::hide();
            BOOL(1)
        }
        _ => BOOL(1),
    }
}

#[cfg(windows)]
#[no_mangle]
pub unsafe extern "system" fn DllGetClassObject(
    rclsid: *const GUID,
    riid: *const GUID,
    ppv: *mut *mut std::ffi::c_void,
) -> HRESULT {
    if rclsid.is_null() || ppv.is_null() {
        return windows::Win32::Foundation::E_INVALIDARG;
    }

    if *rclsid != register::CLSID_NONE_IME {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    let factory: IClassFactory = tsf::ClassFactory.into();
    factory.query(riid, ppv)
}

#[cfg(windows)]
#[no_mangle]
pub unsafe extern "system" fn DllCanUnloadNow() -> HRESULT {
    if tsf::DLL_REF_COUNT.load(Ordering::SeqCst) == 0 {
        S_OK
    } else {
        S_FALSE
    }
}

#[cfg(windows)]
#[no_mangle]
pub unsafe extern "system" fn DllRegisterServer() -> HRESULT {
    let inst = HINSTANCE(DLL_INSTANCE.load(Ordering::SeqCst) as _);
    let mut path_buf = [0u16; 260];
    let len = windows::Win32::System::LibraryLoader::GetModuleFileNameW(
        Some(HMODULE(inst.0)),
        &mut path_buf,
    );
    if len == 0 {
        return windows::Win32::Foundation::E_FAIL;
    }

    let dll_path_str = String::from_utf16_lossy(&path_buf[..len as usize]);
    let dll_path = std::path::Path::new(&dll_path_str);

    match register::register_server(dll_path) {
        Ok(_) => S_OK,
        Err(e) => e.code(),
    }
}

#[cfg(windows)]
#[no_mangle]
pub unsafe extern "system" fn DllUnregisterServer() -> HRESULT {
    match register::unregister_server() {
        Ok(_) => S_OK,
        Err(e) => e.code(),
    }
}
