#include "db_init.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static sqlite3 *db = NULL;

int db_init(const char *db_path)
{
    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[DB ERROR] Cannot open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    // Enable WAL mode for better concurrency
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", 0, 0, 0);
    sqlite3_exec(db, "PRAGMA foreign_keys=ON;", 0, 0, 0);

    const char *schema =
        "CREATE TABLE IF NOT EXISTS users ("
        "  id TEXT PRIMARY KEY,"
        "  name TEXT NOT NULL,"
        "  email TEXT,"
        "  school_id TEXT,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"

        "CREATE TABLE IF NOT EXISTS study_plans ("
        "  id TEXT PRIMARY KEY,"
        "  user_id TEXT NOT NULL,"
        "  subjects_json TEXT NOT NULL,"
        "  days_remaining INTEGER NOT NULL,"
        "  hours_per_day INTEGER NOT NULL,"
        "  plan_json TEXT NOT NULL,"
        "  strategy TEXT,"
        "  estimated_improvement TEXT,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  FOREIGN KEY (user_id) REFERENCES users(id)"
        ");"

        "CREATE TABLE IF NOT EXISTS plan_days ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  plan_id TEXT NOT NULL,"
        "  day_number INTEGER NOT NULL,"
        "  plan_date DATE NOT NULL,"
        "  focus TEXT,"
        "  sessions_json TEXT,"
        "  total_hours REAL,"
        "  daily_tip TEXT,"
        "  completed INTEGER DEFAULT 0,"
        "  FOREIGN KEY (plan_id) REFERENCES study_plans(id)"
        ");"

        "CREATE INDEX IF NOT EXISTS idx_plans_user ON study_plans(user_id);"
        "CREATE INDEX IF NOT EXISTS idx_days_plan ON plan_days(plan_id);";

    char *err_msg = NULL;
    rc = sqlite3_exec(db, schema, 0, 0, &err_msg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[DB ERROR] Schema creation failed: %s\n", err_msg);
        sqlite3_free(err_msg);
        return 1;
    }

    printf("[DB] Tables created successfully\n");
    return 0;
}

sqlite3 *db_get_connection()
{
    return db;
}

void db_close()
{
    if (db)
    {
        sqlite3_close(db);
        db = NULL;
    }
    printf("[DB] Connection closed\n");
}

int db_ensure_user(const char *user_id, const char *name, const char *email)
{
    const char *sql = "INSERT OR IGNORE INTO users (id, name, email) VALUES (?, ?, ?);";
    sqlite3_stmt *stmt;

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return 1;

    sqlite3_bind_text(stmt, 1, user_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, email, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE || rc == SQLITE_CONSTRAINT) ? 0 : 1;
}

int db_save_plan(const char *plan_id, const char *user_id,
                 const char *subjects_json, int days_remaining,
                 int hours_per_day, const char *plan_json,
                 const char *strategy, const char *estimated_improvement)
{

    const char *sql =
        "INSERT INTO study_plans (id, user_id, subjects_json, days_remaining, "
        "hours_per_day, plan_json, strategy, estimated_improvement) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[DB ERROR] Prepare: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    sqlite3_bind_text(stmt, 1, plan_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, user_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, subjects_json, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, days_remaining);
    sqlite3_bind_int(stmt, 5, hours_per_day);
    sqlite3_bind_text(stmt, 6, plan_json, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, strategy ? strategy : "", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, estimated_improvement ? estimated_improvement : "", -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[DB ERROR] Insert plan: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    return 0;
}

int db_save_plan_day(const char *plan_id, int day_number,
                     const char *plan_date, const char *focus,
                     const char *sessions_json, float total_hours,
                     const char *daily_tip)
{

    const char *sql =
        "INSERT INTO plan_days (plan_id, day_number, plan_date, focus, "
        "sessions_json, total_hours, daily_tip) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    sqlite3_bind_text(stmt, 1, plan_id, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, day_number);
    sqlite3_bind_text(stmt, 3, plan_date, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, focus ? focus : "", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, sessions_json, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 6, total_hours);
    sqlite3_bind_text(stmt, 7, daily_tip ? daily_tip : "", -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : 1;
}

char *db_get_plan(const char *plan_id)
{
    const char *sql =
        "SELECT id, user_id, subjects_json, days_remaining, hours_per_day, "
        "plan_json, strategy, estimated_improvement, created_at "
        "FROM study_plans WHERE id = ?;";

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, plan_id, -1, SQLITE_STATIC);

    char *json = NULL;

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        json = malloc(8192);
        snprintf(json, 8192,
                 "{"
                 "\"id\":\"%s\","
                 "\"user_id\":\"%s\","
                 "\"subjects\":%s,"
                 "\"days_remaining\":%d,"
                 "\"hours_per_day\":%d,"
                 "\"plan\":%s,"
                 "\"strategy\":\"%s\","
                 "\"estimated_improvement\":\"%s\","
                 "\"created_at\":\"%s\""
                 "}",
                 (const char *)sqlite3_column_text(stmt, 0),
                 (const char *)sqlite3_column_text(stmt, 1),
                 (const char *)sqlite3_column_text(stmt, 2),
                 sqlite3_column_int(stmt, 3),
                 sqlite3_column_int(stmt, 4),
                 (const char *)sqlite3_column_text(stmt, 5),
                 sqlite3_column_text(stmt, 6) ? (const char *)sqlite3_column_text(stmt, 6) : "",
                 sqlite3_column_text(stmt, 7) ? (const char *)sqlite3_column_text(stmt, 7) : "",
                 (const char *)sqlite3_column_text(stmt, 8));
    }

    sqlite3_finalize(stmt);
    return json;
}

char *db_get_plan_days(const char *plan_id)
{
    const char *sql =
        "SELECT day_number, plan_date, focus, sessions_json, total_hours, daily_tip, completed "
        "FROM plan_days WHERE plan_id = ? ORDER BY day_number;";

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, plan_id, -1, SQLITE_STATIC);

    char *json = malloc(8192);
    strcpy(json, "[");
    int first = 1;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (!first)
            strcat(json, ",");
        first = 0;

        char day_json[2048];
        snprintf(day_json, sizeof(day_json),
                 "{"
                 "\"day\":%d,"
                 "\"date\":\"%s\","
                 "\"focus\":\"%s\","
                 "\"sessions\":%s,"
                 "\"total_hours\":%.1f,"
                 "\"daily_tip\":\"%s\","
                 "\"completed\":%d"
                 "}",
                 sqlite3_column_int(stmt, 0),
                 (const char *)sqlite3_column_text(stmt, 1),
                 sqlite3_column_text(stmt, 2) ? (const char *)sqlite3_column_text(stmt, 2) : "",
                 (const char *)sqlite3_column_text(stmt, 3),
                 sqlite3_column_double(stmt, 4),
                 sqlite3_column_text(stmt, 5) ? (const char *)sqlite3_column_text(stmt, 5) : "",
                 sqlite3_column_int(stmt, 6));
        strcat(json, day_json);
    }
    strcat(json, "]");

    sqlite3_finalize(stmt);
    return json;
}

char *db_get_user_plans(const char *user_id)
{
    const char *sql =
        "SELECT id, subjects_json, days_remaining, hours_per_day, "
        "strategy, estimated_improvement, created_at "
        "FROM study_plans WHERE user_id = ? ORDER BY created_at DESC LIMIT 10;";

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, user_id, -1, SQLITE_STATIC);

    char *json = malloc(4096);
    strcpy(json, "[");
    int first = 1;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (!first)
            strcat(json, ",");
        first = 0;

        char plan_json[1024];
        snprintf(plan_json, sizeof(plan_json),
                 "{\"id\":\"%s\",\"subjects\":%s,\"days\":%d,\"hours_per_day\":%d,"
                 "\"strategy\":\"%s\",\"improvement\":\"%s\",\"created\":\"%s\"}",
                 (const char *)sqlite3_column_text(stmt, 0),
                 (const char *)sqlite3_column_text(stmt, 1),
                 sqlite3_column_int(stmt, 2),
                 sqlite3_column_int(stmt, 3),
                 sqlite3_column_text(stmt, 4) ? (const char *)sqlite3_column_text(stmt, 4) : "",
                 sqlite3_column_text(stmt, 5) ? (const char *)sqlite3_column_text(stmt, 5) : "",
                 (const char *)sqlite3_column_text(stmt, 6));
        strcat(json, plan_json);
    }
    strcat(json, "]");

    sqlite3_finalize(stmt);
    return json;
}
