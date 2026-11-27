// build.rs for consistent-hash Rust bindings

fn main() {
    // Tell cargo to invalidate the built crate whenever the wrapper changes
    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=src/ffi.rs");
    println!("cargo:rerun-if-changed=include/consistent_hash/consistent_hash.hpp");
    println!("cargo:rerun-if-changed=include/consistent_hash/ring.hpp");
    println!("cargo:rerun-if-changed=include/consistent_hash/jump.hpp");
    println!("cargo:rerun-if-changed=include/consistent_hash/maglev.hpp");
    println!("cargo:rerun-if-changed=include/consistent_hash/rendezvous.hpp");
    println!("cargo:rerun-if-changed=include/consistent_hash/multiprobe.hpp");
    println!("cargo:rerun-if-changed=include/consistent_hash/bounded_loads.hpp");
    println!("cargo:rerun-if-changed=include/consistent_hash/ketama_anchor.hpp");
    println!("cargo:rerun-if-changed=include/consistent_hash/lockfree.hpp");
    println!("cargo:rerun-if-changed=include/consistent_hash/health.hpp");
    println!("cargo:rerun-if-changed=include/consistent_hash/simd_hash.hpp");
    println!("cargo:rerun-if-changed=include/consistent_hash/observability.hpp");

    // Build the C++ library
    let mut build = cxx_build::bridge("src/lib.rs");
    build
        .file("src/ffi.cpp")
        .flag_if_supported("-std=c++20")
        .flag_if_supported("-Wall")
        .flag_if_supported("-Wextra")
        .flag_if_supported("-Wpedantic")
        .include("../../include")
        .compile("consistent_hash_bindings");

    // Link the C++ library
    println!("cargo:rustc-link-search=native=../../build");
    println!("cargo:rustc-link-lib=static=consistent_hash");
    println!("cargo:rustc-link-lib=dylib=stdc++");
}