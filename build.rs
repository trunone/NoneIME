use std::env;
use std::fs;
use std::path::Path;
use std::process::Command;

fn main() {
    println!("cargo:rerun-if-changed=NoneIME.rc");
    println!("cargo:rerun-if-changed=NoneIME.def");
    println!("cargo:rerun-if-changed=include/resource.h");
    println!("cargo:rerun-if-changed=image/NoneIME.ico");
    println!("cargo:rerun-if-changed=image/ImeModeOn.ico");
    println!("cargo:rerun-if-changed=image/ImeModeOff.ico");
    println!("cargo:rerun-if-changed=Dictionary/boshiamy.bin");

    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
    if target_os != "windows" {
        return;
    }

    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let out_dir = env::var("OUT_DIR").unwrap();
    let rc_path = Path::new(&manifest_dir).join("NoneIME.rc");

    let target_env = env::var("CARGO_CFG_TARGET_ENV").unwrap_or_default();
    let target_arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap_or_default();

    // Pass .def file with appropriate linker flag
    let def_path = Path::new(&manifest_dir).join("NoneIME.def");
    if target_env == "msvc" {
        println!("cargo:rustc-cdylib-link-arg=/DEF:{}", def_path.display());
    } else {
        println!("cargo:rustc-cdylib-link-arg={}", def_path.display());
    }

    // Prepare a normalized .rc file with forward slashes for cross-compilers like windres
    let rc_content = fs::read_to_string(&rc_path).unwrap();
    let normalized_rc = rc_content.replace('\\', "/");
    let normalized_rc_path = Path::new(&out_dir).join("NoneIME_normalized.rc");
    fs::write(&normalized_rc_path, &normalized_rc).unwrap();

    if target_env == "gnu" {
        let windres_cmd = match target_arch.as_str() {
            "x86" => "i686-w64-mingw32-windres",
            _ => "x86_64-w64-mingw32-windres",
        };
        // Verify windres tool existence
        if Command::new(windres_cmd).arg("--version").output().is_ok() {
            let out_obj = Path::new(&out_dir).join("NoneIME_res.o");
            let status = Command::new(windres_cmd)
                .current_dir(&manifest_dir)
                .args([
                    "-i",
                    normalized_rc_path.to_str().unwrap(),
                    "-o",
                    out_obj.to_str().unwrap(),
                    "-I",
                    &manifest_dir,
                ])
                .status();

            if let Ok(st) = status {
                if st.success() {
                    println!("cargo:rustc-link-arg={}", out_obj.display());
                    return;
                }
            }
        }
    }

    // For MSVC (or when windres is not used), compile using embed-resource
    let _ = embed_resource::compile(&normalized_rc_path, &[&manifest_dir]);
}
