#include <GL/glew.h>
#include <dirent.h>

#include "config.h"
#include "gui.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <stdio.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <GLFW/glfw3.h>

#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/io.hpp>
#include <glm/glm.hpp>
#include <debuggl.h>

int window_width = 1200, window_height = 900;
const std::string window_title = "non-photorealistic rendering";

const char* vertex_shader =
#include "shaders/default.vert"
;

const char* geometry_shader =
#include "shaders/default.geom"
;

const char* fragment_shader =
#include "shaders/default.frag"
;

const char* floor_fragment_shader =
#include "shaders/floor.frag"
;

// FIXME: Add more shaders here.

using namespace std;

void ErrorCallback(int error, const char* description) {
	std::cerr << "GLFW Error: " << description << "\n";
}

GLFWwindow* init_glefw()
{
	if (!glfwInit())
		exit(EXIT_FAILURE);
	glfwSetErrorCallback(ErrorCallback);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SAMPLES, 4);
	auto ret = glfwCreateWindow(window_width, window_height, window_title.data(), nullptr, nullptr);
	CHECK_SUCCESS(ret != nullptr);
	glfwMakeContextCurrent(ret);
	glewExperimental = GL_TRUE;
	CHECK_SUCCESS(glewInit() == GLEW_OK);
	glGetError();  // clear GLEW's error for it
	glfwSwapInterval(1);
	const GLubyte* renderer = glGetString(GL_RENDERER);  // get renderer string
	const GLubyte* version = glGetString(GL_VERSION);    // version as a string
	std::cout << "Renderer: " << renderer << "\n";
	std::cout << "OpenGL version supported:" << version << "\n";

	return ret;
}

bool loadOBJ(
	const char * path, 
	std::vector<glm::vec3> & out_vertices, 
	std::vector<glm::vec2> & out_uvs,
	std::vector<glm::vec3> & out_normals
){
	printf("Loading OBJ file %s...\n", path);

	std::vector<unsigned int> vertexIndices, uvIndices, normalIndices;
	std::vector<glm::vec3> temp_vertices; 
	std::vector<glm::vec2> temp_uvs;
	std::vector<glm::vec3> temp_normals;


	FILE * file = fopen(path, "r");
	if( file == NULL ){
		printf("Impossible to open the file ! Are you in the right path ? See Tutorial 1 for details\n");
		getchar();
		return false;
	}

	while( 1 ){

		char lineHeader[128];
		// read the first word of the line
		int res = fscanf(file, "%s", lineHeader);
		if (res == EOF)
			break; // EOF = End Of File. Quit the loop.

		// else : parse lineHeader
		
		if (strcmp( lineHeader, "v" ) == 0){
			glm::vec3 vertex;
			fscanf(file, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z );
			temp_vertices.push_back(vertex);
		} else if (strcmp( lineHeader, "vt" ) == 0){
			glm::vec2 uv;
			fscanf(file, "%f %f\n", &uv.x, &uv.y );
			uv.y = -uv.y; // Invert V coordinate since we will only use DDS texture, which are inverted. Remove if you want to use TGA or BMP loaders.
			temp_uvs.push_back(uv);
		} else if ( strcmp( lineHeader, "vn" ) == 0 ){
			glm::vec3 normal;
			fscanf(file, "%f %f %f\n", &normal.x, &normal.y, &normal.z );
			temp_normals.push_back(normal);
		} else if ( strcmp( lineHeader, "f" ) == 0 ){
			std::string vertex1, vertex2, vertex3;
			unsigned int vertexIndex[3], uvIndex[3], normalIndex[3];
			int matches = fscanf(file, "%d/%d/%d %d/%d/%d %d/%d/%d\n", &vertexIndex[0], &uvIndex[0], &normalIndex[0], &vertexIndex[1], &uvIndex[1], &normalIndex[1], &vertexIndex[2], &uvIndex[2], &normalIndex[2] );
			if (matches != 9){
				printf("File can't be read by our simple parser :-( Try exporting with other options\n");
				fclose(file);
				return false;
			}
			vertexIndices.push_back(vertexIndex[0]);
			vertexIndices.push_back(vertexIndex[1]);
			vertexIndices.push_back(vertexIndex[2]);
			uvIndices    .push_back(uvIndex[0]);
			uvIndices    .push_back(uvIndex[1]);
			uvIndices    .push_back(uvIndex[2]);
			normalIndices.push_back(normalIndex[0]);
			normalIndices.push_back(normalIndex[1]);
			normalIndices.push_back(normalIndex[2]);
		} else{
			// Probably a comment, eat up the rest of the line
			char stupidBuffer[1000];
			fgets(stupidBuffer, 1000, file);
		}
	}

	// For each vertex of each triangle
	for( unsigned int i=0; i<vertexIndices.size(); i++ ){
		// Get the indices of its attributes
		unsigned int vertexIndex = vertexIndices[i];
		unsigned int uvIndex = uvIndices[i];
		unsigned int normalIndex = normalIndices[i];
		
		// Get the attributes thanks to the index
		glm::vec3 vertex = temp_vertices[ vertexIndex-1 ];
		glm::vec2 uv = temp_uvs[ uvIndex-1 ];
		glm::vec3 normal = temp_normals[ normalIndex-1 ];
		
		// Put the attributes in buffers
		out_vertices.push_back(vertex);
		out_uvs     .push_back(uv);
		out_normals .push_back(normal);
	}
	fclose(file);
	return true;
}

unsigned char * loadBMP(const char * imagepath, unsigned int& width, unsigned int& height){

	printf("Reading image %s\n", imagepath);

	// Data read from the header of the BMP file
	unsigned char header[54];
	unsigned int dataPos;
	unsigned int imageSize;

	// Actual RGB data
	unsigned char * data;

	// Open the file
	FILE * file = fopen(imagepath,"rb");
	if (!file){
		printf("%s could not be opened.\n", imagepath);
		getchar();
		return 0;
	}

	// Read the header, i.e. the 54 first bytes

	// If less than 54 bytes are read, problem
	if ( fread(header, 1, 54, file)!=54 ){ 
		printf("Not a correct BMP file\n");
		fclose(file);
		return 0;
	}
	// A BMP files always begins with "BM"
	if ( header[0]!='B' || header[1]!='M' ){
		printf("Not a correct BMP file\n");
		fclose(file);
		return 0;
	}
	// Make sure this is a 24bpp file
	if ( *(int*)&(header[0x1E])!=0  )         {printf("Not a correct BMP file\n");    fclose(file); return 0;}
	if ( *(int*)&(header[0x1C])!=24 )         {printf("Not a correct BMP file\n");    fclose(file); return 0;}

	// Read the information about the image
	dataPos    = *(int*)&(header[0x0A]);
	imageSize  = *(int*)&(header[0x22]);
	width      = *(int*)&(header[0x12]);
	height     = *(int*)&(header[0x16]);

	// Some BMP files are misformatted, guess missing information
	if (imageSize==0)    imageSize=width*height*3; // 3 : one byte for each Red, Green and Blue component
	if (dataPos==0) dataPos=54; // The BMP header is done that way
	
	// Create a buffer
	data = new unsigned char [imageSize];

	// Read the actual data from the file into the buffer
	fread(data,1,imageSize,file);

	// Everything is in memory now, the file can be closed.
	fclose (file);
	return data;
}

int main(int argc, char* argv[])
{
	if (argc < 2) {
		std::cerr << "Input model file is missing" << std::endl;
		std::cerr << "Usage: " << argv[0] << " <PMD file>" << std::endl;
		return -1;
	}
	GLFWwindow *window = init_glefw();
	GUI gui(window);

	//dk what this is
	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);

	std::vector<glm::vec3> vertices;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> normals;
	bool res = loadOBJ(argv[1], vertices, uvs, normals);

	unsigned int width, height;
	unsigned char * data = loadBMP(argv[2], width, height);

	//load into VBOS
	GLuint vertexbuffer;
	glGenBuffers(1, &vertexbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), &vertices[0], GL_STATIC_DRAW);

	GLuint uvbuffer;
	glGenBuffers(1, &uvbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
	glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), &uvs[0], GL_STATIC_DRAW);

	GLuint normalbuffer;
	glGenBuffers(1, &normalbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
	glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), &normals[0], GL_STATIC_DRAW);

	// Setup vertex shader.
	GLuint vertex_shader_id = 0;
	const char* vertex_source_pointer = vertex_shader;
	CHECK_GL_ERROR(vertex_shader_id = glCreateShader(GL_VERTEX_SHADER));
	CHECK_GL_ERROR(glShaderSource(vertex_shader_id, 1, &vertex_source_pointer, nullptr));
	glCompileShader(vertex_shader_id);
	CHECK_GL_SHADER_ERROR(vertex_shader_id);

	// Setup fragment shader.
	GLuint fragment_shader_id = 0;
	const char* fragment_source_pointer = fragment_shader;
	CHECK_GL_ERROR(fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER));
	CHECK_GL_ERROR(glShaderSource(fragment_shader_id, 1, &fragment_source_pointer, nullptr));
	glCompileShader(fragment_shader_id);
	CHECK_GL_SHADER_ERROR(fragment_shader_id);

	//load textures
	GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	//Let's create our program.
	GLuint program_id = 0;
	CHECK_GL_ERROR(program_id = glCreateProgram());
	CHECK_GL_ERROR(glAttachShader(program_id, vertex_shader_id));
	CHECK_GL_ERROR(glAttachShader(program_id, fragment_shader_id));
	//CHECK_GL_ERROR(glAttachShader(program_id, geometry_shader_id));

	// Bind attributes.
	CHECK_GL_ERROR(glBindAttribLocation(program_id, 0, "vertex_position"));
	CHECK_GL_ERROR(glBindAttribLocation(program_id, 1, "vertex_uv"));
	CHECK_GL_ERROR(glBindAttribLocation(program_id, 2, "vertex_normal"));
	CHECK_GL_ERROR(glBindFragDataLocation(program_id, 0, "fragment_color"));
	glLinkProgram(program_id);
	CHECK_GL_PROGRAM_ERROR(program_id);

	// Get the uniform locations.
	GLint projection_matrix_location = 0;
	CHECK_GL_ERROR(projection_matrix_location =
			glGetUniformLocation(program_id, "projection"));
	GLint view_matrix_location = 0;
	CHECK_GL_ERROR(view_matrix_location =
			glGetUniformLocation(program_id, "view"));
	GLint model_matrix_location = 0;
	CHECK_GL_ERROR(model_matrix_location =
			glGetUniformLocation(program_id, "model"));
	GLint light_position_location = 0;
	CHECK_GL_ERROR(light_position_location =
			glGetUniformLocation(program_id, "light_position"));
	GLint camera_position_location = 0;
	CHECK_GL_ERROR(camera_position_location =
			glGetUniformLocation(program_id, "camera_position"));

	//color uniforms
	GLint diffuse_color_location = 0;
	CHECK_GL_ERROR(diffuse_color_location =
			glGetUniformLocation(program_id, "diffuse_color"));
	GLint ambient_color_location = 0;
	CHECK_GL_ERROR(ambient_color_location =
			glGetUniformLocation(program_id, "ambient_color"));
	GLint specular_color_location = 0;
	CHECK_GL_ERROR(specular_color_location =
			glGetUniformLocation(program_id, "specular_color"));
	GLint warm_color_location = 0;
	CHECK_GL_ERROR(warm_color_location =
			glGetUniformLocation(program_id, "warm_color"));
	GLint cool_color_location = 0;
	CHECK_GL_ERROR(cool_color_location =
			glGetUniformLocation(program_id, "cool_color"));
	GLint light_color_location = 0;
	CHECK_GL_ERROR(light_color_location =
			glGetUniformLocation(program_id, "light_color"));

	//float/int uniforms
	GLint warm_amount_location = 0;
	CHECK_GL_ERROR(warm_amount_location =
			glGetUniformLocation(program_id, "warm_amount"));
	GLint cool_amount_location = 0;
	CHECK_GL_ERROR(cool_amount_location =
			glGetUniformLocation(program_id, "cool_amount"));
	GLint num_colors_location = 0;
	CHECK_GL_ERROR(num_colors_location =
			glGetUniformLocation(program_id, "num_colors"));
	GLint outline_size_location = 0;
	CHECK_GL_ERROR(outline_size_location =
			glGetUniformLocation(program_id, "outline_size"));
	GLint shininess_location = 0;
	CHECK_GL_ERROR(shininess_location =
			glGetUniformLocation(program_id, "shininess"));
	GLint ka_location = 0;
	CHECK_GL_ERROR(ka_location =
			glGetUniformLocation(program_id, "ka"));
	GLint kd_location = 0;
	CHECK_GL_ERROR(kd_location =
			glGetUniformLocation(program_id, "kd"));
	GLint ks_location = 0;
	CHECK_GL_ERROR(ks_location =
			glGetUniformLocation(program_id, "ks"));

	//boolean uniforms 
	GLint render_outline_location = 0;
	CHECK_GL_ERROR(render_outline_location =
			glGetUniformLocation(program_id, "render_outline"));
	GLint cel_shade_location = 0;
	CHECK_GL_ERROR(cel_shade_location =
			glGetUniformLocation(program_id, "cel_shade"));
	GLint gooch_shade_location = 0;
	CHECK_GL_ERROR(gooch_shade_location =
			glGetUniformLocation(program_id, "gooch_shade"));
	GLint hatch_shade_location = 0;
	CHECK_GL_ERROR(hatch_shade_location =
			glGetUniformLocation(program_id, "hatch_shade"));
	GLint on_white_location = 0;
	CHECK_GL_ERROR(on_white_location =
			glGetUniformLocation(program_id, "on_white"));
	GLint on_flat_location = 0;
	CHECK_GL_ERROR(on_flat_location =
			glGetUniformLocation(program_id, "on_flat"));
	GLint texture_hatch_location = 0;
	CHECK_GL_ERROR(texture_hatch_location =
			glGetUniformLocation(program_id, "texture_hatch"));

	//texture uniform
	GLint texture_location = 0;
	CHECK_GL_ERROR(texture_location =
			glGetUniformLocation(program_id, "hatching_texture"));

	glm::vec4 light_position = glm::vec4(13.0f, 18.0f, 20.0f, 1.0f);
	bool draw_outline = false;
	bool outline_hold = false;
	bool cel_shaded = false;
	bool gooch_shaded = false;
	bool hatch_shaded = false;
	bool on_white = false;
	bool on_flat = false;
	bool texture_hatch = false;

	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();
	ImGui::StyleColorsDark();

	glm::vec4 diffuse_color = glm::vec4(0.3, 1.0, 1.0, 1.0);
	glm::vec4 light_color = glm::vec4(1.0, 0.0, 1.0, 1.0);
	glm::vec4 ambient_color = glm::vec4(0.1, 0.1, 0.1, 1.0);
	glm::vec4 specular_color = glm::vec4(1.0, 1.0, 1.0, 1.0);
	glm::vec4 warm_color = glm::vec4(0.4, 0.4, 0.0, 1.0);
	glm::vec4 cool_color = glm::vec4(0.0, 0.0, 0.4, 1.0);
	glm::vec4 background_color = glm::vec4(0.9f, 0.9f, 0.8f, 0.0f);
	float warm_amount = 0.75;
	float cool_amount = 0.25;
	int num_colors = 4;
	float outline_size = 0.04;
	float shininess = 32.0;
	float ka = 0.5;
	float kd = 1.0;
	float ks = 1.0;
	delete[] data;

	while (!glfwWindowShouldClose(window)) {
		// Setup some basic window stuff.
		glfwGetFramebufferSize(window, &window_width, &window_height);
		glViewport(0, 0, window_width, window_height);
		glClearColor(background_color.x, background_color.y, background_color.z, background_color.a);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_MULTISAMPLE);
		glEnable(GL_BLEND);
		glEnable(GL_CULL_FACE);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDepthFunc(GL_LESS);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glCullFace(GL_BACK);

		ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Use our program.
		CHECK_GL_ERROR(glUseProgram(program_id));

		gui.updateMatrices();

		glm::mat4 projection_matrix = gui.getProjection();
		glm::mat4 view_matrix = gui.getView();
		glm::mat4 model_matrix = gui.getModel();
		glm::vec3 camera_position = gui.getCamera();
		//std::cout << "camera pos " << glm::to_string(camera_position) << endl;

		// Pass uniforms in.
		CHECK_GL_ERROR(glUniformMatrix4fv(projection_matrix_location, 1, GL_FALSE,
					&projection_matrix[0][0]));
		CHECK_GL_ERROR(glUniformMatrix4fv(view_matrix_location, 1, GL_FALSE,
					&view_matrix[0][0]));
		CHECK_GL_ERROR(glUniformMatrix4fv(model_matrix_location, 1, GL_FALSE,
					&model_matrix[0][0]));
		CHECK_GL_ERROR(glUniform4fv(light_position_location, 1, &light_position[0]));
		CHECK_GL_ERROR(glUniform3fv(camera_position_location, 1, &camera_position[0]));

		//set bool uniforms
		CHECK_GL_ERROR(glUniform1i(cel_shade_location, cel_shaded));
		CHECK_GL_ERROR(glUniform1i(gooch_shade_location, gooch_shaded));
		CHECK_GL_ERROR(glUniform1i(hatch_shade_location, hatch_shaded));
		CHECK_GL_ERROR(glUniform1i(texture_hatch_location, texture_hatch));

		//texture uniform
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture);
		CHECK_GL_ERROR(glUniform1i(texture_location, 0));

		// 1st attribute buffer : vertices
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glVertexAttribPointer(
			0,                  // attribute
			3,                  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
		);

		// 2nd attribute buffer : UVs
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
		glVertexAttribPointer(
			1,                                // attribute
			2,                                // size
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
		);

		// 3rd attribute buffer : normals
		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
		glVertexAttribPointer(
			2,                                // attribute
			3,                                // size
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
		);

		if (outline_hold) {

			draw_outline = true;
			//render outline
			glCullFace(GL_FRONT);
			glDepthMask(GL_TRUE);
			//add new uniforms
			glm::vec4 outline_color = glm::vec4(0.0, 0.0, 0.0, 1.0);
			CHECK_GL_ERROR(glUniform4fv(diffuse_color_location, 1, &outline_color[0]));
			CHECK_GL_ERROR(glUniform1f(outline_size_location, outline_size));
			CHECK_GL_ERROR(glUniform1i(render_outline_location, draw_outline));

			// draw the triangles !
			glDrawArrays(GL_TRIANGLES, 0, vertices.size());

			draw_outline = false;
		}
		//render normal
		glCullFace(GL_BACK);

		//add new uniforms
		CHECK_GL_ERROR(glUniform4fv(diffuse_color_location, 1, &diffuse_color[0]));
		CHECK_GL_ERROR(glUniform4fv(ambient_color_location, 1, &ambient_color[0]));
		CHECK_GL_ERROR(glUniform4fv(specular_color_location, 1, &specular_color[0]));
		CHECK_GL_ERROR(glUniform4fv(light_color_location, 1, &light_color[0]));
		CHECK_GL_ERROR(glUniform1f(ka_location, ka)); 
		CHECK_GL_ERROR(glUniform1f(kd_location, kd)); 
		CHECK_GL_ERROR(glUniform1f(ks_location, ks)); 
		CHECK_GL_ERROR(glUniform1f(shininess_location, shininess));
		//gooch
		CHECK_GL_ERROR(glUniform4fv(warm_color_location, 1, &warm_color[0])); //warm color for gooch shading
		CHECK_GL_ERROR(glUniform4fv(cool_color_location, 1, &cool_color[0])); //cool color for gooch shading
		CHECK_GL_ERROR(glUniform1f(warm_amount_location, warm_amount)); //alpha for gooch shading
		CHECK_GL_ERROR(glUniform1f(cool_amount_location, cool_amount)); //beta for gooch shading
		//cel
		CHECK_GL_ERROR(glUniform1i(num_colors_location, num_colors)); //number of colors for cel shading
		CHECK_GL_ERROR(glUniform1i(render_outline_location, draw_outline)); //render outline (should be false)
		//hatch
		CHECK_GL_ERROR(glUniform1i(on_white_location, on_white)); //flat white bg
		CHECK_GL_ERROR(glUniform1i(on_flat_location, on_flat)); //flat base color bg

		// Draw the triangles !
		glDrawArrays(GL_TRIANGLES, 0, vertices.size());

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);

		{
            ImGui::Begin("shading options");
            ImGui::ColorEdit3("object color", (float *)&diffuse_color);
            ImGui::ColorEdit3("ambient color", (float *)&ambient_color);
            ImGui::ColorEdit3("specular color", (float *)&specular_color);
            ImGui::SliderFloat("kd", &kd, 0.0f, 1.0f);
            ImGui::SliderFloat("ka", &ka, 0.0f, 1.0f);
            ImGui::SliderFloat("ks", &ks, 0.0f, 1.0f);
            ImGui::SliderFloat("shininess", &shininess, 1.0f, 80.0f);
            ImGui::Checkbox("cel shaded", &cel_shaded);
            if (cel_shaded) {
            	ImGui::SliderInt("number of colors", &num_colors, 0, 10);
            }
            ImGui::Checkbox("gooch shaded", &gooch_shaded);
            if (gooch_shaded) {
            	ImGui::ColorEdit3("warm color", (float *)&warm_color);
            	ImGui::ColorEdit3("cool color", (float *)&cool_color);
            	ImGui::SliderFloat("warm amount", &warm_amount, 0.0f, 1.0f);
            	ImGui::SliderFloat("cool amount", &cool_amount, 0.0f, 1.0f);
            }
            ImGui::Checkbox("hatching", &hatch_shaded);
            if (hatch_shaded) {
            	ImGui::Checkbox("flat white", &on_white);
            	ImGui::Checkbox("flat base color", &on_flat);
            }
            ImGui::Checkbox("outline", &outline_hold);
            if (outline_hold) {
            	ImGui::SliderFloat("outline size", &outline_size, 0.0f, 0.05f);
            }
            ImGui::Checkbox("texture", &texture_hatch);
            ImGui::SliderFloat3("light position", &light_position.x, -50.0f, 50.0f);
            ImGui::ColorEdit3("light color", (float *)&light_color);
            ImGui::ColorEdit3("background color", (float *)&background_color);
            //ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
		}

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// Poll and swap.
		glfwPollEvents();
		glfwSwapBuffers(window);
	}

	ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glDeleteBuffers(1, &vertexbuffer);
	glDeleteBuffers(1, &uvbuffer);
	glDeleteBuffers(1, &normalbuffer);
	glDeleteProgram(program_id);
	glDeleteVertexArrays(1, &VertexArrayID);
	glfwDestroyWindow(window);
	glfwTerminate();
	exit(EXIT_SUCCESS);
}
