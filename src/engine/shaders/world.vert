#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 color;
layout(location=0) out vec3 faceColor;
layout(set=1,binding=0,std140) uniform View {
    vec4 eye, right, up, forward, projection, translation, quaternion;
} u;
void main() {
    vec3 q=u.quaternion.xyz;
    vec3 world=position+2.0*cross(q,cross(q,position)+u.quaternion.w*position)+u.translation.xyz;
    vec3 relative=world-u.eye.xyz;
    float z=dot(relative,u.forward.xyz);
    gl_Position=vec4(dot(relative,u.right.xyz)*u.projection.x,
                     dot(relative,u.up.xyz)*u.projection.y,
                     z*u.projection.z+u.projection.w,z);
    faceColor=color;
}
