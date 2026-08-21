// Purism Core: sample model viewer drawable shader (with a clipping mask)
//
// Copyright (c) 2026 Sakura Motion Project
// SPDX-License-Identifier: MIT

// Note: clip groups are channel-packed (4 per RGBA buffer). channelSelector determines
// the group.

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
    t.rgb *= multiplyColor.rgb;
    t.rgb = t.rgb + screenColor.rgb - t.rgb * screenColor.rgb;
    vec4 c = t * baseColor;
    float m = dot(texture(maskTexture, gl_FragCoord.xy / resolution), channelSelector);
    if (maskInvert > 0.5) m = 1.0 - m;
    c.a *= m;
    finalColor = vec4(c.rgb * c.a, c.a);
}
