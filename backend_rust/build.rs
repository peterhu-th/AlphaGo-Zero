fn main() {
    cxx_build::bridge("src/cxx_bridge.rs")
        // 如果需要将 ../core 下的文件编译进静态库，可以像这样配置：
        // .file("../core/games/GoGame.cpp")
        // .include("../core")
        .compile("core_engine_rs");

    println!("cargo:rerun-if-changed=src/cxx_bridge.rs");
}
