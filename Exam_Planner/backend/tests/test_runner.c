#include <stdio.h>
#include <stdlib.h>
#include "../src/db_init.h"
#include "../src/json_parser.h"

int test_database(void)
{
    printf("Database Tests:\n");

    printf("  Init... ");
    if (db_init("test.db") != 0)
    {
        printf("FAIL\n");
        return 1;
    }
    printf("OK\n");

    printf("  Create user... ");
    if (db_ensure_user("test_user", "Test", "") != 0)
    {
        printf("FAIL\n");
        return 1;
    }
    printf("OK\n");

    printf("  Save plan... ");
    if (db_save_plan("p1", "test_user", "[]", 7, 4, "[]", "s", "e") != 0)
    {
        printf("FAIL\n");
        return 1;
    }
    printf("OK\n");

    printf("  Get plan... ");
    char *plan = db_get_plan("p1");
    if (!plan)
    {
        printf("FAIL\n");
        return 1;
    }
    free(plan);
    printf("OK\n");

    printf("  Save day... ");
    if (db_save_plan_day("p1", 1, "2026-06-01", "F", "[]", 4.0, "T") != 0)
    {
        printf("FAIL\n");
        return 1;
    }
    printf("OK\n");

    printf("  Get days... ");
    char *days = db_get_plan_days("p1");
    if (!days)
    {
        printf("FAIL\n");
        return 1;
    }
    free(days);
    printf("OK\n");

    printf("  Get user plans... ");
    char *plans = db_get_user_plans("test_user");
    if (!plans)
    {
        printf("FAIL\n");
        return 1;
    }
    free(plans);
    printf("OK\n");

    db_close();
    return 0;
}

int test_json(void)
{
    printf("JSON Tests:\n");

    printf("  Parse request... ");
    const char *req_json = "{\"user_id\":\"u1\",\"user_name\":\"T\","
                           "\"subjects\":[],\"days_remaining\":5,\"hours_per_day\":3}";
    plan_request_t *req = parse_plan_request(req_json);
    if (!req || req->days_remaining != 5)
    {
        printf("FAIL\n");
        return 1;
    }
    free_plan_request(req);
    printf("OK\n");

    printf("  Parse AI response... ");
    const char *ai_json = "{\"plan\":[{\"day\":1,\"date\":\"2026-06-01\","
                          "\"focus\":\"Test\",\"sessions\":[],\"total_hours\":4.0,"
                          "\"daily_tip\":\"Tip\"}],\"overall_strategy\":\"S\","
                          "\"estimated_improvement\":\"20%%\"}";
    study_plan_t *plan = parse_ai_plan(ai_json);
    if (!plan || plan->day_count != 1)
    {
        printf("FAIL\n");
        return 1;
    }
    free_study_plan(plan);
    printf("OK\n");

    return 0;
}

int main(void)
{
    printf("=========================================\n");
    printf("  Exam Planner AI - Unit Tests\n");
    printf("=========================================\n\n");

    int failed = 0;
    failed += test_database();
    printf("\n");
    failed += test_json();

    printf("\n=========================================\n");
    if (failed == 0)
        printf("  ALL TESTS PASSED\n");
    else
        printf("  %d SUITE(S) FAILED\n", failed);
    printf("=========================================\n");

    return failed;
}
