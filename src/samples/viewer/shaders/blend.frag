// Purism Core: sample model viewer compositing shader for "exotic" blend modes
//
// Copyright (c) 2026 Sakura Motion Project
// SPDX-License-Identifier: MIT

in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0; // source
uniform sampler2D blendTexture; // backdrop
uniform sampler2D maskTexture;
uniform vec2 resolution;

uniform int colorMode; // must be a valid csmColorBlendType value
uniform int alphaMode; // must be a valid csmAlphaBlendType value

uniform float useMask;
uniform float maskInvert;
uniform vec4 channelSelector;

uniform vec4 baseColor;
uniform vec4 multiplyColor;
uniform vec4 screenColor;

vec4 straight(vec4 c) {
    return c.a < 1e-5 ? vec4(0.0) : vec4(c.rgb / c.a, c.a);
}

float burn(float s, float d) {
    if (d >= 0.999999) return 1.0;
    if (s < 1e-6) return 0.0;
    return 1.0 - min(1.0, (1.0 - d) / s);
}

float dodge(float s, float d) {
    if (d <= 0.0) return 0.0;
    if (s >= 1.0) return 1.0;
    return min(1.0, d / (1.0 - s));
}

float overlay(float s, float d) {
    return d < 0.5 ? 2.0 * s * d : 1.0 - 2.0 * (1.0 - s) * (1.0 - d);
}

float hardl(float s, float d) {
    return s < 0.5 ? 2.0 * s * d : 1.0 - 2.0 * (1.0 - s) * (1.0 - d);
}

float softl(float s, float d) {
    float a = d - (1.0 - 2.0 * s) * d * (1.0 - d);
    float b = d + (2.0 * s - 1.0) * d * ((16.0 * d - 12.0) * d + 3.0);
    float c = d + (2.0 * s - 1.0) * (sqrt(d) - d);
    if (s <= 0.5) return a;
    return d <= 0.25 ? b : c;
}

float linl(float s, float d) {
    float bn = max(0.0, 2.0 * s + d - 1.0);
    float dg = min(1.0, 2.0 * (s - 0.5) + d);
    return s < 0.5 ? bn : dg;
}

float luma(vec3 c) {
    return 0.30 * c.r + 0.59 * c.g + 0.11 * c.b;
}

vec3 clipcolor(vec3 c) {
    float l = luma(c);
    float mn = min(c.r, min(c.g, c.b));
    float mx = max(c.r, max(c.g, c.b));
    if (mn < 0.0) c = l + (c - l) * l / (l - mn);
    if (mx > 1.0) c = l + (c - l) * (1.0 - l) / (mx - l);
    return c;
}

vec3 setluma(vec3 c, float l) { return clipcolor(c + (l - luma(c))); }

vec3 setsat(vec3 c, float s) {
    float mx = max(c.r, max(c.g, c.b));
    float mn = min(c.r, min(c.g, c.b));
    float md = c.r + c.g + c.b - mx - mn;
    float oMax = mn < mx ? s : 0.0;
    float oMed = mn < mx ? (md - mn) * s / (mx - mn) : 0.0;
    if (c.r == mx) return c.b < c.g ? vec3(oMax, oMed, 0.0) : vec3(oMax, 0.0, oMed);
    else if (c.g == mx) return c.r < c.b ? vec3(0.0, oMax, oMed) : vec3(oMed, oMax, 0.0);
    return c.g < c.r ? vec3(oMed, 0.0, oMax) : vec3(0.0, oMed, oMax);
}

vec3 colorBlend(vec3 s, vec3 d) {
    if (colorMode == 1 || colorMode == 3) return min(s + d, 1.0); // Add / AddCompatible
    if (colorMode == 4) return s + d; // AddGlow
    if (colorMode == 5) return min(s, d); // Darken
    if (colorMode == 2 || colorMode == 6) return s * d; // Multiply / MultiplyCompatible
    if (colorMode == 7) return vec3(burn(s.r, d.r), burn(s.g, d.g), burn(s.b, d.b));
    if (colorMode == 8) return max(vec3(0.0), s + d - 1.0); // LinearBurn
    if (colorMode == 9) return max(s, d); // Lighten
    if (colorMode == 10) return s + d - s * d;
    if (colorMode == 11) return vec3(dodge(s.r, d.r), dodge(s.g, d.g), dodge(s.b, d.b));
    if (colorMode == 12) return vec3(overlay(s.r, d.r), overlay(s.g, d.g), overlay(s.b, d.b));
    if (colorMode == 13) return vec3(softl(s.r, d.r), softl(s.g, d.g), softl(s.b, d.b));
    if (colorMode == 14) return vec3(hardl(s.r, d.r), hardl(s.g, d.g), hardl(s.b, d.b));
    if (colorMode == 15) return vec3(linl(s.r, d.r), linl(s.g, d.g), linl(s.b, d.b));
    if (colorMode == 16) return setluma(setsat(s, max(d.r, max(d.g, d.b)) - min(d.r, min(d.g, d.b))), luma(d));
    if (colorMode == 17) return setluma(s, luma(d));
    return s; // Normal
}

// Porter-Duff coverage weights (sa,da = source/dest alpha) for the result
// result = col*w.x + s*w.y + d*w.z, alpha = w.x + w.y + w.z.
vec3 alphaBlend(float sa, float da) {
    if (alphaMode == 1) return vec3(sa * da, 0.0, da * (1.0 - sa)); // Atop
    if (alphaMode == 2) return vec3(0.0, 0.0, da * (1.0 - sa)); // Out
    if (alphaMode == 3) return vec3(min(sa, da), max(sa - da, 0.0), max(da - sa, 0.0)); // ConjointOver
    if (alphaMode == 4) return vec3(max(sa + da - 1.0, 0.0), min(sa, 1.0 - da), min(da, 1.0 - sa)); // DisjointOver
    return vec3(sa * da, sa * (1.0 - da), da * (1.0 - sa)); // Over
}

void main() {
    vec4 t = texture(texture0, fragTexCoord);
    t.rgb = t.rgb * multiplyColor.rgb;
    t.rgb = t.rgb + screenColor.rgb * t.a - t.rgb * screenColor.rgb;
    if (useMask > 0.5) {
        float m = dot(texture(maskTexture, gl_FragCoord.xy / resolution), channelSelector);
        if (maskInvert > 0.5) m = 1.0 - m;
        t *= m;
    }
    vec4 s = straight(t * baseColor);
    vec4 d = straight(texture(blendTexture, gl_FragCoord.xy / resolution));

    vec3 col = colorBlend(s.rgb, d.rgb);
    vec3 p = alphaBlend(s.a, d.a);
    finalColor = vec4(col * p.x + s.rgb * p.y + d.rgb * p.z, p.x + p.y + p.z);
}
