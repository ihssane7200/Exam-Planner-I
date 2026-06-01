#ifndef AI_CLIENT_H
#define AI_CLIENT_H

#include "cjson/cJSON.h"
#include "json_parser.h"

// Configuration
void ai_set_api_key(const char *key);
void ai_set_provider(const char *provider);
int ai_is_configured(void);

// Core function
study_plan_t *ai_generate_plan(plan_request_t *request);

#endif // AI_CLIENT_H
