#version 330 core

in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;

uniform sampler2D texture0;
uniform vec2 resolution;
uniform float time;
uniform vec3 phosphorTint;  // Theme color passed from CPU

// CRT parameters - subtle atmosphere, maximum readability
const float SCANLINE_STRENGTH = 0.02;
const float SCANLINE_FREQ = 1.0;
const float BRIGHTNESS = 1.02;
const float CONTRAST = 1.01;
const float CURVATURE = 0.015;
const float VIGNETTE_STRENGTH = 0.08;
const float GLOW_STRENGTH = 0.04;
const float FLICKER_SPEED = 8.0;
const float FLICKER_STRENGTH = 0.003;

// Apply barrel distortion for CRT curvature
vec2 applyCurvature(vec2 uv) {
    uv = uv * 2.0 - 1.0;
    float dist = length(uv);
    uv = uv * (1.0 + CURVATURE * dist * dist) / (1.0 + CURVATURE);
    return uv * 0.5 + 0.5;
}

// Generate scanlines
float scanlines(vec2 uv) {
    return sin(uv.y * resolution.y * SCANLINE_FREQ) * SCANLINE_STRENGTH;
}

// Create vignette effect
float vignette(vec2 uv) {
    vec2 center = vec2(0.5, 0.5);
    float dist = distance(uv, center);
    return 1.0 - smoothstep(0.0, 1.0, dist * VIGNETTE_STRENGTH);
}

// Add phosphor glow/bloom
vec3 phosphorGlow(vec3 color, vec2 uv) {
    // Sample neighboring pixels for bloom effect
    vec2 texel = 1.0 / resolution;
    vec3 glow = vec3(0.0);
    
    glow += texture(texture0, uv + vec2(texel.x, 0.0)).rgb * 0.25;
    glow += texture(texture0, uv - vec2(texel.x, 0.0)).rgb * 0.25;
    glow += texture(texture0, uv + vec2(0.0, texel.y)).rgb * 0.25;
    glow += texture(texture0, uv - vec2(0.0, texel.y)).rgb * 0.25;
    
    return color + glow * GLOW_STRENGTH;
}

// Subtle flicker effect
float flicker() {
    return 1.0 + sin(time * FLICKER_SPEED) * FLICKER_STRENGTH;
}

void main() {
    // Apply curvature
    vec2 uv = applyCurvature(fragTexCoord);
    
    // Check if we're outside the curved area
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        finalColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    
    // Sample the texture
    vec4 texColor = texture(texture0, uv);
    
    // Apply brightness and contrast
    vec3 color = texColor.rgb * BRIGHTNESS;
    color = (color - 0.5) * CONTRAST + 0.5;
    
    // Apply subtle phosphor atmospheric tint
    // Preserve original colors (drawn in theme color already), just add a slight cast
    // Mix between original color and tinted version at low strength
    vec3 tinted = color * phosphorTint * 1.1;
    color = mix(color, tinted, 0.12);
    
    // Add scanlines
    color *= (1.0 - scanlines(uv));
    
    // Add phosphor glow
    color = phosphorGlow(color, uv);
    
    // Apply vignette
    color *= vignette(uv);
    
    // Add flicker
    color *= flicker();
    
    // Ensure we don't exceed 1.0
    color = clamp(color, 0.0, 1.0);
    
    finalColor = vec4(color, texColor.a);
}
