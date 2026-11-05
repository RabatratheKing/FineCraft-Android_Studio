import re

uv_logic = """
                        float uv0[2], uv1[2], uv2[2], uv3[2];
                        float fw = (float)du[u];
                        float fh = (float)dv[v];

                        if (d == 0) { // X axis. u=Y(fw), v=Z(fh)
                            if (dir == 1) { // +X Right
                                uv0[0] = fh; uv0[1] = fw;
                                uv1[0] = fh; uv1[1] = 0;
                                uv2[0] = 0; uv2[1] = 0;
                                uv3[0] = 0; uv3[1] = fw;
                            } else { // -X Left
                                uv0[0] = 0; uv0[1] = fw;
                                uv1[0] = 0; uv1[1] = 0;
                                uv2[0] = fh; uv2[1] = 0;
                                uv3[0] = fh; uv3[1] = fw;
                            }
                        } else if (d == 1) { // Y axis. u=Z(fw), v=X(fh)
                            if (dir == 1) { // +Y Top
                                uv0[0] = 0; uv0[1] = 0;
                                uv1[0] = 0; uv1[1] = fw;
                                uv2[0] = fh; uv2[1] = fw;
                                uv3[0] = fh; uv3[1] = 0;
                            } else { // -Y Bottom
                                uv0[0] = 0; uv0[1] = 0;
                                uv1[0] = 0; uv1[1] = fw;
                                uv2[0] = fh; uv2[1] = fw;
                                uv3[0] = fh; uv3[1] = 0;
                            }
                        } else { // Z axis. u=X(fw), v=Y(fh)
                            if (dir == 1) { // +Z Front
                                uv0[0] = 0; uv0[1] = fh;
                                uv1[0] = fw; uv1[1] = fh;
                                uv2[0] = fw; uv2[1] = 0;
                                uv3[0] = 0; uv3[1] = 0;
                            } else { // -Z Back
                                uv0[0] = fw; uv0[1] = fh;
                                uv1[0] = 0; uv1[1] = fh;
                                uv2[0] = 0; uv2[1] = 0;
                                uv3[0] = fw; uv3[1] = 0;
                            }
                        }

                        if (!flip) {
                            addV(v0, uv0[0], uv0[1]); addV(v1, uv1[0], uv1[1]); addV(v2, uv2[0], uv2[1]); 
                            addV(v2, uv2[0], uv2[1]); addV(v3, uv3[0], uv3[1]); addV(v0, uv0[0], uv0[1]);
                        } else {
                            addV(v0, uv0[0], uv0[1]); addV(v3, uv3[0], uv3[1]); addV(v2, uv2[0], uv2[1]); 
                            addV(v2, uv2[0], uv2[1]); addV(v1, uv1[0], uv1[1]); addV(v0, uv0[0], uv0[1]);
                        }
"""

with open('app/src/main/cpp/world/World.cpp', 'r') as f:
    content = f.read()

pattern = re.compile(r'float uv0\[2\].*?addV\(v0, uv0\[0\], uv0\[1\]\);\s*\}', re.DOTALL)
if pattern.search(content):
    new_content = pattern.sub(uv_logic.strip(), content)
    with open('app/src/main/cpp/world/World.cpp', 'w') as f:
        f.write(new_content)
    print("Successfully replaced UV logic.")
else:
    print("Could not find UV logic block to replace.")
