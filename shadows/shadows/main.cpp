
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <vector>
#include <iostream>

#include <meshes.h>
#include <shaders.h>

const int WIDTH = 1280;
const int HEIGHT = 800;
const float aspect = WIDTH / HEIGHT;

typedef glm::mat3 mat3;
typedef glm::mat4 mat4;
typedef glm::vec3 vec3;
typedef glm::vec4 vec4;

bool enable_pcf = true;
bool enable_shadow_acne = false;
bool enable_point_light = false;

// Shader programs
GLuint shader_program, depth_program, cube_depth_program;

// Framebuffers
GLuint depth_map_fbo;
GLuint SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
GLuint depth_cube_map_fbo;

// Light attributes
vec3 light_position = vec3(0.f, 5.f, -5.f);
mat4 light_projection, light_view, light_space_matrix;
GLuint light_space_matrix_loc, light_position_loc;

// Point light attributes
vec3 pointlight_position = vec3(0.f, 3.f, 0.f);
float pointlight_far = 100.f;
mat4 pointlight_projection, pointlight_space_matrices[6];
GLuint pointlight_space_matrix_loc;

// Camera attributes
vec3 eye = vec3(-3.f, 3.f, 1.5f);
vec3 up = vec3(0.f, 1.f, 0.f);
float _near = 0.01f; float _far = 100.f;
mat4 projection, view;
GLuint projection_loc, model_loc, view_loc;

// Objects attributes
std::vector<GLuint> cubeVAOs;
std::vector<mat4> cube_models;
std::vector<vec4> cube_colors;
GLuint floorVAO;
vec4 floor_color = vec4(0.f, 1.f, 0.5f, 1.f);
GLuint color_loc;
float quadVertices[] = {
	// positions        // texture Coords
	-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
	-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
	 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
	 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
};
GLuint quadVAO;

// Textures
GLuint wood_texture, cube_texture;
GLuint depth_map, depth_cube_map;

void init();
void changeViewport(int w, int h);
void keyboard(unsigned char key, int x, int y);

void init_depth_shaders();
void init_point_depth_shaders();
void init_shaders();
void init_depth_map_framebuffer();

void init_light_camera();
void init_pointlight_camera();
void init_camera();
void init_quad();
void init_cubes();
void init_cube(vec3 pos, vec3 rotate, vec3 scale, vec4 color);
void init_floor();

void draw_light_camera();
void draw_pointlight_camera();
void draw_camera(GLuint _shader_program);
void draw_light(GLuint _shader_program);
void draw_cubes(GLuint _shader_program);
void draw_floor(GLuint _shader_program);

void render();

int main(int argc, char* argv[])
{
	// Initialize GLUT
	glutInit(&argc, argv);

	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);

	// Create window
	glutCreateWindow("Shadow Test");
	glEnable(GL_DEPTH_TEST);
	glutInitWindowSize(WIDTH, HEIGHT);
	glutReshapeWindow(WIDTH, HEIGHT);

	// Bind functions
	glutReshapeFunc(changeViewport);
	glutKeyboardFunc(keyboard);
	glutDisplayFunc(render);

	// Initialize Glew
	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
		std::cout << "Unable to initalize Glew ! " << std::endl;
		return 1;
	}

	// Start program
	init();

	glutMainLoop();

	return 0;
}

void init()
{
	std::cout << "'Q' to toggle Precision Closer Filtering\n";
	std::cout << "'T' to see the scene with or without shadow acne\n";
	std::cout << "'R' to toogle between point light shadows and directional light shadows\n";
	init_depth_shaders();
	init_point_depth_shaders();
	init_shaders();
	init_depth_map_framebuffer();

	init_light_camera();
	init_pointlight_camera();
	init_camera();
	init_quad();
	init_cubes();
	init_floor();
}

void changeViewport(int w, int h)
{
	glViewport(0, 0, w, h);
}

// Keyboard inputs
void keyboard(unsigned char key, int x, int y)
{
	switch (key)
	{
	case 'q':
		enable_pcf = !enable_pcf;
		glutPostRedisplay();
		break;
	case 't':
		enable_shadow_acne = !enable_shadow_acne;
		glutPostRedisplay();
		break;
	case 'r':
		enable_point_light = !enable_point_light;
		glutPostRedisplay();
		break;
	}
}

void init_depth_shaders()
{
	// Initialize shaders
	GLuint vertex_shader = initshaders(GL_VERTEX_SHADER, "shaders/depth_vs.glsl");
	GLuint fragment_shader = initshaders(GL_FRAGMENT_SHADER, "shaders/depth_fs.glsl");
	depth_program = initprogram(vertex_shader, fragment_shader);
}

void init_point_depth_shaders()
{
	// Initialize shaders
	GLuint vertex_shader = initshaders(GL_VERTEX_SHADER, "shaders/point_depth_vs.glsl");
	GLuint geometry_shader = initshaders(GL_GEOMETRY_SHADER, "shaders/point_depth_gs.glsl");
	GLuint fragment_shader = initshaders(GL_FRAGMENT_SHADER, "shaders/point_depth_fs.glsl");
	cube_depth_program = initprogram(vertex_shader, geometry_shader, fragment_shader);
}

// The last shader that renders the scene with shadows
void init_shaders()
{
	GLuint vertex_shader = initshaders(GL_VERTEX_SHADER, "shaders/vertex_shader.glsl");
	GLuint fragment_shader = initshaders(GL_FRAGMENT_SHADER, "shaders/fragment_shader.glsl");
	shader_program = initprogram(vertex_shader, fragment_shader);
}

// Initialize depth map framebuffer
void init_depth_map_framebuffer()
{
	//
	//// Directional light shadow
	//
	glGenFramebuffers(1, &depth_map_fbo);
	
	// Create 2D depth texture
	glGenTextures(1, &depth_map);
	glBindTexture(GL_TEXTURE_2D, depth_map);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	// Attach depth texture as FBO's depth buffer
	glBindFramebuffer(GL_FRAMEBUFFER, depth_map_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_map, 0);
	// Set both to "none" because there is no need for color attachment
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	//
	//// Point light shadow
	//
	glGenFramebuffers(1, &depth_cube_map_fbo);
	glGenTextures(1, &depth_cube_map);
	glBindTexture(GL_TEXTURE_CUBE_MAP, depth_cube_map);

	// Create 6 2D depth texture framebuffers for genertate a cubemap
	for (int i = 0; i < 6; i++)
	{
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	// Attach depth texture as FBO's depth buffer
	glBindFramebuffer(GL_FRAMEBUFFER, depth_cube_map_fbo);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depth_cube_map, 0);
	// Set both to "none" because there is no need for color attachment
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

}

// Initialize the camera from lights perspective
void init_light_camera()
{
	light_space_matrix_loc = glGetUniformLocation(depth_program, "light_space_matrix");

	light_projection = glm::ortho(-5.f, 5.f, -5.f, 5.f, 0.1f, 10.f);
	light_view = glm::lookAt(light_position, vec3(0.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f));

	light_space_matrix = light_projection * light_view;
}

// Initialize the camera from point lights perspective
void init_pointlight_camera()
{
	// Uniform variables
	light_position_loc = glGetUniformLocation(cube_depth_program, "light_position");

	// Projection matrix of the point light is the same
	pointlight_space_matrix_loc = glGetUniformLocation(cube_depth_program, "light_space_matrix");
	pointlight_projection = glm::perspective(glm::radians(90.f), (float)SHADOW_WIDTH/SHADOW_HEIGHT, 0.1f, pointlight_far);

	// Space matrices
	pointlight_space_matrices[0] = pointlight_projection * glm::lookAt(pointlight_position, pointlight_position + vec3(1.f, 0.f, 0.f), vec3(0.f, -1.f, 0.f));
	pointlight_space_matrices[1] = pointlight_projection * glm::lookAt(pointlight_position, pointlight_position + vec3(-1.f, 0.f, 0.f), vec3(0.f, -1.f, 0.f));
	pointlight_space_matrices[2] = pointlight_projection * glm::lookAt(pointlight_position, pointlight_position + vec3(0.f, 1.f, 0.f), vec3(0.f, 0.f, 1.f));
	pointlight_space_matrices[3] = pointlight_projection * glm::lookAt(pointlight_position, pointlight_position + vec3(0.f, -1.f, 0.f), vec3(0.f, 0.f, -1.f));
	pointlight_space_matrices[4] = pointlight_projection * glm::lookAt(pointlight_position, pointlight_position + vec3(0.f, 0.f, 1.f), vec3(0.f, -1.f, 0.f));
	pointlight_space_matrices[5] = pointlight_projection * glm::lookAt(pointlight_position, pointlight_position + vec3(0.f, 0.f, -1.f), vec3(0.f, -1.f, 0.f));
}

// Initialize the camera variables
void init_camera()
{
	projection = glm::perspective(glm::radians(90.f), (float)WIDTH / HEIGHT, _near, _far);
	view = glm::lookAt(eye, vec3(0.f, 0.f, 0.f), up);
}

// Initalize the quad that we draw on it
void init_quad()
{
	glGenVertexArrays(1, &quadVAO);

	// Setup quad VAO
	GLuint quadVBO;
	glGenBuffers(1, &quadVBO);
	glBindVertexArray(quadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

	glBindVertexArray(0);
}

// Initialize cube VAOs and models
void init_cubes()
{
	init_cube(vec3(0.f, 0.5f, 1.f), vec3(0.f, 45.f, 0.f), vec3(1.f, 1.f, 1.f), vec4(0.5f, 0.f, 1.f, 1.f));
	init_cube(vec3(3.f, 1.f, 1.5f), vec3(45.f, 45.f, 45.f), vec3(1.f, 1.f, 1.f), vec4(1.f, 0.5f, 0.f, 1.f));
	init_cube(vec3(0.f, 1.5f, -1.5f), vec3(0.f, 45.f, 0.f), vec3(1.f, 1.f, 1.f), vec4(1.f, 0.f, 0.5f, 1.f));
}

void init_cube(vec3 pos, vec3 rotate, vec3 scale, vec4 color)
{
	GLuint VAO;
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	// Vertices buffer
	GLuint VBO;
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * 36, cube_vertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

	// Normals buffer
	GLuint NBO;
	glGenBuffers(1, &NBO);
	glBindBuffer(GL_ARRAY_BUFFER, NBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * 36, cube_normals, GL_STATIC_DRAW);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

	cubeVAOs.push_back(VAO); // VAO

	cube_colors.push_back(color);// Color

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0); // Prevent further modifications

	// Model matrix of cube
	mat4 model = glm::translate(mat4(1.f), vec3(pos.x, pos.y, pos.z));
	model = glm::scale(model, vec3(scale.x, scale.y, scale.z));
	model = glm::rotate(model, glm::radians(rotate.x), vec3(1.f, 0.f, 0.f));
	model = glm::rotate(model, glm::radians(rotate.y), vec3(0.f, 1.f, 0.f));
	model = glm::rotate(model, glm::radians(rotate.z), vec3(0.f, 0.f, 1.f));

	cube_models.push_back(model);
}

// Initalize floor VAO
void init_floor()
{
	glGenVertexArrays(1, &floorVAO);
	glBindVertexArray(floorVAO);

	// Vertices buffer
	GLuint VBO;
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * 6, floor_vertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

	// Normals buffer
	GLuint NBO;
	glGenBuffers(1, &NBO);
	glBindBuffer(GL_ARRAY_BUFFER, NBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * 6, floor_normals, GL_STATIC_DRAW);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0); // Prevent further modifications
}

// Draw camera from light perspective
void draw_light_camera()
{
	glUniformMatrix4fv(light_space_matrix_loc, 1, GL_FALSE, &(light_space_matrix)[0][0]);
}

// Draw camera from point light perspective
void draw_pointlight_camera()
{
	GLuint far_loc = glGetUniformLocation(cube_depth_program, "far");

	glUniform1f(far_loc, pointlight_far);
	glUniform3fv(light_position_loc, 1, &pointlight_position[0]);
	glUniformMatrix4fv(pointlight_space_matrix_loc, 6, GL_FALSE, &(pointlight_space_matrices)[0][0][0]);
}

// Send camera variables to shader
void draw_camera(GLuint _shader_program)
{
	projection_loc = glGetUniformLocation(_shader_program, "projection");
	view_loc = glGetUniformLocation(_shader_program, "view");

	glUniformMatrix4fv(projection_loc, 1, GL_FALSE, &projection[0][0]);
	glUniformMatrix4fv(view_loc, 1, GL_FALSE, &view[0][0]);
}

// Send light and shadow variables to shader
void draw_light(GLuint _shader_program)
{
	GLuint light_pos_loc = glGetUniformLocation(_shader_program, "light_pos");
	GLuint pointlight_pos_loc = glGetUniformLocation(_shader_program, "pointlight_pos");
	GLuint eye_loc = glGetUniformLocation(_shader_program, "eye");
	GLuint lsm_loc = glGetUniformLocation(_shader_program, "light_space_matrix");
	GLuint shadow_map_loc = glGetUniformLocation(_shader_program, "directional_shadow_map");
	GLuint shadow_cubemap_loc = glGetUniformLocation(_shader_program, "point_shadow_map");

	GLuint pointlight_loc = glGetUniformLocation(_shader_program, "enable_point_light");
	GLuint enable_pcf_loc = glGetUniformLocation(_shader_program, "enable_pcf");
	GLuint enable_shadow_acne_loc = glGetUniformLocation(_shader_program, "enable_shadow_acne");
	
	GLuint far_loc = glGetUniformLocation(_shader_program, "far");
	 
	glUniform3fv(light_pos_loc, 1, &light_position[0]);
	glUniform3fv(pointlight_pos_loc, 1, &pointlight_position[0]);
	glUniform3fv(eye_loc, 1, &eye[0]);
	glUniformMatrix4fv(lsm_loc, 1, GL_FALSE, &light_space_matrix[0][0]);

	glUniform1i(pointlight_loc, (int)enable_point_light);
	glUniform1i(enable_pcf_loc, (int)enable_pcf);
	glUniform1i(enable_shadow_acne_loc, (int)enable_shadow_acne);
	glUniform1f(far_loc, pointlight_far);

	// Shadow map
	glUniform1i(shadow_map_loc, 0);
	glUniform1i(shadow_cubemap_loc, 1);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, depth_map);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_CUBE_MAP, depth_cube_map);
}

// Draw cubes
void draw_cubes(GLuint _shader_program)
{
	for (unsigned int i = 0; i < cubeVAOs.size(); i++)
	{
		// Model matrix
		model_loc = glGetUniformLocation(_shader_program, "model");
		glUniformMatrix4fv(model_loc, 1, GL_FALSE, &(cube_models[i])[0][0]);

		// Color
		color_loc = glGetUniformLocation(_shader_program, "fColor");
		glUniform4fv(color_loc, 1, &(cube_colors[i])[0]);

		// Draw
		glBindVertexArray(cubeVAOs[i]);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glBindVertexArray(0);
	}
}

// Draw floor
void draw_floor(GLuint _shader_program)
{
	// Model matrix
	mat4 model = glm::scale(mat4(1.f), vec3(10.f, 10.f, 10.f));
	model_loc = glGetUniformLocation(_shader_program, "model");
	glUniformMatrix4fv(model_loc, 1, GL_FALSE, &model[0][0]);

	// Color
	color_loc = glGetUniformLocation(_shader_program, "fColor");
	glUniform4fv(color_loc, 1, &(floor_color)[0]);

	// Draw
	glBindVertexArray(floorVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

// Draw the scene
void render()
{
	if (enable_point_light) // Point light shadows
	{
		// First pass, render to depth map (from light's perspective)
		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depth_cube_map_fbo);
		glClear(GL_DEPTH_BUFFER_BIT);

		glUseProgram(cube_depth_program);
		draw_pointlight_camera();
		draw_cubes(cube_depth_program);
		draw_floor(cube_depth_program);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// Second pass, render scene with using depth_map for shadows
		glViewport(0, 0, WIDTH, HEIGHT);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(shader_program);

		draw_camera(shader_program);
		draw_light(shader_program);
		draw_cubes(shader_program);
		draw_floor(shader_program);
	}
	else // Directional light shadows
	{
		// First pass, render to depth map (from light's perspective)
		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depth_map_fbo);
		glClear(GL_DEPTH_BUFFER_BIT);

		glUseProgram(depth_program);
		draw_light_camera();
		draw_cubes(depth_program);
		draw_floor(depth_program);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// Second pass, render scene with using depth_map for shadows
		glViewport(0, 0, WIDTH, HEIGHT);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(shader_program);

		draw_camera(shader_program);
		draw_light(shader_program);
		draw_cubes(shader_program);
		draw_floor(shader_program);
	}

	glutSwapBuffers();
	GLuint err = glGetError(); if (err) fprintf(stderr, "%s\n", gluErrorString(err));

	glutPostRedisplay(); // For render loop
}
