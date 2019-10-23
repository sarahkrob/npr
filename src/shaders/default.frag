R"zzz(
#version 330 core
in vec4 light_direction;
in vec4 world_position;
in vec4 normal;
in vec2 uv;
in vec4 camera_direction;
out vec4 fragment_color;
uniform vec4 light_position;
uniform mat4 projection;
uniform mat4 model;
uniform mat4 view;
uniform sampler2D hatching_texture;

//basic shading
uniform vec4 diffuse_color;
uniform vec4 specular_color;
uniform vec4 ambient_color;
uniform vec4 light_color;
uniform bool render_outline;
uniform float ka;
uniform float kd;
uniform float ks;
uniform float shininess;

//for cel shading
uniform bool cel_shade;
uniform int num_colors;

//for gooch shading
uniform bool gooch_shade;
uniform vec4 warm_color;
uniform vec4 cool_color;
uniform float warm_amount;
uniform float cool_amount;

//for hatching
uniform bool hatch_shade;
uniform bool on_white;
uniform bool on_flat;
uniform bool texture_hatch;


void main() {
	vec4 basecolor = diffuse_color;

	vec3 lightdir3 = vec3(light_direction);
	vec3 normal3 = vec3(normal);

	float dot_nl = dot(normalize(lightdir3), normalize(normal3));
	float specpow = 0.0;
	vec3 reflection = reflect(-normalize(lightdir3), normalize(normal3));
	float NdotL = max(dot(normalize(normal3), normalize(lightdir3)), 0.0);
	if (NdotL > 0.0) {
		specpow = pow(max(0.0, dot(reflection, normalize(vec3(world_position)))), shininess);
	}

	//else if (!cel_shade && !gooch_shade && !hatch_shade){
		//BASIC SHADING
		vec3 phongcolor;
		vec3 diffuse = basecolor.xyz;
		vec4 ambient = ambient_color;
		vec4 specular = specular_color;
		dot_nl = clamp(dot_nl, 0.0, 1.0);
		vec4 spec = specular * specpow;
		phongcolor = clamp(kd * dot_nl * diffuse + ka * vec3(ambient) + ks * vec3(spec) * vec3(light_color), 0.0, 1.0);
		fragment_color = vec4(phongcolor, 1.0);
	//}

	if (cel_shade) {
		dot_nl = clamp(dot_nl, 0.0, 1.0);
		float diffuse = dot_nl;
		vec4 diffusecol = diffuse_color * (floor(diffuse * num_colors) / num_colors);
		float spec = specpow > 0.45 ? 1 : 0;
		fragment_color = (ka * ambient_color) + (kd * diffusecol) + (ks * specular_color * spec * light_color);
	}

	else if (gooch_shade) {
		//GOOCH
		vec4 warmcolor = warm_color;
		vec4 coolcolor = cool_color;
		float alpha = cool_amount;
		float beta = warm_amount;
		float interpolation = (1.0 + dot_nl) / 2.0;
		vec4 warmbase = warmcolor + beta * basecolor;
		vec4 coolbase = coolcolor + alpha * basecolor;
		//interpolate
		fragment_color = mix(coolbase, warmbase, interpolation);
	}

	if (hatch_shade) {
		float ambient = 0.05f;
		float diffuse = clamp(dot_nl, 0.0, 1.0);
		float specular = specpow;
		float intensity = clamp(diffuse + specular + ambient, 0.0, 1.0);
		if (on_white)
			fragment_color = vec4(1.0, 1.0, 1.0, 1.0);
		else if (on_flat)
			fragment_color = basecolor;
		if (intensity < 0.85) {
			if (mod(gl_FragCoord.x + gl_FragCoord.y, 10.0) == 0.0) {
				fragment_color = vec4(0.0, 0.0, 0.0, 1.0);
			}
		}
		if (intensity < 0.75) {
			if (mod(gl_FragCoord.x - gl_FragCoord.y, 10.0) == 0.0) {
				fragment_color = vec4(0.0, 0.0, 0.0, 1.0);
			}
		}
		if (intensity < 0.5) {
			if (mod(gl_FragCoord.x + gl_FragCoord.y - 5.0, 10.0) == 0.0) {
				fragment_color = vec4(0.0, 0.0, 0.0, 1.0);
			}
		}
		if (intensity < 0.25) {
			if (mod(gl_FragCoord.x - gl_FragCoord.y - 5.0, 10.0) == 0.0) {
				fragment_color = vec4(0.0, 0.0, 0.0, 1.0);
			}
		}
	}

	else if (texture_hatch) {
		//float intensity = clamp(dot_nl + 0.025 + specpow, 0.0, 1.0);
		float intensity = clamp(dot(normalize(normal3), normalize(lightdir3)), 0.0, 1.0);
		ivec2 texturesize = textureSize(hatching_texture, 0);
		vec2 texturexy = vec2(uv.x/texturesize.x, uv.y);
		int x, y;
		if (intensity < 0.33) {
			x = 0;
		}
		else if (intensity < 0.66) {
			x = 1;
		}
		else {
			x = 2;
		}
		y = 0;
		vec2 newxy = vec2(texturexy.x + x/3.0, texturexy.y + y);
		newxy += vec2(texturexy.x + x/3.0, texturexy.y + y - 0.1);
		fragment_color.xyz = texture(hatching_texture, newxy).rgb;
	}

	if (render_outline) {
		fragment_color = basecolor;
	}

	fragment_color.w = 1.0;
}
)zzz"
