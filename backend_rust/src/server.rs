use axum::{
    routing::get,
    Router,
};
use sqlx::PgPool;

pub fn create_router(pool: PgPool) -> Router {
    Router::new()
        .route("/health", get(|| async { "OK" }))
        .with_state(pool)
}
