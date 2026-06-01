-- Reference only. Tables are created automatically by db_init.c

CREATE TABLE IF NOT EXISTS users (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    email TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS study_plans (
    id TEXT PRIMARY KEY,
    user_id TEXT NOT NULL,
    subjects_json TEXT NOT NULL,
    days_remaining INTEGER NOT NULL,
    hours_per_day INTEGER NOT NULL,
    plan_json TEXT NOT NULL,
    strategy TEXT,
    estimated_improvement TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(id)
);

CREATE TABLE IF NOT EXISTS plan_days (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    plan_id TEXT NOT NULL,
    day_number INTEGER NOT NULL,
    plan_date TEXT NOT NULL,
    focus TEXT,
    sessions_json TEXT,
    total_hours REAL,
    daily_tip TEXT,
    completed INTEGER DEFAULT 0,
    FOREIGN KEY (plan_id) REFERENCES study_plans(id)
);
