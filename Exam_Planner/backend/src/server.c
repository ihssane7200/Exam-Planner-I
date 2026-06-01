#include "server.h"
#include "db_init.h"
#include "json_parser.h"
#include "ai_client.h"
#include <cjson/cJSON.h>
#include <microhttpd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h> // for pause()

// ============================================
// Helper: Send JSON response with CORS
// ============================================
static enum MHD_Result send_json(struct MHD_Connection *connection,
                                 const char *json, int status_code)
{
    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(json), (void *)json, MHD_RESPMEM_MUST_COPY);

    MHD_add_response_header(response, "Content-Type", "application/json; charset=utf-8");
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type");

    int ret = MHD_queue_response(connection, status_code, response);
    MHD_destroy_response(response);
    return ret;
}

// ============================================
// Handler: POST /api/generate-plan
// ============================================
static enum MHD_Result handle_generate_plan(struct MHD_Connection *connection,
                                            const char *upload_data, size_t upload_size)
{
    if (upload_size == 0)
    {
        return send_json(connection, "{\"error\":\"Empty request\"}", MHD_HTTP_BAD_REQUEST);
    }

    printf("[SERVER] Generate plan request (%zu bytes)\n", upload_size);

    // Parse the request
    plan_request_t *request = parse_plan_request(upload_data);
    if (!request)
    {
        return send_json(connection, "{\"error\":\"Invalid JSON\"}", MHD_HTTP_BAD_REQUEST);
    }

    printf("[SERVER] User: %s, Days: %d, Hours: %d\n",
           request->user_name, request->days_remaining, request->hours_per_day);

    // Ensure user exists
    db_ensure_user(request->user_id, request->user_name, "");

    // Generate plan via AI
    study_plan_t *plan = ai_generate_plan(request);

    if (!plan)
    {
        free_plan_request(request);
        return send_json(connection,
                         "{\"error\":\"AI generation failed. Check API key.\"}",
                         MHD_HTTP_INTERNAL_SERVER_ERROR);
    }

    printf("[SERVER] Plan generated: %d days\n", plan->day_count);

    // Generate plan ID and save
    char plan_id[64];
    generate_plan_id(plan_id, sizeof(plan_id));

    char *plan_json = plan_to_json(plan);
    db_save_plan(plan_id, request->user_id, request->subjects_json,
                 request->days_remaining, request->hours_per_day,
                 plan_json, plan->overall_strategy, plan->estimated_improvement);

    // Save each day
    for (int i = 0; i < plan->day_count; i++)
    {
        cJSON *sess_array = cJSON_CreateArray();
        for (int j = 0; j < plan->days[i].session_count; j++)
        {
            cJSON *s = cJSON_CreateObject();
            cJSON_AddStringToObject(s, "subject", plan->days[i].sessions[j].subject);
            cJSON_AddStringToObject(s, "chapter", plan->days[i].sessions[j].chapter);
            cJSON_AddNumberToObject(s, "duration", plan->days[i].sessions[j].duration_minutes);
            cJSON_AddStringToObject(s, "priority", plan->days[i].sessions[j].priority);
            cJSON_AddStringToObject(s, "type", plan->days[i].sessions[j].type);
            cJSON_AddItemToArray(sess_array, s);
        }
        char *sessions_str = cJSON_PrintUnformatted(sess_array);
        cJSON_Delete(sess_array);

        db_save_plan_day(plan_id, plan->days[i].day_number,
                         plan->days[i].date, plan->days[i].focus,
                         sessions_str, plan->days[i].total_hours,
                         plan->days[i].daily_tip);
        free(sessions_str);
    }

    printf("[SERVER] Plan saved to database: %s\n", plan_id);

    // Build response
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "plan_id", plan_id);
    cJSON_AddStringToObject(response, "status", "success");
    cJSON_AddStringToObject(response, "strategy", plan->overall_strategy);
    cJSON_AddStringToObject(response, "estimated_improvement", plan->estimated_improvement);

    cJSON *parsed_plan = cJSON_Parse(plan_json);
    if (parsed_plan)
    {
        cJSON *days = cJSON_GetObjectItem(parsed_plan, "plan");
        if (days)
            cJSON_AddItemToObject(response, "plan", cJSON_Duplicate(days, 1));
        cJSON_Delete(parsed_plan);
    }

    char *response_str = cJSON_PrintUnformatted(response);

    // Cleanup
    free_plan_request(request);
    free_study_plan(plan);
    free(plan_json);
    cJSON_Delete(response);

    enum MHD_Result res = send_json(connection, response_str, MHD_HTTP_OK);
    free(response_str);
    return res;
}

// ============================================
// Handler: GET /api/plan/{id}
// ============================================
static enum MHD_Result handle_get_plan(struct MHD_Connection *connection, const char *plan_id)
{
    printf("[SERVER] Get plan request: %s\n", plan_id);

    char *plan = db_get_plan(plan_id);
    if (!plan)
    {
        return send_json(connection, "{\"error\":\"Plan not found\"}", MHD_HTTP_NOT_FOUND);
    }

    enum MHD_Result ret = send_json(connection, plan, MHD_HTTP_OK);
    free(plan);
    return ret;
}

// ============================================
// Handler: GET /api/plan/{id}/days
// ============================================
static enum MHD_Result handle_get_plan_days(struct MHD_Connection *connection, const char *plan_id)
{
    printf("[SERVER] Get plan days request: %s\n", plan_id);

    char *days = db_get_plan_days(plan_id);
    if (!days)
    {
        return send_json(connection, "[]", MHD_HTTP_OK);
    }

    enum MHD_Result ret = send_json(connection, days, MHD_HTTP_OK);
    free(days);
    return ret;
}

// ============================================
// Handler: GET /api/history
// ============================================
static enum MHD_Result handle_get_history(struct MHD_Connection *connection)
{
    const char *user_id = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "user_id");

    if (!user_id)
    {
        return send_json(connection, "{\"error\":\"Missing user_id parameter\"}", MHD_HTTP_BAD_REQUEST);
    }

    printf("[SERVER] Get history for user: %s\n", user_id);

    char *plans = db_get_user_plans(user_id);
    if (!plans)
    {
        return send_json(connection, "[]", MHD_HTTP_OK);
    }

    enum MHD_Result ret = send_json(connection, plans, MHD_HTTP_OK);
    free(plans);
    return ret;
}

// ============================================
// MAIN REQUEST HANDLER (The Router)
// ============================================
struct request_context
{
    char *data;
    size_t size;
};

static enum MHD_Result handle_request(void *cls,
                                      struct MHD_Connection *connection,
                                      const char *url, const char *method,
                                      const char *version, const char *upload_data,
                                      size_t *upload_data_size, void **ptr)
{

    // Suppress unused parameter warnings
    (void)cls;
    (void)version;

    // Handle POST data chunks
    if (strcmp(method, "POST") == 0)
    {
        if (*ptr == NULL)
        {
            // First call: allocate context
            struct request_context *ctx = calloc(1, sizeof(struct request_context));
            if (!ctx)
                return MHD_NO;
            *ptr = ctx;
            return MHD_YES;
        }

        struct request_context *ctx = *ptr;

        if (*upload_data_size != 0)
        {
            // Second+ call: append data
            char *new_data = realloc(ctx->data, ctx->size + *upload_data_size + 1);
            if (!new_data)
                return MHD_NO;

            ctx->data = new_data;
            memcpy(ctx->data + ctx->size, upload_data, *upload_data_size);
            ctx->size += *upload_data_size;
            ctx->data[ctx->size] = '\0';

            *upload_data_size = 0;
            return MHD_YES;
        }

        // Final call: execute route
        if (strcmp(url, "/api/generate-plan") == 0)
        {
            printf("[SERVER] POST %s\n", url);
            enum MHD_Result ret = handle_generate_plan(connection, ctx->data, ctx->size);
            free(ctx->data);
            free(ctx);
            *ptr = NULL;
            return ret;
        }
    }

    printf("[SERVER] %s %s\n", method, url);

    // ========================================
    // ROUTE: OPTIONS (CORS preflight)
    // ========================================
    if (strcmp(method, "OPTIONS") == 0)
    {
        return send_json(connection, "", MHD_HTTP_OK);
    }

    // ========================================
    // ROUTE: GET /api/health
    // ========================================
    if (strcmp(method, "GET") == 0 && strcmp(url, "/api/health") == 0)
    {
        return send_json(connection,
                         "{\"status\":\"ok\",\"service\":\"Exam Planner AI\",\"version\":\"1.0\"}",
                         MHD_HTTP_OK);
    }

    // ========================================
    // ROUTE: GET /api/plan/{id}/days
    // ========================================
    if (strcmp(method, "GET") == 0 && strncmp(url, "/api/plan/", 10) == 0)
    {
        const char *plan_id = url + 10; // Skip "/api/plan/"

        // Check if requesting days
        const char *slash = strchr(plan_id, '/');
        if (slash && strcmp(slash, "/days") == 0)
        {
            // Extract plan_id without "/days"
            char pid[64];
            size_t len = slash - plan_id;
            if (len > 63)
                len = 63;
            strncpy(pid, plan_id, len);
            pid[len] = '\0';
            return handle_get_plan_days(connection, pid);
        }

        return handle_get_plan(connection, plan_id);
    }

    // ========================================
    // ROUTE: GET /api/history?user_id=xxx
    // ========================================
    if (strcmp(method, "GET") == 0 && strcmp(url, "/api/history") == 0)
    {
        return handle_get_history(connection);
    }

    // ========================================
    // 404 - Route not found
    // ========================================
    printf("[SERVER] 404: %s %s\n", method, url);
    return send_json(connection, "{\"error\":\"Not found\"}", MHD_HTTP_NOT_FOUND);
}

// ============================================
// Start the server
// ============================================
void server_start(int port)
{
    struct MHD_Daemon *daemon = MHD_start_daemon(
        MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD,
        port, NULL, NULL,
        &handle_request, NULL,
        MHD_OPTION_END);

    if (!daemon)
    {
        fprintf(stderr, "[SERVER ERROR] Failed to start on port %d\n", port);
        return;
    }

    printf("\n[SERVER] Running on http://localhost:%d\n", port);
    printf("[SERVER] Endpoints:\n");
    printf("  GET  /api/health\n");
    printf("  POST /api/generate-plan\n");
    printf("  GET  /api/plan/{id}\n");
    printf("  GET  /api/plan/{id}/days\n");
    printf("  GET  /api/history?user_id=xxx\n");
    printf("[SERVER] Press Enter to stop...\n\n");

    printf("[SERVER] Running. Press Ctrl+C to stop.\n");
    fflush(stdout);
    // Wait indefinitely using pause()
    pause();

    MHD_stop_daemon(daemon);
    printf("[SERVER] Stopped\n");
}
