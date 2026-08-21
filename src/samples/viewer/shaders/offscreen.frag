// Purism Core: sample model viewer offscreen group compositing shader
//
// Copyright (c) 2026 Sakura Motion Project
// SPDX-License-Identifier: MIT

in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0; // premultiplied
uniform vec4 baseColor; // (1, 1, 1, opacity)
uniform vec4 multiplyColor;
uniform vec4 screenColor;

void main() {
    vec4 t = texture(texture0, fragTexCoord);
    vec3 c = t.rgb * multiplyColor.rgb;
    c = c + screenColor.rgb * t.a - c * screenColor.rgb;
    finalColor = vec4(c, t.a) * baseColor.a; // scale premult by opacity
}
