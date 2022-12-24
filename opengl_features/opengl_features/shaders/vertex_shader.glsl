#version 330 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec2 vTexCoord;

uniform mat4 projection;
uniform mat4 model;
uniform mat4 view;


//out vec4 fColor;
out vec2 fTexCoord;

void main()
{
	gl_Position = projection * view * model * vec4(position, 1.0f);

	fTexCoord = vTexCoord;
}