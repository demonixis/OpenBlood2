#version 450
layout(location=0) in vec3 faceColor;
layout(location=0) out vec4 color;
layout(location=1) in vec2 texCoord;
layout(location=2) in vec3 worldPosition;
layout(set=2,binding=0) uniform sampler2D albedo;

layout(set=3,binding=0,std140) uniform Light { vec4 modulation, positionRadius, lightColor; } u;
// Bounded radial brightening over the existing unlit base. No shadow/lightmap claim.
vec3 illumination() {
    float radial = u.positionRadius.w > 0.0 ? max(0.0, 1.0-distance(worldPosition,u.positionRadius.xyz)/u.positionRadius.w) : 0.0;
    return vec3(1.0)+u.lightColor.xyz*radial;
}
void main() { color=vec4(texture(albedo,texCoord).rgb*faceColor*illumination(),1.0); }
