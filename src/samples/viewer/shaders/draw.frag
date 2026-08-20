// Purism Core viewer: drawable shader
//
// Copyright (c) 2026 Sakura Motion Project
// SPDX-License-Identifier MIT

in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec4 baseColor; // (1, 1, 1, opacity)
uniform vec4 multiplyColor;
uniform vec4 screenColor;

void main() {
    vec4 t = texture(texture0, fragTexCoord);
    t.rgb *= multiplyColor.rgb;
    t.rgb = t.rgb + screenColor.rgb - t.rgb * screenColor.rgb;
    vec4 c = t * baseColor;
    finalColor = vec4(c.rgb * c.a, c.a); // premultiply
}
