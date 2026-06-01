#include "json_parser.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ============================================
// Parse incoming request from frontend
// ============================================
plan_request_t *parse_plan_request(const char *json_string)
{
    cJSON *root = cJSON_Parse(json_string);
    if (!root)
    {
        fprintf(stderr, "[JSON ERROR] Failed to parse request\n");
        return NULL;
    }

    plan_request_t *req = calloc(1, sizeof(plan_request_t));
    if (!req)
    {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *item;

    item = cJSON_GetObjectItem(root, "user_id");
    // Ensure we handle both string and number for user_id to be safe
    if (item)
    {
        if (cJSON_IsString(item))
        {
            strncpy(req->user_id, item->valuestring, 63);
        }
        else if (cJSON_IsNumber(item))
        {
            snprintf(req->user_id, 64, "%d", item->valueint);
        }
    }

    item = cJSON_GetObjectItem(root, "user_name");
    if (item && cJSON_IsString(item))
        strncpy(req->user_name, item->valuestring, 63);

    item = cJSON_GetObjectItem(root, "subjects");
    if (item)
    {
        char *subjects_str = cJSON_PrintUnformatted(item);
        strncpy(req->subjects_json, subjects_str, 4095);
        free(subjects_str);
    }
    else
    {
        // Fallback to "subject" string
        item = cJSON_GetObjectItem(root, "subject");
        if (item && cJSON_IsString(item))
        {
            snprintf(req->subjects_json, 4096, "[\"%s\"]", item->valuestring);
        }
    }

    item = cJSON_GetObjectItem(root, "days_remaining"); // in test we used days_until_exam
    if (item)
    {
        req->days_remaining = item->valueint;
    }
    else
    {
        item = cJSON_GetObjectItem(root, "days_until_exam");
        if (item)
            req->days_remaining = item->valueint;
    }

    item = cJSON_GetObjectItem(root, "hours_per_day");
    if (item)
        req->hours_per_day = item->valueint;

    item = cJSON_GetObjectItem(root, "preferences");
    if (item && cJSON_IsString(item))
    {
        strncpy(req->preferences, item->valuestring, 511);
    }
    else
    {
        item = cJSON_GetObjectItem(root, "current_level");
        if (item && cJSON_IsString(item))
        {
            snprintf(req->preferences, 512, "Level: %s", item->valuestring);
        }
    }

    if (strlen(req->user_id) == 0)
    {
        strcpy(req->user_id, "default_user");
    }
    if (strlen(req->user_name) == 0)
    {
        strcpy(req->user_name, "Anonymous");
    }
    if (strlen(req->subjects_json) == 0)
    {
        strcpy(req->subjects_json, "[]");
    }

    cJSON_Delete(root);
    return req;
}

void free_plan_request(plan_request_t *request)
{
    free(request);
}

// ============================================
// Parse AI response into structured plan
// ============================================
study_plan_t *parse_ai_plan(const char *json_string)
{
    cJSON *root = cJSON_Parse(json_string);
    if (!root)
    {
        fprintf(stderr, "[JSON ERROR] Failed to parse AI response\n");
        return NULL;
    }

    study_plan_t *plan = calloc(1, sizeof(study_plan_t));
    if (!plan)
    {
        cJSON_Delete(root);
        return NULL;
    }

    // Parse overall strategy
    cJSON *strategy = cJSON_GetObjectItem(root, "overall_strategy");
    if (strategy && cJSON_IsString(strategy))
    {
        strncpy(plan->overall_strategy, strategy->valuestring, 1023);
    }

    // Parse estimated improvement
    cJSON *improvement = cJSON_GetObjectItem(root, "estimated_improvement");
    if (improvement && cJSON_IsString(improvement))
    {
        strncpy(plan->estimated_improvement, improvement->valuestring, 127);
    }

    // Parse plan array
    cJSON *plan_array = cJSON_GetObjectItem(root, "plan");
    if (plan_array && cJSON_IsArray(plan_array))
    {
        plan->day_count = cJSON_GetArraySize(plan_array);
        plan->days = calloc(plan->day_count, sizeof(plan_day_t));

        for (int i = 0; i < plan->day_count; i++)
        {
            cJSON *day_obj = cJSON_GetArrayItem(plan_array, i);
            plan_day_t *day = &plan->days[i];

            cJSON *num = cJSON_GetObjectItem(day_obj, "day");
            if (num)
                day->day_number = num->valueint;

            cJSON *date = cJSON_GetObjectItem(day_obj, "date");
            if (date && cJSON_IsString(date))
                strncpy(day->date, date->valuestring, 31);

            cJSON *focus = cJSON_GetObjectItem(day_obj, "focus");
            if (focus && cJSON_IsString(focus))
                strncpy(day->focus, focus->valuestring, 255);

            cJSON *hours = cJSON_GetObjectItem(day_obj, "total_hours");
            if (hours)
                day->total_hours = (float)hours->valuedouble;

            cJSON *tip = cJSON_GetObjectItem(day_obj, "daily_tip");
            if (tip && cJSON_IsString(tip))
                strncpy(day->daily_tip, tip->valuestring, 511);

            // Parse sessions
            cJSON *sessions = cJSON_GetObjectItem(day_obj, "sessions");
            if (sessions && cJSON_IsArray(sessions))
            {
                day->session_count = cJSON_GetArraySize(sessions);
                day->sessions = calloc(day->session_count, sizeof(session_t));

                for (int j = 0; j < day->session_count; j++)
                {
                    cJSON *sess = cJSON_GetArrayItem(sessions, j);
                    session_t *s = &day->sessions[j];

                    cJSON *subj = cJSON_GetObjectItem(sess, "subject");
                    if (subj && cJSON_IsString(subj))
                        strncpy(s->subject, subj->valuestring, 127);

                    cJSON *chap = cJSON_GetObjectItem(sess, "chapter");
                    if (chap && cJSON_IsString(chap))
                        strncpy(s->chapter, chap->valuestring, 255);

                    cJSON *dur = cJSON_GetObjectItem(sess, "duration_minutes");
                    if (dur)
                        s->duration_minutes = dur->valueint;

                    cJSON *pri = cJSON_GetObjectItem(sess, "priority");
                    if (pri && cJSON_IsString(pri))
                        strncpy(s->priority, pri->valuestring, 31);

                    cJSON *typ = cJSON_GetObjectItem(sess, "type");
                    if (typ && cJSON_IsString(typ))
                        strncpy(s->type, typ->valuestring, 63);

                    cJSON *not = cJSON_GetObjectItem(sess, "notes");
                    if (not && cJSON_IsString(not))
                        strncpy(s->notes, not->valuestring, 255);
                }
            }
        }
    }

    // Parse revision days
    cJSON *revision = cJSON_GetObjectItem(root, "revision_days");
    if (revision && cJSON_IsArray(revision))
    {
        plan->revision_count = cJSON_GetArraySize(revision);
        for (int i = 0; i < plan->revision_count && i < 10; i++)
        {
            plan->revision_days[i] = cJSON_GetArrayItem(revision, i)->valueint;
        }
    }

    cJSON_Delete(root);
    return plan;
}

void free_study_plan(study_plan_t *plan)
{
    if (!plan)
        return;
    if (plan->days)
    {
        for (int i = 0; i < plan->day_count; i++)
        {
            free(plan->days[i].sessions);
        }
        free(plan->days);
    }
    free(plan);
}

// ============================================
// Convert plan to JSON string
// ============================================
char *plan_to_json(study_plan_t *plan)
{
    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "strategy", plan->overall_strategy);
    cJSON_AddStringToObject(root, "estimated_improvement", plan->estimated_improvement);

    cJSON *days_array = cJSON_CreateArray();
    for (int i = 0; i < plan->day_count; i++)
    {
        cJSON *day_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(day_obj, "day", plan->days[i].day_number);
        cJSON_AddStringToObject(day_obj, "date", plan->days[i].date);
        cJSON_AddStringToObject(day_obj, "focus", plan->days[i].focus);
        cJSON_AddNumberToObject(day_obj, "total_hours", plan->days[i].total_hours);
        cJSON_AddStringToObject(day_obj, "daily_tip", plan->days[i].daily_tip);

        cJSON *sess_array = cJSON_CreateArray();
        for (int j = 0; j < plan->days[i].session_count; j++)
        {
            cJSON *sess = cJSON_CreateObject();
            cJSON_AddStringToObject(sess, "subject", plan->days[i].sessions[j].subject);
            cJSON_AddStringToObject(sess, "chapter", plan->days[i].sessions[j].chapter);
            cJSON_AddNumberToObject(sess, "duration", plan->days[i].sessions[j].duration_minutes);
            cJSON_AddStringToObject(sess, "priority", plan->days[i].sessions[j].priority);
            cJSON_AddStringToObject(sess, "type", plan->days[i].sessions[j].type);
            cJSON_AddItemToArray(sess_array, sess);
        }
        cJSON_AddItemToObject(day_obj, "sessions", sess_array);
        cJSON_AddItemToArray(days_array, day_obj);
    }
    cJSON_AddItemToObject(root, "plan", days_array);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

void generate_plan_id(char *buffer, size_t size)
{
    snprintf(buffer, size, "plan_%ld", (long)time(NULL));
}
