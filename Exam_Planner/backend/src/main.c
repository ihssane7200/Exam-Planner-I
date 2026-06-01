#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "server.h"
#include "db_init.h"
#include "ai_client.h"

int main(int argc, char *argv[])
{
    printf("╔══════════════════════════════════╗\n");
    printf("║     EXAM PLANNER AI v1.0         ║\n");
    printf("╚══════════════════════════════════╝\n\n");

    // Step 1: Initialize database
    printf("[1/3] Initializing database...\n");
    if (db_init("exam_planner.db") != 0)
    {
        fprintf(stderr, "[ERROR] Database init failed\n");
        return 1;
    }
    printf("      Database ready.\n\n");

    // Step 2: Load API key
    printf("[2/3] Loading API key...\n");

    // Try Groq first
    const char *api_key = getenv("GROQ_API_KEY");
    if (api_key)
    {
        ai_set_api_key(api_key);
        ai_set_provider("groq");
        printf("      Using Groq API (free)\n");
    }
    else
    {
        // Try Mistral
        api_key = getenv("MISTRAL_API_KEY");
        if (api_key)
        {
            ai_set_api_key(api_key);
            ai_set_provider("mistral");
            printf("      Using Mistral API (free)\n");
        }
        else
        {
            printf("      WARNING: No API key found.\n");
            printf("      Set GROQ_API_KEY or MISTRAL_API_KEY\n");
        }
    }
    printf("\n");

    // Step 3: Start server
    if (argc > 1 && strcmp(argv[1], "--serve") == 0)
    {
        int port = 8080;
        if (argc > 2)
            port = atoi(argv[2]);

        printf("[3/3] Starting server...\n");
        printf("      URL: http://localhost:%d\n", port);
        printf("      Press Ctrl+C to stop\n\n");
        server_start(port);
    }
    else
    {
        printf("Usage: %s --serve [port]\n", argv[0]);
        printf("Example: %s --serve 8080\n", argv[0]);
    }

    db_close();
    return 0;
}
