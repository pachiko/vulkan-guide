#version 450
#extension GL_EXT_buffer_reference : require // extension for shader compiler to handle buffer references

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec2 outUV;

struct Vertex {
	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
}; 

// buffer_reference = using buffer address; std430 = alignment rules
layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

//push constants block
layout( push_constant ) uniform constants
{	
	mat4 render_matrix;
	VertexBuffer vertexBuffer; // uint64 handle (reference)
} PushConstants;

void main() 
{	
	//load vertex data from device adress
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];

	//output data
	gl_Position = PushConstants.render_matrix * vec4(v.position, 1.0f);
	outColor = v.color;
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
}