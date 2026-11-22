import json, sys
vf = sys.argv[1]
dec = json.JSONDecoder()
text, i, apps = sys.stdin.read(), 0, []
while i < len(text):
    if text[i].isspace():
        i += 1
        continue
    obj, i = dec.raw_decode(text, i)
    apps.append(obj)
for app in apps:
    if app.get("kind") != "Application":
        continue
    spec = app.get("spec", {})
    sources = spec.get("sources") or [spec.get("source", {})]
    refs = {s["ref"]: s for s in sources if s.get("ref")}
    for s in sources:
        files = s.get("helm", {}).get("valueFiles", [])
        local = []
        for f in files:
            if f.startswith("$"):
                ref, _, path = f[1:].partition("/")
                if "bl4ko/gitops-home" not in refs.get(ref, {}).get("repoURL", ""):
                    break
                local.append(path)
            else:
                local.append((s.get("path", "") + "/" + f).lstrip("/"))
        else:
            if vf not in local or not s.get("repoURL") or not s.get("targetRevision"):
                continue
            url, chart = s["repoURL"], s.get("chart", "")
            if url.startswith("oci://") or "://" not in url:
                ref = url if url.startswith("oci://") else "oci://" + url
                ref, repo = (ref if not chart or ref.endswith("/" + chart) else ref + "/" + chart), ""
            else:
                ref, repo = chart, url
            print("\t".join([app["metadata"]["name"], s.get("helm", {}).get("releaseName") or app["metadata"]["name"],
                ref, repo, str(s["targetRevision"]), spec.get("destination", {}).get("namespace", "default"), " ".join(local),
                "inline" if s["helm"].get("values") or s["helm"].get("valuesObject") or s["helm"].get("parameters") else ""]))
