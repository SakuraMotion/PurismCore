// Purism Core viewer: offscreen group compositing with masking
//
// Copyright (c) 2026 Sakura Motion Project
// SPDX-License-Identifier: MIT

in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0;
uniform sampler2D maskTexture;
uniform vec2 resolution;
uniform float maskInvert;
uniform vec4 channelSelector;
uniform vec4 baseColor;
uniform vec4 multiplyColor;
uniform vec4 screenColor;

void main() {
    vec4 t = texture(texture0, fragTexCoord);
    vec3 c = t.rgb * multiplyColor.rgb;
    c = c + screenColor.rgb * t.a - c * screenColor.rgb;
    vec4 o = vec4(c, t.a) * baseColor.a;
    float m = dot(texture(maskTexture, gl_FragCoord.xy / resolution), channelSelector);
    if (maskInvert > 0.5) m = 1.0 - m;
    finalColor = o * m;
}
