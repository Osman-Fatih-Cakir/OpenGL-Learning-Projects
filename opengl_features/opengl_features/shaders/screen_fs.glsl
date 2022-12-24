#version 330 core

in vec2 fTexCoord;

uniform sampler2D screen_texture;

// post_process:
// 0: default
// 1: inversion
// 2: grayscale
// 3: default
// 4: blur
uniform int post_process;

const float offset = 1.0 / 300.0;

out vec4 out_color;

void main()
{
    vec4 last_color;
    if (post_process == 0) // Default
    {
        vec3 color = texture(screen_texture, fTexCoord).rgb;
        last_color = vec4(color, 1.0);
    }
    else if (post_process == 1) // Inversion
    {
        last_color = vec4(vec3(1.0 - texture(screen_texture, fTexCoord)), 1.0);
    }
    else if (post_process == 2) // Grayscale
    {
        vec4 color = texture(screen_texture, fTexCoord);
        float average = 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b;
        last_color = vec4(average, average, average, 1.0);
    }
    else if (post_process == 3) // Blur
    {
        float kernel[9] = float[](
            1.0 / 16, 2.0 / 16, 1.0 / 16,
            2.0 / 16, 4.0 / 16, 2.0 / 16,
            1.0 / 16, 2.0 / 16, 1.0 / 16  
        );

        vec2 offsets[9] = vec2[](
            vec2(-offset,  offset), // top-left
            vec2( 0.0f,    offset), // top-center
            vec2( offset,  offset), // top-right
            vec2(-offset,  0.0f),   // center-left
            vec2( 0.0f,    0.0f),   // center-center
            vec2( offset,  0.0f),   // center-right
            vec2(-offset, -offset), // bottom-left
            vec2( 0.0f,   -offset), // bottom-center
            vec2( offset, -offset)  // bottom-right    
        );
    
        vec3 sampleTex[9];
        for(int i = 0; i < 9; i++)
        {
            sampleTex[i] = vec3(texture(screen_texture, fTexCoord.st + offsets[i]));
        }

        vec3 _last_color = vec3(0.0);
        for(int i = 0; i < 9; i++)
            _last_color += sampleTex[i] * kernel[i];

        last_color = vec4(_last_color, 1.0);
    }

    out_color = last_color;
}
