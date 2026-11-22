import json, os, re, sys
kv = sys.argv[1]
core = re.compile(r"^(v1|(apps|batch|policy|autoscaling|(networking|rbac\.authorization|storage|scheduling|coordination|node|discovery|admissionregistration|apiextensions|certificates|resource|flowcontrol\.apiserver)\.k8s\.io)/[a-z0-9]+)$")
def docs(path):
    out = []
    for line in open(path):
        line = line.strip()
        if line and line != "null":
            out.append(json.loads(line))
    return [d for d in out if isinstance(d, dict) and d.get("kind")]
def write(path, items):
    open(path, "w").write("\n---\n".join(json.dumps(i) for i in items) + "\n")
def plural(kind):
    k = kind.lower()
    return k[:-1] + "ies" if k.endswith("y") else k + "es" if k.endswith(("s", "x", "ch")) else k + "s"
def ns_ok(sel, ns):
    if not sel or not ns:
        return True
    labels = {"kubernetes.io/metadata.name": ns}
    for key, value in (sel.get("matchLabels") or {}).items():
        if key in labels and labels[key] != value:
            return False
    for e in sel.get("matchExpressions") or []:
        if e.get("key") in labels:
            have = labels[e["key"]] in (e.get("values") or [])
            if (e.get("operator") == "In" and not have) or (e.get("operator") == "NotIn" and have):
                return False
    return True
def matches(vap, r):
    group, _, _ = r["apiVersion"].rpartition("/")
    mc = vap["spec"].get("matchConstraints") or {}
    rules = mc.get("resourceRules") or []
    hit = any((group in rule.get("apiGroups", []) or "*" in rule.get("apiGroups", []))
              and (plural(r["kind"]) in rule.get("resources", []) or "*" in rule.get("resources", []))
              and any(op in ("CREATE", "*") for op in rule.get("operations", ["*"])) for rule in rules)
    return hit and ns_ok(mc.get("namespaceSelector"), r.get("metadata", {}).get("namespace", ""))
custom, seen, keep = 0, set(), []
for r in docs(os.path.join(kv, "res.jsonl")):
    if not core.match(r.get("apiVersion", "")):
        custom += 1
        continue
    m = r.get("metadata", {})
    key = (r["apiVersion"], r["kind"], m.get("namespace", ""), m.get("name", ""))
    if key not in seen:
        seen.add(key)
        keep.append(r)
if keep:
    write(os.path.join(kv, "all.yaml"), keep)
jobs = []
for name in sorted(os.listdir(os.path.join(kv, "pol"))):
    items = docs(os.path.join(kv, "pol", name))
    vaps = [i for i in items if i["kind"] == "ValidatingAdmissionPolicy"]
    others = [i for i in items if i["kind"] not in ("ValidatingAdmissionPolicy", "ValidatingAdmissionPolicyBinding")]
    base = name.removesuffix(".jsonl").removesuffix(".yaml")
    if others and keep:
        write(os.path.join(kv, f"{base}.yaml"), others)
        jobs.append(f"{base}.yaml all.yaml")
    for n, vap in enumerate(vaps):
        hits = [r for r in keep if matches(vap, r)]
        if not hits:
            continue
        binds = [i for i in items if i["kind"] == "ValidatingAdmissionPolicyBinding" and i["spec"].get("policyName") == vap["metadata"]["name"]]
        write(os.path.join(kv, f"{base}-{n}.yaml"), [vap] + binds)
        write(os.path.join(kv, f"{base}-{n}-res.yaml"), hits)
        jobs.append(f"{base}-{n}.yaml {base}-{n}-res.yaml")
open(os.path.join(kv, "jobs"), "w").write("\n".join(jobs) + ("\n" if jobs else ""))
print(custom)
