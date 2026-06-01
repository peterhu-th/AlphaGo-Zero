mod db;
mod server;
mod cxx_bridge;

use dotenvy::dotenv;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    // 加载 .env 环境变量
    dotenv().ok();

    println!("正在初始化数据库连接池和迁移...");
    let pool = db::init_db().await?;
    println!("数据库就绪。");

    println!("构建 Web 服务路由...");
    let app = server::create_router(pool);

    let listener = tokio::net::TcpListener::bind("0.0.0.0:8080").await?;
    println!("Rust 后端服务器已启动，监听 0.0.0.0:8080");
    axum::serve(listener, app).await?;

    Ok(())
}
