# Exam Planner AI — API Documentation

## Base URL

- Local: http://localhost:8080
- Production: https://[your-app].up.railway.app

## Endpoints

### GET /api/health

Returns server status.
Response: {"status":"ok","service":"Exam Planner AI","version":"1.0"}

### POST /api/generate-plan

Generates AI study plan.
Request: {user_id, user_name, subjects, days_remaining, hours_per_day, preferences}
Response: {plan_id, status, strategy, estimated_improvement, plan[]}

### GET /api/plan/{id}

Retrieves saved plan.

### GET /api/plan/{id}/days

Returns days for a plan.

### GET /api/history?user_id={id}

Returns all user plans.
