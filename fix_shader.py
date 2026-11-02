import re

with open('app/src/main/cpp/shaders/Shaders.h', 'r') as f:
    content = f.read()

old_water = """        // Water transparency / tinting
        if (fragTexIndex == 7.0) {
            texColor.rgb *= vec3(0.6, 0.8, 1.0);
        }"""

new_tint = """        // Grass and foliage tint
        if (index == 462.0 || index == 676.0 || index == 973.0 || index == 74.0 || index == 533.0 || index == 2.0 || index == 321.0 || index == 425.0 || index == 546.0 || index == 545.0 || index == 1039.0 || index == 1038.0 || index == 460.0) {
            texColor.rgb *= vec3(0.44, 0.69, 0.33); // A nice grassy green
        }

        // Water transparency / tinting
        if (index == 1094.0 || index == 1095.0) {
            texColor.rgb *= vec3(0.2, 0.4, 0.8); // Adjust water color
        }"""

if old_water in content:
    content = content.replace(old_water, new_tint)
    with open('app/src/main/cpp/shaders/Shaders.h', 'w') as f:
        f.write(content)
    print("Shader patched successfully.")
else:
    print("Could not find the target code in shader.")
