#!/usr/bin/env python3
"""Extract compact blockstate/model geometry evidence from the official 26.2 client JAR."""
import argparse, hashlib, json, zipfile
from pathlib import Path
EXPECTED_SHA1 = "2dc72797acbc1b63fc16a11c4ac393605f453754"

def main():
    ap=argparse.ArgumentParser(); ap.add_argument("client_jar", type=Path); ap.add_argument("--output",type=Path,default=Path(__file__).parent/"source/official_26.2/client_geometry.json"); a=ap.parse_args()
    digest=hashlib.sha1(a.client_jar.read_bytes()).hexdigest()
    if digest != EXPECTED_SHA1: raise SystemExit(f"Unexpected client jar SHA-1: {digest}")
    with zipfile.ZipFile(a.client_jar) as z:
        names=set(z.namelist())
        def read(name): return json.loads(z.read(name))
        blockstates={n.split("/",4)[-1][:-5]:read(n) for n in names if n.startswith("assets/minecraft/blockstates/") and n.endswith(".json")}
        raw_models={n[len("assets/minecraft/models/"):-5]:read(n) for n in names if n.startswith("assets/minecraft/models/") and n.endswith(".json")}
    cache={}
    def resolved(model, trail=()):
        if model in cache:return cache[model]
        if model in trail or model not in raw_models:return []
        data=raw_models[model]; elems=[]
        if "parent" in data:
            parent=data["parent"].split(":",1)[-1]
            elems=resolved(parent,trail+(model,))
        if "elements" in data: elems=data["elements"]
        cache[model]=elems
        return elems
    refs={}
    def walk(v):
        if isinstance(v,dict):
            if isinstance(v.get("model"),str): refs[v["model"]]=True
            for x in v.values():walk(x)
        elif isinstance(v,list):
            for x in v:walk(x)
    for v in blockstates.values():walk(v)
    models={}
    for ref in sorted(refs):
        key=ref.split(":",1)[-1]
        models[ref]={"resolved_elements":[{"from":e.get("from"),"to":e.get("to"),"rotation":e.get("rotation")} for e in resolved(key)]}
    out={"source":{"edition":"Minecraft Java Edition","version":"26.2","client_jar_sha1":digest,"official_url":"https://piston-data.mojang.com/v1/objects/2dc72797acbc1b63fc16a11c4ac393605f453754/client.jar"},"blockstates":blockstates,"models":models}
    a.output.parent.mkdir(parents=True,exist_ok=True); a.output.write_text(json.dumps(out,indent=2)+"\n",encoding="utf-8")
    print(f"Extracted {len(blockstates)} blockstates and {len(models)} referenced models to {a.output}")
if __name__=="__main__":main()
