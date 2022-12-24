#version 330 core
layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormal;

out VS_OUT {
    vec3 fPos;
    vec3 fNormal;
    vec4 fPos_light_space;
} vs_out;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat4 light_space_matrix;

void main()
{
    vs_out.fPos = vec3(model * vec4(vPos, 1.0));
    vs_out.fNormal = transpose(inverse(mat3(model))) * vNormal; // Inefficient
    vs_out.fPos_light_space = light_space_matrix * vec4(vs_out.fPos, 1.0);
    gl_Position = projection * view * model * vec4(vPos, 1.0);
}