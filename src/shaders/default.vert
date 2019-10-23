R"zzz(
#version 330 core
uniform vec4 light_position;
uniform vec3 camera_position;
uniform mat4 projection;
uniform mat4 model;
uniform mat4 view;
uniform bool render_outline;
uniform float outline_size;
layout(location = 0) in vec3 vertex_position;
layout(location = 1) in vec2 vertex_uv;
layout(location = 2) in vec3 vertex_normal;
out vec4 world_normal;
out vec4 light_direction;
out vec4 world_position;
out vec4 normal;
out vec2 uv;
out vec4 camera_direction;
void main() {
	//Transform vertex into clipping coordinates
	if (render_outline) {
		gl_Position = projection * view * model * vec4(vertex_position + vertex_normal * outline_size, 1.0);
	}
	else {
		gl_Position = projection * view * model * vec4(vertex_position, 1.0);
	}
	world_position = model * vec4(vertex_position, 1.0); //ok

    light_direction = light_position - gl_Position;
    camera_direction = vec4(0.0, 0.0, 0.0, 1.0) - gl_Position;

    normal = vec4(vertex_normal, 1.0);
    //normal = model * vec4(vertex_normal, 1.0);
	uv = vertex_uv; //ok
}
)zzz"
