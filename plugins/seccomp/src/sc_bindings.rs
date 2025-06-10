use libc::{c_int, c_uint, c_void};

// Opaque type for seccomp filter context
pub type ScmpFilterCtx = *mut c_void; // Renamed to UpperCamelCase

// Seccomp action constants from seccomp.h
// #define SCMP_ACT_KILL  0x00000000U /* Kill the task */
// #define SCMP_ACT_ALLOW 0x7fff0000U /* Allow the syscall */
pub const SCMP_ACT_KILL: c_uint = 0x00000000;
pub const SCMP_ACT_ALLOW: c_uint = 0x7fff0000;

// Syscall numbers are imported from libc in main.rs directly

// Function signatures from seccomp.h
#[link(name = "seccomp")]
extern "C" {
    pub fn seccomp_init(def_action: c_uint) -> ScmpFilterCtx;
    pub fn seccomp_rule_add(ctx: ScmpFilterCtx, action: c_uint, syscall: c_int, arg_cnt: c_uint) -> c_int;
    // For simplicity, we are using the version of seccomp_rule_add with 0 varargs.
    // The full signature can be:
    // pub fn seccomp_rule_add(ctx: ScmpFilterCtx, action: u32, syscall: i32, arg_cnt: u32, ...) -> i32;
    pub fn seccomp_load(ctx: ScmpFilterCtx) -> c_int;
    // We are omitting seccomp_release for now as it caused issues with older libseccomp versions
    // pub fn seccomp_release(ctx: ScmpFilterCtx);
}
