#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <stddef.h>

// Structure for the study plan request from frontend
typedef struct
{
    char user_id[64];
    char user_name[64];
    char subjects_json[4096];
    int days_remaining;
    int hours_per_day;
    char preferences[512];
} plan_request_t;

// Structure for a session within a day
typedef struct
{
    char subject[128];
    char chapter[256];
    int duration_minutes;
    char priority[32];
    char type[64];
    char notes[256];
} session_t;

// Structure for a single day in the plan
typedef struct
{
    int day_number;
    char date[32];
    char focus[256];
    session_t *sessions;
    int session_count;
    float total_hours;
    char daily_tip[512];
} plan_day_t;

// Structure for the complete AI-generated plan
typedef struct
{
    plan_day_t *days;
    int day_count;
    char overall_strategy[1024];
    int revision_days[10];
    int revision_count;
    int mock_exam_days[5];
    int mock_count;
    char estimated_improvement[128];
} study_plan_t;

// Parse incoming request from frontend
plan_request_t *parse_plan_request(const char *json_string);
void free_plan_request(plan_request_t *request);

// Parse AI response into structured plan
study_plan_t *parse_ai_plan(const char *json_string);
void free_study_plan(study_plan_t *plan);

// Convert plan to JSON string for frontend
char *plan_to_json(study_plan_t *plan);

// Generate a unique plan ID
void generate_plan_id(char *buffer, size_t size);

#endif
