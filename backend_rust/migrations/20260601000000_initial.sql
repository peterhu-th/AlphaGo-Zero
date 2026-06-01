CREATE TABLE users (
    id UUID PRIMARY KEY,
    username VARCHAR(50) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    elo_go INTEGER NOT NULL DEFAULT 1200,
    elo_gomoku INTEGER NOT NULL DEFAULT 1200,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE matches (
    id UUID PRIMARY KEY,
    black_id UUID REFERENCES users(id),
    white_id UUID REFERENCES users(id),
    mode VARCHAR(20) NOT NULL,
    board_size INTEGER NOT NULL,
    result VARCHAR(50),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    finished_at TIMESTAMPTZ
);

CREATE TABLE match_records (
    match_id UUID PRIMARY KEY REFERENCES matches(id),
    sgf_data TEXT NOT NULL
);
