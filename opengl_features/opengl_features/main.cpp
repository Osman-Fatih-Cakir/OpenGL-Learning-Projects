
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <vector>
#include <map>
#include <iostream>

#include <shaders.h>
#include <meshes.h>

int WIDTH = 1200;
int HEIGHT = 800;

typedef glm::mat3 mat3;
typedef glm::mat4 mat4;
typedef glm::vec3 vec3;
typedef glm::vec4 vec4;

GLuint framebuffer;
GLuint quadVAO, textureColorbuffer, rbo;
GLfloat quad_vertices[] = {
	// positions   // texCoords
	-1.0f,  1.0f,  0.0f, 1.0f,
	-1.0f, -1.0f,  0.0f, 0.0f,
	 1.0f, -1.0f,  1.0f, 0.0f,

	-1.0f,  1.0f,  0.0f, 1.0f,
	 1.0f, -1.0f,  1.0f, 0.0f,
	 1.0f,  1.0f,  1.0f, 1.0f
};

int post_process = 0;
GLuint post_process_loc;

GLuint transparentVAO;
std::vector<mat4> transparent_models;
unsigned int transparent_texture;
std::vector<glm::vec3> transparent_positions;
std::map<float, vec3> sorted;

vec3 eye = vec3(0.0f, 2.f, 8.f);
vec3 up = vec3(0.f, 0.5f, -1.f);
mat4 projection, view;
GLuint projection_loc, view_loc;
GLuint s_projection_loc, s_view_loc;

GLuint shader_program, stencil_shader_program, screen_shader_program;

std::vector<GLuint> cubeVAOs;
std::vector<mat4> cube_models;
GLuint floorVAO;
mat4 floor_model;
unsigned int cube_texture;

unsigned int floor_texture;

GLuint depth_mode_loc;
bool depth_mode = false;

bool render_mode = false;

bool allow_draw_transparents = true;

void init();
void changeViewport(int w, int h);
void keyboard(unsigned char key, int x, int y);
unsigned int load_texture(const char* path);
void init_shaders();
void init_stencil_shaders();
void init_framebuffer();
void init_screen_shaders();
void init_camera();
void init_cubes();
void init_floor();
void init_transparent();
void sort_transparent(std::vector<vec3> vector);
void draw_camera();
void draw_cubes();
void draw_scaled_cubes(float scale);
void draw_floor();
void draw_transparent();
void draw_quad();
void stencil_render();
void render();

int main(int argc, char* argv[])
{
	// Initialize GLUT
	glutInit(&argc, argv);

	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);

	// Create window
	glutCreateWindow("OPENGL APP");

	glEnable(GL_DEPTH_TEST); // Enable depth test
	glDepthFunc(GL_LESS); // Default

	glEnable(GL_STENCIL_TEST); // Enable stencil test
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

	glEnable(GL_BLEND); // Enable blending
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

void changeViewport(int w, int h)
{
	glViewport(0, 0, w, h);
}

void keyboard(unsigned char key, int x, int y)
{
	switch (key)
	{
		case 'k':
			depth_mode = !depth_mode;
			glutPostRedisplay();
			break;
		case 'r':
			render_mode = !render_mode;
			if (render_mode)
				glutDisplayFunc(stencil_render);
			else
				glutDisplayFunc(render);
			glutPostRedisplay();
			break;
		case 't':
			allow_draw_transparents = !allow_draw_transparents;
			glutPostRedisplay();
			break;
		case 's':
			post_process = (post_process +1) % 4;
			if (post_process == 0)
				std::cout << "Default" << std::endl;
			else if (post_process == 1)
				std::cout << "Inversion" << std::endl;
			else if (post_process == 2)
				std::cout << "Grayscale" << std::endl;
			else if (post_process == 3)
				std::cout << "Blur" << std::endl;
			glutPostRedisplay();
			break;
		default:
			break;
	}
}

unsigned int load_texture(const char* path)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);

	int width, height, nrComponents;
	unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
	GLenum format;
	if (data)
	{
		if (nrComponents == 1)
			format = GL_RED;
		else if (nrComponents == 3)
			format = GL_RGB;
		else if (nrComponents == 4)
			format = GL_RGBA;

		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(data);
	}
	else
	{
		std::cout << "Texture failed to load at path: " << path << std::endl;
		stbi_image_free(data);
	}

	return textureID;
}

void init()
{
	std::cout << "Press 'k' to see depth values with color of the fragments." << std::endl;;
	std::cout << "Press 'r' to see highligted objects using stencil" << std::endl;;
	std::cout << "Press 't' to toogle draw transparent objects." << std::endl;
	std::cout << "Press 's' to change post process events." << std::endl;
	init_shaders();
	init_stencil_shaders();
	init_screen_shaders();
	init_framebuffer();
	init_camera();
	init_floor();
	init_cubes();
	init_transparent();
}

void init_shaders()
{
	// Initialize shaders
	GLuint vertex_shader = initshaders(GL_VERTEX_SHADER, "shaders/vertex_shader.glsl");
	GLuint fragment_shader = initshaders(GL_FRAGMENT_SHADER, "shaders/fragment_shader.glsl");
	shader_program = initprogram(vertex_shader, fragment_shader);

	depth_mode_loc = glGetUniformLocation(shader_program, "depth_mode");
}

void init_stencil_shaders()
{
	// Initialize shaders
	GLuint vertex_shader = initshaders(GL_VERTEX_SHADER, "shaders/stencil_vs.glsl");
	GLuint fragment_shader = initshaders(GL_FRAGMENT_SHADER, "shaders/stencil_fs.glsl");
	stencil_shader_program = initprogram(vertex_shader, fragment_shader);
}

void init_screen_shaders()
{
	// Initialize shaders
	GLuint vertex_shader = initshaders(GL_VERTEX_SHADER, "shaders/screen_vs.glsl");
	GLuint fragment_shader = initshaders(GL_FRAGMENT_SHADER, "shaders/screen_fs.glsl");
	screen_shader_program = initprogram(vertex_shader, fragment_shader);

	post_process_loc = glGetUniformLocation(screen_shader_program, "post_process");
}

void init_framebuffer()
{
	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	// Create color attachment texture
	glGenTextures(1, &textureColorbuffer);
	glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WIDTH, HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	// Create renderbuffer object attachment for depth and stencil buffers
	glGenRenderbuffers(1, &rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, WIDTH, HEIGHT);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	// Check is buffer complete
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "ERROR: Framebuffer is not complete!" << std::endl;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Create a VAO for screen texture
	glGenVertexArrays(1, &quadVAO);
	glBindVertexArray(quadVAO);

	GLuint VBO;
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 4 * 6, quad_vertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, (void*)(sizeof(GLfloat) * 2));

	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void init_camera()
{
	// Initialize viewing values
	projection = glm::perspective(glm::radians(60.0f), (float)1200 / 800, 0.1f, 100.0f);
	view = glm::lookAt(eye, vec3(0.0f, 0.0f, 0.0f), up);

	projection_loc = glGetUniformLocation(shader_program, "projection");
	view_loc = glGetUniformLocation(shader_program, "view");

	s_projection_loc = glGetUniformLocation(stencil_shader_program, "projection");
	s_view_loc = glGetUniformLocation(stencil_shader_program, "view");
}

void init_floor()
{
	glGenVertexArrays(1, &floorVAO);

	glBindVertexArray(floorVAO);

	// Vertices buffer
	GLuint VBO;
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*4*3, floor_vertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

	// Texture coordinates
	GLuint TBO;
	glGenBuffers(1, &TBO);
	glBindBuffer(GL_ARRAY_BUFFER, TBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 * 6, floor_texture_coord, GL_STATIC_DRAW);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);

	// Element buffer
	GLuint EBO;
	glGenBuffers(1, &EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLshort)*6, floor_elements, GL_STATIC_DRAW);

	// Model matrix of the floor
	floor_model = mat4(1.0);

	floor_texture = load_texture("grey.png"); // Texture

	// Unbind for prevent further modification
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void init_cubes()
{
	float posx = -1.5f; float posz = -1.f;

	for (int i = 0; i < 2; i++)
	{
		GLuint cubeVAO;
		glGenVertexArrays(1, &cubeVAO);
		glBindVertexArray(cubeVAO);

		// Vertices buffer
		GLuint VBO;
		glGenBuffers(1, &VBO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * 36, cube_vertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

		// Texture coordinates
		GLuint TBO;
		glGenBuffers(1, &TBO);
		glBindBuffer(GL_ARRAY_BUFFER, TBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2*36, cube_texture_coord, GL_STATIC_DRAW);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);

		// Model matrix of the floor
		mat4 cube_model = glm::translate(mat4(1.0f), vec3(posx + i * 3, 0.0f, posz + i * 3));

		// Store model matrices and VAO of cube
		cube_models.push_back(cube_model);
		cubeVAOs.push_back(cubeVAO);

		// Unbind for prevent further modification
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}

	cube_texture = load_texture("container.jpg"); // Load texture
}

void init_transparent()
{
	// I store texture coordinates here also
	GLfloat transparent_vertices[] = {
		0.0f,  0.5f,  0.0f,  0.0f,  0.0f,
		0.0f, -0.5f,  0.0f,  0.0f,  1.0f,
		1.0f, -0.5f,  0.0f,  1.0f,  1.0f,

		0.0f,  0.5f,  0.0f,  0.0f,  0.0f,
		1.0f, -0.5f,  0.0f,  1.0f,  1.0f,
		1.0f,  0.5f,  0.0f,  1.0f,  0.0f
	}; 

	transparent_texture = load_texture("window.png");
	transparent_positions.push_back(vec3(-1.5f, -0.5f, 0.1f));
	transparent_positions.push_back(vec3(-1.f, -0.5f, 1.2f));
	transparent_positions.push_back(vec3(-0.5f, -0.5f, 2.f));
	transparent_positions.push_back(vec3(0.75f, -0.5f, 3.1f));

	// The order matters !
	sort_transparent(transparent_positions);

	// Creating transparent object
	glGenVertexArrays(1, &transparentVAO);
	glBindVertexArray(transparentVAO);

	GLuint VBO;
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 5*6, transparent_vertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*5, 0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT) * 5, (void*)(sizeof(GL_FLOAT)*3));
	
	// Model matrices of transparentes
	for (std::map<float, glm::vec3>::reverse_iterator it = sorted.rbegin(); it != sorted.rend(); ++it)
	{
		mat4 model = mat4(1.0f);
		model = glm::translate(model, it->second);
		transparent_models.push_back(model);
	}
}

void sort_transparent(std::vector<vec3> vector)
{
	// Map the transparent objects according to the distance from the camera 
	for (unsigned int i = 0; i < transparent_positions.size(); i++)
	{
		float distance = glm::length(eye - transparent_positions[i]);
		sorted[distance] = transparent_positions[i];
	}
}

void draw_camera()
{
	// Send camera uniforms
	glUniformMatrix4fv(view_loc, 1, GL_FALSE, &(view)[0][0]);
	glUniformMatrix4fv(projection_loc, 1, GL_FALSE, &(projection)[0][0]);
}

void draw_stencil_camera()
{
	// Send camera uniforms
	glUniformMatrix4fv(s_view_loc, 1, GL_FALSE, &(view)[0][0]);
	glUniformMatrix4fv(s_projection_loc, 1, GL_FALSE, &(projection)[0][0]);
}

void draw_floor()
{
	GLuint model_loc = glGetUniformLocation(shader_program, "model");
	glUniformMatrix4fv(model_loc, 1, GL_FALSE, &floor_model[0][0]);

	// Texture
	glActiveTexture(GL_TEXTURE0);
	GLuint texture_loc = glGetUniformLocation(shader_program, "image");
	glUniform1i(texture_loc, 0);
	glBindTexture(GL_TEXTURE_2D, floor_texture);

	glBindVertexArray(floorVAO);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
	glBindVertexArray(0);
}

void draw_cubes()
{
	for (int i = 0; i < 2; i++)
	{
		GLuint model_loc = glGetUniformLocation(shader_program, "model");
		glUniformMatrix4fv(model_loc, 1, GL_FALSE, &(cube_models[i])[0][0]);

		// Texture
		glActiveTexture(GL_TEXTURE0);
		GLuint texture_loc = glGetUniformLocation(shader_program, "image");
		glUniform1i(texture_loc, 0);
		glBindTexture(GL_TEXTURE_2D, cube_texture);

		glBindVertexArray(cubeVAOs[i]);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glBindVertexArray(0);
	}
}

void draw_scaled_cubes(float scale)
{
	for (int i = 0; i < 2; i++)
	{
		GLuint model_loc = glGetUniformLocation(stencil_shader_program, "model");
		mat4 temp_model = glm::scale(cube_models[i], vec3(scale, scale, scale));
		glUniformMatrix4fv(model_loc, 1, GL_FALSE, &temp_model[0][0]);

		glBindVertexArray(cubeVAOs[i]);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glBindVertexArray(0);
	}
}

void draw_transparent()
{
	GLuint model_loc = glGetUniformLocation(shader_program, "model");

	// Draw all transparentes
	glBindVertexArray(transparentVAO);

	// Texture
	glActiveTexture(GL_TEXTURE0);
	GLuint texture_loc = glGetUniformLocation(shader_program, "image");
	glUniform1i(texture_loc, 0);
	glBindTexture(GL_TEXTURE_2D, transparent_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Draw transparent objects with a sequence !
	for (unsigned int i = 0; i < transparent_positions.size(); i++)
	{
		glUniformMatrix4fv(model_loc, 1, GL_FALSE, &(transparent_models[i])[0][0]);

		glDrawArrays(GL_TRIANGLES, 0, 6);
	}
	glBindVertexArray(0);
}

void draw_quad()
{
	
	GLuint texture_loc = glGetUniformLocation(screen_shader_program, "screen_texture");
	glUniform1i(texture_loc, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textureColorbuffer);

	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

void stencil_render()
{
	// Bind framebuffer and draw the scene as we normally would to color texture
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glEnable(GL_DEPTH_TEST); // Enable depth testing (is disabled for rendering screen-space quad)

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glClearColor(0.2f, 0.2f, 0.2f, 1.f);

	glUseProgram(shader_program);
	glUniform1i(depth_mode_loc, (int)depth_mode);

	glStencilMask(0x00); // Make sure we don't update the stencil buffer while drawing the floor
	draw_floor();

	float scale = 1.05f;
	
	// 1st. render pass, draw objects as normal, writing to the stencil buffer
	glStencilFunc(GL_ALWAYS, 1, 0xFF);
	glStencilMask(0xFF); // Enable writing to the stencil buffer

	draw_camera();
	draw_cubes();

	// 2nd. render pass: now draw slightly scaled versions of the objects, this time disabling stencil writing.
	// Because the stencil buffer is now filled with several 1s. The parts of the buffer that are 1 are not drawn, thus only drawing 
	// the objects' size differences, making it look like borders.
	glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
	glStencilMask(0x00); // Disable writing to the stencil buffer
	glDisable(GL_DEPTH_TEST);
	
	glUseProgram(stencil_shader_program);

	draw_stencil_camera();
	draw_scaled_cubes(scale); // Scale the objects for highlight

	glStencilMask(0xFF);
	glStencilFunc(GL_ALWAYS, 1, 0xFF);
	glEnable(GL_DEPTH_TEST); // Enable depth test again for the next frame

	// Bind back to default framebuffer and draw a quad plane with the attached framebuffer color texture
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDisable(GL_DEPTH_TEST); // Disable depth test so screen-space quad isn't discarded due to depth test.
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(screen_shader_program);
	draw_quad();

	glutSwapBuffers();

	glutPostRedisplay(); // Render loop
}

void render()
{
	// Bind framebuffer and draw the scene as we normally would to color texture
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glEnable(GL_DEPTH_TEST); // Enable depth testing (is disabled for rendering screen-space quad)

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glClearColor(0.9f, 0.9f, 0.9f, 1.f);

	glUseProgram(shader_program);

	glUniform1i(depth_mode_loc, (int)depth_mode);
	
	draw_camera();
	draw_floor();
	draw_cubes();

	if (allow_draw_transparents)
		draw_transparent();

	// Bind back to default framebuffer and draw a quad plane with the attached framebuffer color texture
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDisable(GL_DEPTH_TEST); // Disable depth test so screen-space quad isn't discarded due to depth test.
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(screen_shader_program);

	glUniform1i(post_process_loc, post_process);

	draw_quad();

	glutSwapBuffers();

	glutPostRedisplay(); // Render loop
}
