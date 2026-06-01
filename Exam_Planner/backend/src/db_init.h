#ifndef DB_INIT_H
#define DB_INIT_H

#include <sqlite3.h>

// Initialize database and create tables
int db_init(const char *db_path);

// Get database connection
sqlite3 *db_get_connection(void);

// Close database
void db_close(void);

// Ensure a user exists
int db_ensure_user(const char *user_id, const char *name, const char *email);

// Save a study plan
int db_save_plan(const char *plan_id, const char *user_id,
                 const char *subjects_json, int days_remaining,
                 int hours_per_day, const char *plan_json,
                 const char *strategy, const char *estimated_improvement);

// Save a single plan day
int db_save_plan_day(const char *plan_id, int day_number,
                     const char *plan_date, const char *focus,
                     const char *sessions_json, float total_hours,
                     const char *daily_tip);

// Get all plans for a user
char *db_get_user_plans(const char *user_id);

// Get a specific plan
char *db_get_plan(const char *plan_id);

// Get days for a plan
char *db_get_plan_days(const char *plan_id);

#endif
