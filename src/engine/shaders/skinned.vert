#version 450
// Rigid ABC skinning: one node per vertex, optional CPU-sampled packed deformation.
layout(location=0) in vec3 position;
layout(location=1) in vec2 uv;
layout(location=2) in uvec4 nodes; // x: vertex node slot; yzw: the face's three corner slots.
layout(location=3) in uint deform;  // Deformation slot, or 0xffffffff for the authored position.
layout(location=0) out vec3 faceColor;
layout(location=1) out vec2 texCoord;
layout(location=2) out vec3 worldPosition;
// Per node: rotation quaternion (xyzw), then translation (xyz) with w = 1 when hidden.
layout(set=0,binding=0,std430) readonly buffer Palette { vec4 bones[]; } palette;
layout(set=0,binding=1,std430) readonly buffer Deformation { vec4 locals[]; } deformation;
layout(set=1,binding=0,std140) uniform View {
    vec4 eye, right, up, forward, projection, translation, quaternion, textureOffset, scale, color;
    uvec4 bases; // x: palette base (vec4 index), y: deformation base.
} u;
vec3 rotate(vec4 q, vec3 v) { return v+2.0*cross(q.xyz,cross(q.xyz,v)+q.w*v); }
void main() {
    uint base = u.bases.x;
    vec4 rotation = palette.bones[base+nodes.x*2u];
    vec3 bone = palette.bones[base+nodes.x*2u+1u].xyz;
    vec3 local = deform == 0xffffffffu ? position : deformation.locals[u.bases.y+deform].xyz;
    vec3 model = (bone+rotate(rotation,local))*u.scale.xyz;
    vec3 world = rotate(u.quaternion,model)+u.translation.xyz;
    vec3 relative = world-u.eye.xyz;
    float z = dot(relative,u.forward.xyz);
    gl_Position = vec4(dot(relative,u.right.xyz)*u.projection.x,
                       dot(relative,u.up.xyz)*u.projection.y,
                       z*u.projection.z+u.projection.w,z);
    // A face with any hidden corner node is omitted: every corner collapses identically.
    if (palette.bones[base+nodes.y*2u+1u].w+palette.bones[base+nodes.z*2u+1u].w+
        palette.bones[base+nodes.w*2u+1u].w > 0.0)
        gl_Position = vec4(0.0);
    worldPosition = world;
    faceColor = u.color.xyz;
    texCoord = uv+u.textureOffset.xy;
}
