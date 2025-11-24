#version 460 core
out vec4 FragColor; // 片段着色器的最终颜色输出

// in vec3 color; // 从顶点着色器插值而来的颜色 (暂时不使用)
in vec2 uv; // <<< 从顶点着色器插值而来的纹理坐标

uniform sampler2D sampler; // <<< 重新引入纹理采样器uniform

void main()
{
  FragColor = texture(sampler, uv); // <<< 使用纹理采样作为最终颜色
  // FragColor = vec4(color, 1.0); // 如果使用顶点颜色，这里使用
}