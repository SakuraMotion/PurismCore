// Purism Core viewer: write alpha coverage of a drawable (mask) to an RGBA channel
//
// Copyright (c) 2026 Sakura Motion Project
// SPDX-License-Identifier: MIT

in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec4 channelMask;
void main() {
    float a = texture(texture0, fragTexCoord).a;
    finalColor = a * channelMask;
}
