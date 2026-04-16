import json, subprocess, sys

deps = json.load(open("ports/dependencies.json"))

before, after = sys.argv[1], sys.argv[2]

# Find which ports have changed
changed = set()
if before == "0000000000000000000000000000000000000000":
    changed = set(deps.keys())
else:
    diff = subprocess.check_output(["git", "diff", "--name-only", before, after]).decode()
    for line in diff.splitlines():
        if line.startswith("ports/"):
            port = line.split("/")[1]
            if port in deps:
                changed.add(port)

# Expand to include all dependents (ports that depend on a changed port)
def dependents(port):
    return {name for name, d in deps.items() if port in d}

dirty = set(changed)
while True:
    new = dirty | {p for d in dirty for p in dependents(d)}
    if new == dirty:
        break
    dirty = new

# Expand to include all transitive dependencies (for submodule checkout)
def dependencies(port):
    return set(deps.get(port, []))

checkout = set(dirty)
while True:
    new = checkout | {p for d in checkout for p in dependencies(d)}
    if new == checkout:
        break
    checkout = new

print(json.dumps({"build": sorted(dirty), "checkout": sorted(checkout)}))
