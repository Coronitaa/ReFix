//! Safe Rust bindings for the ReFix Mods Sandboxed WebAssembly API.

pub type PeerHandle = u64;

#[link(wasm_import_module = "env")]
extern "C" {
    fn refix_log(level: i32, msg_ptr: *const u8, msg_len: u32);
    fn refix_get_time_ms() -> i64;
    fn refix_storage_read(key_ptr: *const u8, key_len: u32, out_ptr: *mut u8, max_len: u32) -> i32;
    fn refix_storage_write(key_ptr: *const u8, key_len: u32, val_ptr: *const u8, val_len: u32) -> i32;
}

pub fn log_info(msg: &str) {
    unsafe {
        refix_log(1, msg.as_ptr(), msg.len() as u32);
    }
}

pub fn log_warn(msg: &str) {
    unsafe {
        refix_log(2, msg.as_ptr(), msg.len() as u32);
    }
}

pub fn log_error(msg: &str) {
    unsafe {
        refix_log(3, msg.as_ptr(), msg.len() as u32);
    }
}

pub fn get_time_ms() -> u64 {
    unsafe { refix_get_time_ms() as u64 }
}

pub fn storage_get(key: &str) -> Option<Vec<u8>> {
    let mut buf = vec![0u8; 4096];
    let res = unsafe {
        refix_storage_read(key.as_ptr(), key.len() as u32, buf.as_mut_ptr(), buf.len() as u32)
    };
    if res >= 0 {
        buf.truncate(res as usize);
        Some(buf)
    } else {
        None
    }
}

pub fn storage_set(key: &str, data: &[u8]) -> bool {
    let res = unsafe {
        refix_storage_write(key.as_ptr(), key.len() as u32, data.as_ptr(), data.len() as u32)
    };
    res == 0
}
