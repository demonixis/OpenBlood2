#version 450
layout(location=0) in vec3 faceColor;
layout(location=1) in vec2 texCoord;
layout(location=0) out vec4 color;
layout(set=2,binding=0) uniform sampler2D albedo;
layout(set=3,binding=0,std140) uniform Sprite { vec4 modulation; } u;
// DTX alpha multiplied by the original effect opacity. The original SDK explicitly disables FLAG_SPRITECHROMAKEY.
void main() {
    vec4 texel=texture(albedo,texCoord);
    if (texel.a == 0.0) discard;
    color=vec4(texel.rgb*faceColor,texel.a*u.modulation.x);
}
