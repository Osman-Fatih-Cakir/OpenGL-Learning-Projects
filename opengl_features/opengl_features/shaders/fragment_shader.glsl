#version 330 core

//in vec4 fColor;
in vec2 fTexCoord;

uniform sampler2D image;

uniform int depth_mode;

out vec4 out_color;

void main()
{
	if (depth_mode == 0)
	{
		out_color = texture(image, fTexCoord);

		if (out_color.a < 0.1) // If this part of fragment is transparent, discard this fragment
		{
			discard;
		}
	}
	else 
	{
		// Transform non-linear depth values to linear depth values to see the depth values more clear with the color
		float near =  0.1; float far = 100.0; // Same with the perspective function values
		float depth = gl_FragCoord.z;
		float z = depth * 2.0 - 1.0;
		float linear_depth = (2.0 * near * far) / (far + near - z * (far - near));
		linear_depth /= far;

		out_color = vec4(vec3(linear_depth), 1.0);
	}
}