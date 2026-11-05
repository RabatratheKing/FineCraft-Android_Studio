#pragma once


const char* vertexShaderSource = R"(#version 300 es
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in vec2 aUV;
    layout(location = 2) in float aTexIndex;
    layout(location = 3) in vec3 aNormal;
    layout(location = 4) in float aAO;

    out vec3 fragPos;
    out vec3 fragNormal;
    out vec2 fragUV;
    out float fragTexIndex;
    out float fragAO;
    
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    
    void main() {
        vec4 worldPos = model * vec4(aPos, 1.0);
        gl_Position = projection * view * worldPos;
        fragPos = worldPos.xyz;
        fragNormal = aNormal;
        fragUV = aUV;
        fragTexIndex = aTexIndex;
        fragAO = aAO;
    }
)";

const char* fragmentShaderSource = R"(#version 300 es
    precision mediump float;
    
    in vec3 fragPos;
    in vec3 fragNormal;
    in vec2 fragUV;
    in float fragTexIndex;
    in float fragAO;
    
    out vec4 FragColor;
    
    uniform vec3 sunDir;
    uniform vec3 sunColor;
    uniform vec3 horizonColor;
    uniform vec3 zenithColor;
    uniform vec3 cameraPos;
    uniform sampler2D atlas;
    uniform bool u_useFog;
    uniform bool u_useShadows;
    uniform float u_brightness;
    
    void main() {
        vec2 tileUV = fract(fragUV);
        float index = round(fragTexIndex);
        float tx = mod(index, 64.0);
        float ty = floor(index / 64.0);
        
        // Clamp to half a pixel (0.5 / 64.0) to prevent bleeding
        tileUV = clamp(tileUV, 0.0078125, 0.9921875);
        vec2 atlasUV = (vec2(tx, ty) + tileUV) * 0.015625;
        vec4 texColor = texture(atlas, atlasUV);
        
        if (texColor.a < 0.1) discard;
        
        // Dynamic tinting based on fractional part of texIndex
        float tintType = round((fragTexIndex - index) * 10.0);
        if (tintType == 1.0) {
            texColor.rgb *= vec3(0.44, 0.69, 0.33); // Foliage / Grass green
        } else if (tintType == 2.0) {
            texColor.rgb *= vec3(0.2, 0.4, 0.8); // Water blue
        }
        
        // AO: curve it for better look
        float ao = u_useShadows ? pow(fragAO, 1.5) : 1.0;
        
        // Lighting
        vec3 normal = normalize(fragNormal);
        float diff = max(dot(normal, sunDir), 0.0);
        
        // Fake bounce light from sky/ground
        vec3 ambient = mix(horizonColor * 0.4, zenithColor * 0.6, normal.y * 0.5 + 0.5);
        ambient += vec3(0.02, 0.02, 0.04); // Base night ambient
        
        vec3 diffuse = diff * sunColor;
        
        vec3 result = (ambient * ao + diffuse * ao) * texColor.rgb;
        
        // Fog
        float distance = length(cameraPos - fragPos);
        if(!u_useFog) distance = 0.0;
        float fogFactor = clamp((distance - 30.0) / 45.0, 0.0, 1.0);
        fogFactor = pow(fogFactor, 1.5); // smoother fog
        
        // Fog color matches the horizon
        vec3 fogColor = horizonColor;
        // if looking towards the sun, fog gets sun colored
        float sunView = max(dot(normalize(fragPos - cameraPos), sunDir), 0.0);
        fogColor = mix(fogColor, sunColor, pow(sunView, 4.0) * 0.4);
        
        FragColor = vec4(mix(result, fogColor, fogFactor) * u_brightness, 1.0);
    }
)";

const char* skyVertexShaderSource = R"(#version 300 es
    out vec3 rayDir;
    uniform mat4 invProjView;
    uniform vec3 cameraPos;
    void main() {
        float x = -1.0 + float((gl_VertexID & 1) << 2);
        float y = -1.0 + float((gl_VertexID & 2) << 1);
        gl_Position = vec4(x, y, 0.99999, 1.0);
        vec4 clipPos = vec4(x, y, 1.0, 1.0);
        vec4 worldPos = invProjView * clipPos;
        rayDir = (worldPos.xyz / worldPos.w) - cameraPos;
    }
)";

const char* skyFragmentShaderSource = R"(#version 300 es
    precision mediump float;
    in vec3 rayDir;
    out vec4 FragColor;
    
    uniform vec3 sunDir;
    uniform vec3 horizonColor;
    uniform vec3 zenithColor;
    uniform vec3 sunColor;
    uniform float time;
    uniform bool u_useClouds;
    uniform float u_brightness;
    
    float hash(vec2 p) { return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x)))); }
    float noise(vec2 x) {
        vec2 i = floor(x); vec2 f = fract(x);
        float a = hash(i); float b = hash(i + vec2(1.0, 0.0));
        float c = hash(i + vec2(0.0, 1.0)); float d = hash(i + vec2(1.0, 1.0));
        vec2 u = f * f * (3.0 - 2.0 * f);
        return mix(a, b, u.x) + (c - a)* u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
    }
    float fbm(vec2 p) {
        return noise(p)*0.5 + noise(p*2.0)*0.25 + noise(p*4.0)*0.125;
    }
    
    void main() {
        vec3 dir = normalize(rayDir);
        
        float dirY = max(dir.y, 0.0);
        vec3 sky = mix(horizonColor, zenithColor, pow(dirY, 0.5));
        
        float sunPos = dot(dir, sunDir);
        if (sunPos > 0.99) {
            sky = mix(sky, sunColor, smoothstep(0.99, 0.998, sunPos));
        }
        
        if (u_useClouds && dir.y > 0.05) {
            vec2 cloudUV = dir.xz / dir.y * 1.5 + time * 0.02;
            float cloudVal = fbm(cloudUV);
            float cloudCover = smoothstep(0.4, 0.8, cloudVal);
            vec3 cloudColor = mix(horizonColor * 0.5, sunColor * 0.8 + 0.2, max(sunDir.y, 0.0));
            sky = mix(sky, cloudColor, cloudCover * 0.9 * smoothstep(0.05, 0.2, dir.y));
        }
        
        FragColor = vec4(sky * u_brightness, 1.0);
    }
)";

const char* wireframeVertexShaderSource = R"(#version 300 es
    layout(location = 0) in vec3 aPos;
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    void main() {
        gl_Position = projection * view * model * vec4(aPos, 1.0);
    }
)";
const char* wireframeFragmentShaderSource = R"(#version 300 es
    precision mediump float;
    uniform vec4 u_color;
    out vec4 FragColor;
    void main() {
        FragColor = u_color;
    }
)";

const char* uiVertexShaderSource = R"(#version 300 es
    layout(location = 0) in vec2 aPos;
    layout(location = 1) in vec2 aUV;
    uniform mat4 projection;
    uniform mat4 model;
    out vec2 vUV;
    void main() {
        gl_Position = projection * model * vec4(aPos, 0.0, 1.0);
        vUV = aUV;
    }
)";

const char* uiFragmentShaderSource = R"(#version 300 es
    precision mediump float;
    in vec2 vUV;
    uniform vec4 u_color;
    uniform bool u_useTex;
    uniform sampler2D u_tex;
    out vec4 FragColor;
    void main() {
        if (u_useTex) {
            vec4 texColor = texture(u_tex, vUV);
            if (texColor.a < 0.1) discard;
            FragColor = texColor * u_color;
        } else {
            FragColor = u_color;
        }
    }
)";
