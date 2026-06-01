#include "ai_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

static char api_key[256] = {0};
static char api_provider[32] = {0}; // "groq" or "mistral"

struct MemoryStruct
{
    char *memory;
    size_t size;
};

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr)
        return 0;
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

void ai_set_api_key(const char *key)
{
    if (key)
        strncpy(api_key, key, 255);
}

void ai_set_provider(const char *provider)
{
    if (provider)
        strncpy(api_provider, provider, 31);
}

int ai_is_configured(void)
{
    return strlen(api_key) > 0;
}

static char *build_prompt(plan_request_t *request)
{
    char *prompt = malloc(8192);
    snprintf(prompt, 8192,
             "You are ExamPlanner AI. Create a detailed study plan.\n\n"
             "STUDENT INFO:\n"
             "- Subjects and chapters: %s\n"
             "- Days until exam: %d\n"
             "- Hours per day to study: %d\n"
             "- Preferences: %s\n\n"
             "Create a day-by-day study plan. Prioritize difficult chapters first. "
             "Alternate subjects to avoid mental fatigue. Include revision days.\n\n"
             "Return ONLY valid JSON in this exact format (no markdown, no extra text):\n"
             "{\n"
             "  \"plan\": [\n"
             "    {\n"
             "      \"day\": 1,\n"
             "      \"date\": \"YYYY-MM-DD\",\n"
             "      \"focus\": \"Short description of the day's focus\",\n"
             "      \"sessions\": [\n"
             "        {\n"
             "          \"subject\": \"Subject name\",\n"
             "          \"chapter\": \"Chapter name\",\n"
             "          \"duration_minutes\": 120,\n"
             "          \"priority\": \"haute\",\n"
             "          \"type\": \"cours\",\n"
             "          \"notes\": \"Specific instructions\"\n"
             "        }\n"
             "      ],\n"
             "      \"total_hours\": 5.0,\n"
             "      \"daily_tip\": \"Motivational tip for the day\"\n"
             "    }\n"
             "  ],\n"
             "  \"overall_strategy\": \"2-3 sentence overall preparation strategy\",\n"
             "  \"revision_days\": [7],\n"
             "  \"mock_exam_days\": [10],\n"
             "  \"estimated_improvement\": \"Estimated improvement percentage\"\n"
             "}\n\n"
             "Generate exactly %d days. Each day must total exactly %d hours. "
             "All text in French. Start from tomorrow's date.",
             request->subjects_json, request->days_remaining,
             request->hours_per_day, request->preferences,
             request->days_remaining, request->hours_per_day);
    return prompt;
}

study_plan_t *ai_generate_plan(plan_request_t *request)
{
    if (!ai_is_configured())
    {
        fprintf(stderr, "[AI ERROR] No API key set\n");
        return NULL;
    }

    CURL *curl = curl_easy_init();
    if (!curl)
    {
        fprintf(stderr, "[AI ERROR] curl init failed\n");
        return NULL;
    }

    char *prompt = build_prompt(request);

    // Build request body
    cJSON *body = cJSON_CreateObject();

    // Different model names for different providers
    if (strcmp(api_provider, "groq") == 0)
    {
        cJSON_AddStringToObject(body, "model", "llama-3.3-70b-versatile");
    }
    else if (strcmp(api_provider, "mistral") == 0)
    {
        cJSON_AddStringToObject(body, "model", "mistral-small-latest");
    }
    else
    {
        cJSON_AddStringToObject(body, "model", "llama-3.3-70b-versatile");
    }

    cJSON *messages = cJSON_CreateArray();

    cJSON *sys = cJSON_CreateObject();
    cJSON_AddStringToObject(sys, "role", "system");
    cJSON_AddStringToObject(sys, "content",
                            "You are a study plan generator. You respond ONLY with valid JSON. "
                            "No markdown, no code blocks, no explanations. Just the JSON object.");
    cJSON_AddItemToArray(messages, sys);

    cJSON *usr = cJSON_CreateObject();
    cJSON_AddStringToObject(usr, "role", "user");
    cJSON_AddStringToObject(usr, "content", prompt);
    cJSON_AddItemToArray(messages, usr);

    cJSON_AddItemToObject(body, "messages", messages);
    cJSON_AddNumberToObject(body, "max_tokens", 4000);
    cJSON_AddNumberToObject(body, "temperature", 0.7);

    char *json_body = cJSON_PrintUnformatted(body);
    free(prompt);

    // Response storage
    struct MemoryStruct response = {malloc(1), 0};

    // Headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", api_key);
    headers = curl_slist_append(headers, auth);

    // Different URLs for different providers
    const char *url;
    if (strcmp(api_provider, "groq") == 0)
    {
        url = "https://api.groq.com/openai/v1/chat/completions";
    }
    else if (strcmp(api_provider, "mistral") == 0)
    {
        url = "https://api.mistral.ai/v1/chat/completions";
    }
    else
    {
        url = "https://api.groq.com/openai/v1/chat/completions";
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    printf("[AI] Calling %s API...\n", api_provider);
    CURLcode res = curl_easy_perform(curl);

    study_plan_t *plan = NULL;

    if (res == CURLE_OK)
    {
        printf("[AI] Response received (%zu bytes)\n", response.size);

        cJSON *ai_resp = cJSON_Parse(response.memory);
        if (ai_resp)
        {
            cJSON *choices = cJSON_GetObjectItem(ai_resp, "choices");
            if (choices && cJSON_GetArraySize(choices) > 0)
            {
                cJSON *msg = cJSON_GetObjectItem(cJSON_GetArrayItem(choices, 0), "message");
                cJSON *content = cJSON_GetObjectItem(msg, "content");
                if (content && content->valuestring)
                {
                    printf("[AI] Parsing plan from response...\n");
                    plan = parse_ai_plan(content->valuestring);
                    if (plan)
                    {
                        printf("[AI] Success! Generated %d days\n", plan->day_count);
                    }
                    else
                    {
                        fprintf(stderr, "[AI ERROR] Failed to parse AI response as JSON\n");
                        printf("[AI DEBUG] Raw content: %.200s...\n", content->valuestring);
                    }
                }
            }
            else
            {
                cJSON *error = cJSON_GetObjectItem(ai_resp, "error");
                if (error)
                {
                    cJSON *err_msg = cJSON_GetObjectItem(error, "message");
                    fprintf(stderr, "[AI ERROR] API: %s\n",
                            err_msg ? err_msg->valuestring : "Unknown");
                }
            }
            cJSON_Delete(ai_resp);
        }
        else
        {
            fprintf(stderr, "[AI ERROR] Failed to parse API response\n");
        }
    }
    else
    {
        fprintf(stderr, "[AI ERROR] curl: %s\n", curl_easy_strerror(res));
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    free(json_body);
    free(response.memory);
    cJSON_Delete(body);

    return plan;
}
