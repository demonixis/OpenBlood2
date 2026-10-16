#version 450
layout(location=0) in vec3 faceColor;
layout(location=0) out vec4 color;
layout(location=1) in vec2 texCoord;
layout(set=2,binding=0) uniform sampler2D albedo;
// Opaque base-color diagnostic. Alpha/sky/lightmaps are explicitly separate work.
void main() { color=vec4(texture(albedo,texCoord).rgb*faceColor,1.0); }
