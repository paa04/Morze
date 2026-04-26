#!/usr/bin/env python3
"""
Fetch JIRA KAN project issues with changelog and compute Lead Time
broken down by status per month (average days in each status).

Required env vars: JIRA_BASE_URL, JIRA_USER_EMAIL, JIRA_API_TOKEN
Optional env var:  JIRA_PROJECT_KEY (default: KAN)
"""

import os, sys, json, math
from datetime import datetime, timezone
from collections import defaultdict
from urllib.request import Request, urlopen
from urllib.parse import urlencode
from base64 import b64encode

# --- config ---
# Extract base URL (strip any /jira/software/... paths)
_raw_url = os.environ.get("JIRA_BASE_URL", "").rstrip("/")
# Keep only scheme + host (e.g. https://x.atlassian.net)
if _raw_url:
    from urllib.parse import urlparse
    _parsed = urlparse(_raw_url)
    BASE_URL = f"{_parsed.scheme}://{_parsed.netloc}"
else:
    BASE_URL = ""
EMAIL    = os.environ.get("JIRA_USER_EMAIL", "")
TOKEN    = os.environ.get("JIRA_API_TOKEN", "")
PROJECT  = os.environ.get("JIRA_PROJECT_KEY", "KAN")
OUT_DIR  = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data")

def write_empty():
    os.makedirs(OUT_DIR, exist_ok=True)
    with open(os.path.join(OUT_DIR, "lead_time.json"), "w") as f:
        json.dump([], f)

if not all([BASE_URL, EMAIL, TOKEN]):
    print("JIRA credentials not set, skipping JIRA metrics")
    print(f"  JIRA_BASE_URL={'set' if BASE_URL else 'EMPTY'}")
    print(f"  JIRA_USER_EMAIL={'set' if EMAIL else 'EMPTY'}")
    print(f"  JIRA_API_TOKEN={'set' if TOKEN else 'EMPTY'}")
    write_empty()
    sys.exit(0)

AUTH = b64encode(f"{EMAIL}:{TOKEN}".encode()).decode()
HEADERS = {
    "Authorization": f"Basic {AUTH}",
    "Accept": "application/json",
    "Content-Type": "application/json",
}

# Statuses in display order (matches the diagram)
STATUS_ORDER = [
    "В работе",
    "Ревью",
    "Подготовка к тестированию",
    "На проверку",
    "Тестируется",
    "Ожидает приемку",
    "Приемка на платформе",
    "Готово",
    "Требуется выгрузка",
]

def jira_get(path, params=None):
    url = BASE_URL + path
    if params:
        url += "?" + urlencode(params)
    print(f"  GET {url[:80]}...")
    req = Request(url, headers=HEADERS)
    try:
        with urlopen(req, timeout=30) as resp:
            return json.loads(resp.read())
    except Exception as e:
        print(f"  JIRA API error: {e}", file=sys.stderr)
        raise

def fetch_all_issues():
    """Fetch all issues from project with changelog."""
    issues = []
    start = 0
    while True:
        data = jira_get("/rest/api/2/search", {
            "jql": f"project={PROJECT} ORDER BY created ASC",
            "startAt": start,
            "maxResults": 100,
            "expand": "changelog",
            "fields": "created,resolutiondate,status,summary",
        })
        issues.extend(data["issues"])
        if start + data["maxResults"] >= data["total"]:
            break
        start += data["maxResults"]
    return issues

def parse_dt(s):
    """Parse JIRA datetime string."""
    if not s:
        return None
    # Handle various JIRA datetime formats
    for fmt in ("%Y-%m-%dT%H:%M:%S.%f%z", "%Y-%m-%dT%H:%M:%S%z", "%Y-%m-%dT%H:%M:%S.%f"):
        try:
            return datetime.strptime(s, fmt)
        except ValueError:
            continue
    # Fallback: strip timezone offset manually
    try:
        clean = s[:19]
        return datetime.strptime(clean, "%Y-%m-%dT%H:%M:%S").replace(tzinfo=timezone.utc)
    except Exception:
        return None

def compute_status_durations(issue):
    """
    Walk the changelog and compute how many days the issue spent in each status.
    Returns dict {status_name: days}.
    """
    created = parse_dt(issue["fields"]["created"])
    if not created:
        return {}

    # Build list of (timestamp, from_status, to_status) transitions
    transitions = []
    for history in issue.get("changelog", {}).get("histories", []):
        ts = parse_dt(history["created"])
        for item in history["items"]:
            if item["field"] == "status":
                transitions.append((ts, item["fromString"], item["toString"]))

    transitions.sort(key=lambda x: x[0])

    durations = defaultdict(float)

    if not transitions:
        # No transitions — issue is still in initial status
        # Use current time as end
        initial_status = issue["fields"]["status"]["name"]
        now = datetime.now(timezone.utc)
        days = (now - created).total_seconds() / 86400
        durations[initial_status] = days
        return dict(durations)

    # Time from creation to first transition
    first_from = transitions[0][1]
    days_initial = (transitions[0][0] - created).total_seconds() / 86400
    if days_initial > 0:
        durations[first_from] += days_initial

    # Time between transitions
    for i in range(len(transitions)):
        status = transitions[i][2]  # status we moved TO
        if i + 1 < len(transitions):
            end = transitions[i + 1][0]
        else:
            # Last transition — use resolution date or now
            res = parse_dt(issue["fields"].get("resolutiondate"))
            end = res if res else datetime.now(timezone.utc)
        days = (end - transitions[i][0]).total_seconds() / 86400
        if days > 0:
            durations[status] += days

    return dict(durations)

def get_issue_month(issue):
    """Determine which month to attribute this issue to (resolution month or created month)."""
    res = parse_dt(issue["fields"].get("resolutiondate"))
    if res:
        return res.strftime("%Y-%m")
    created = parse_dt(issue["fields"]["created"])
    if created:
        return created.strftime("%Y-%m")
    return None

def main():
    print(f"Fetching JIRA issues from project {PROJECT}...")
    issues = fetch_all_issues()
    print(f"  Fetched {len(issues)} issues")

    # Group durations by month
    # month -> status -> [list of days]
    monthly = defaultdict(lambda: defaultdict(list))

    for issue in issues:
        month = get_issue_month(issue)
        if not month:
            continue
        durations = compute_status_durations(issue)
        for status, days in durations.items():
            monthly[month][status].append(days)

    # Build output: for each month, average days per status
    result = []
    for month in sorted(monthly.keys()):
        entry = {"month": month}
        statuses = {}
        for status in STATUS_ORDER:
            vals = monthly[month].get(status, [])
            if vals:
                statuses[status] = round(sum(vals) / len(vals), 2)
            else:
                statuses[status] = 0
        # Also include any statuses not in STATUS_ORDER
        for status, vals in monthly[month].items():
            if status not in statuses:
                statuses[status] = round(sum(vals) / len(vals), 2)
        entry["statuses"] = statuses
        entry["issue_count"] = sum(len(v) for v in monthly[month].values())
        result.append(entry)

    os.makedirs(OUT_DIR, exist_ok=True)
    out_path = os.path.join(OUT_DIR, "lead_time.json")
    with open(out_path, "w") as f:
        json.dump(result, f, indent=2, ensure_ascii=False)
    print(f"  lead_time.json: {len(result)} months")

if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"JIRA collection failed: {e}", file=sys.stderr)
        write_empty()
        sys.exit(0)  # don't fail the whole pipeline
