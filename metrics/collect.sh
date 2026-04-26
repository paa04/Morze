#!/bin/bash
# Morze Team Speed Metrics Collector
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUT_DIR="$SCRIPT_DIR/data"
mkdir -p "$OUT_DIR"

cd "$REPO_ROOT"
echo "Collecting git metrics from $REPO_ROOT ..."

# 1. Commits per week per author
python3 -c "
import subprocess, json, collections
out = subprocess.check_output(['git','log','--all','--format=%an|%ai','--since=2026-01-01'], text=True)
weeks = collections.Counter()
for line in out.strip().split('\n'):
    if '|' not in line: continue
    author, date = line.split('|',1)
    import datetime
    d = datetime.datetime.strptime(date[:10], '%Y-%m-%d')
    week = d.strftime('%Y-W%V')
    weeks[(week, author)] += 1
result = [{'week': k[0], 'author': k[1], 'commits': v} for k,v in sorted(weeks.items())]
json.dump(result, open('$OUT_DIR/commits_weekly.json','w'), indent=2)
print(f'  commits_weekly.json: {len(result)} entries')
"

# 2. Commits by author
python3 -c "
import subprocess, json, collections
out = subprocess.check_output(['git','log','--all','--format=%an','--since=2026-01-01'], text=True)
counts = collections.Counter(out.strip().split('\n'))
result = [{'author': a, 'commits': c} for a,c in counts.most_common()]
json.dump(result, open('$OUT_DIR/commits_by_author.json','w'), indent=2)
print(f'  commits_by_author.json: {len(result)} entries')
"

# 3. LOC per author
python3 -c "
import subprocess, json, re
out = subprocess.check_output(['git','log','--all','--shortstat','--format=%an','--since=2026-01-01'], text=True)
stats = {}
author = None
for line in out.split('\n'):
    line = line.strip()
    if not line: continue
    if 'file' in line and 'changed' in line:
        ins = re.search(r'(\d+) insertion', line)
        dels = re.search(r'(\d+) deletion', line)
        i = int(ins.group(1)) if ins else 0
        d = int(dels.group(1)) if dels else 0
        if author:
            s = stats.setdefault(author, [0,0])
            s[0] += i; s[1] += d
    else:
        author = line
result = [{'author': a, 'insertions': v[0], 'deletions': v[1], 'churn_pct': round(v[1]*100/max(v[0],1))} for a,v in sorted(stats.items())]
json.dump(result, open('$OUT_DIR/loc_by_author.json','w'), indent=2)
print(f'  loc_by_author.json: {len(result)} entries')
"

# 4. Pull requests
python3 -c "
import subprocess, json, re
out = subprocess.check_output(['git','log','--all','--merges','--format=%ai|%s','--since=2026-01-01'], text=True)
result = []
for line in out.strip().split('\n'):
    if 'pull request' not in line.lower(): continue
    date, subject = line.split('|',1)
    pr = re.search(r'#(\d+)', subject)
    branch = re.search(r'from \S+/(\S+)', subject)
    result.append({
        'pr': int(pr.group(1)) if pr else 0,
        'branch': branch.group(1) if branch else 'unknown',
        'merged': date[:10]
    })
json.dump(result, open('$OUT_DIR/pull_requests.json','w'), indent=2)
print(f'  pull_requests.json: {len(result)} entries')
"

# 5. Commits daily
python3 -c "
import subprocess, json, collections
out = subprocess.check_output(['git','log','--all','--format=%ai','--since=2026-01-01'], text=True)
days = collections.Counter(line[:10] for line in out.strip().split('\n') if line.strip())
result = [{'date': d, 'commits': c} for d,c in sorted(days.items())]
json.dump(result, open('$OUT_DIR/commits_daily.json','w'), indent=2)
print(f'  commits_daily.json: {len(result)} entries')
"

# 6. Summary
python3 -c "
import subprocess, json, re
def git(*args):
    return subprocess.check_output(['git']+list(args), text=True, stderr=subprocess.DEVNULL)

commits = len(git('log','--all','--oneline','--since=2026-01-01').strip().split('\n'))
prs = git('log','--all','--merges','--since=2026-01-01').count('pull request')
authors = len(set(git('log','--all','--format=%an','--since=2026-01-01').strip().split('\n')))
dates = [l[:10] for l in git('log','--all','--format=%ai','--since=2026-01-01').strip().split('\n') if l]
first, last = min(dates), max(dates)
from datetime import datetime
weeks = max(1, (datetime.strptime(last,'%Y-%m-%d') - datetime.strptime(first,'%Y-%m-%d')).days // 7 + 1)

stat_out = git('log','--all','--shortstat','--since=2026-01-01')
ins = sum(int(x) for x in re.findall(r'(\d+) insertion', stat_out))
dels = sum(int(x) for x in re.findall(r'(\d+) deletion', stat_out))

result = [{
    'total_commits': commits, 'total_prs': prs, 'total_authors': authors,
    'project_weeks': weeks, 'loc_added': ins, 'loc_deleted': dels,
    'loc_net': ins - dels, 'commits_per_week': round(commits/weeks, 1),
    'prs_per_week': round(prs/weeks, 1), 'first_date': first, 'last_date': last
}]
json.dump(result, open('$OUT_DIR/summary.json','w'), indent=2)
print(f'  summary.json: {weeks} weeks, {commits} commits')
"

# 7. Hot files
python3 -c "
import subprocess, json, collections
out = subprocess.check_output(['git','log','--all','--name-only','--format=','--since=2026-01-01'], text=True)
files = collections.Counter(f for f in out.strip().split('\n') if f.strip())
result = [{'file': f, 'changes': c} for f,c in files.most_common(20)]
json.dump(result, open('$OUT_DIR/hot_files.json','w'), indent=2)
print(f'  hot_files.json: {len(result)} files')
"

# 8. JIRA Lead Time (requires JIRA_BASE_URL, JIRA_USER_EMAIL, JIRA_API_TOKEN)
echo "Collecting JIRA metrics..."
python3 "$SCRIPT_DIR/collect_jira.py"

echo "Done! Files in $OUT_DIR/"
