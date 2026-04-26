#!/bin/bash
# Генерирует self-contained HTML из уже собранных данных (metrics/data/*.json)
# Для полного цикла: сначала запустите collect.sh, потом этот скрипт.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Встроить JSON прямо в HTML
python3 -c "
import json, os

data_dir = '$SCRIPT_DIR/data'
files = ['summary','commits_daily','commits_by_author','loc_by_author',
         'pull_requests','hot_files','commits_weekly','lead_time']

embedded = {}
for f in files:
    with open(os.path.join(data_dir, f + '.json')) as fp:
        embedded[f] = json.load(fp)

with open('$SCRIPT_DIR/dashboard.html') as fp:
    html = fp.read()

# Заменяем fetch-вызовы на встроенные данные
inject = 'const EMBEDDED = ' + json.dumps(embedded, ensure_ascii=False) + ';\\n'
inject += '''async function load(name) {
  const key = name.replace('.json','');
  return EMBEDDED[key];
}

async function loadSafe(name) {
  const key = name.replace('.json','');
  return EMBEDDED[key] || [];
}'''

html = html.replace(
    '''async function load(name) {
  const r = await fetch('data/' + name);
  return r.json();
}

async function loadSafe(name) {
  try { return await load(name); } catch(e) { return []; }
}''', inject)

out = '$SCRIPT_DIR/team_speed_report.html'
with open(out, 'w') as fp:
    fp.write(html)
print(f'Done: {out}')
print('Open in browser — no server needed.')
"
