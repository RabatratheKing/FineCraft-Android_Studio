import json
with open('app/src/main/assets/atlas_map.json') as f:
    atlas = json.load(f)
for k, v in atlas.items():
    if v in [676, 677, 678, 459, 462, 375]:
        print(f"{v}: {k}")
