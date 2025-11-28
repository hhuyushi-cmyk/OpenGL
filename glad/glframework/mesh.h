#pragma once

#include "core.h"             // 包含GLAD, GLFW, GLM等核心库
#include "../wrapper/checkError.h" // 包含OpenGL错误检查宏
#include "material.h"         // 引入Material类

#include <vector>             // 用于std::vector
#include <string>             // 用于std::string
#include <iostream>           // 用于std::cerr, std::cout进行调试输出

// Mesh类：封装单个几何体（顶点数据、索引、OpenGL缓冲区）及其材质
class Mesh {
public:
    // 构造函数：
    // - vertices: 扁平化的顶点数据 (位置x,y,z, 纹理坐标u,v)
    // - indices: 索引数据
    // - material: 指向该Mesh使用的材质对象
    Mesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, Material* material);
    ~Mesh();

    // 绘制Mesh：
    // - shader: 当前激活的Shader程序。
    // 绑定VAO，激活材质，并发出绘制指令。
    void draw(Shader& shader);

private:
    // 设置OpenGL缓冲区：
    // - 生成并绑定VAO (Vertex Array Object)。
    // - 生成并填充VBO (Vertex Buffer Object) 来存储顶点数据 (位置+纹理坐标)。
    // - 生成并填充EBO (Element Buffer Object) 来存储索引。
    void setupBuffers();

private:
    std::vector<float> m_vertices;      // 扁平化的顶点数据 (PosXYZ + UV)
    std::vector<unsigned int> m_indices; // 索引数据

    GLuint m_vao;       // 顶点数组对象ID
    GLuint m_vbo;       // 顶点缓冲区对象ID (包含位置和纹理坐标)
    GLuint m_ebo;       // 元素缓冲区对象ID (索引)

    Material* m_material; // 该Mesh使用的材质，不拥有其生命周期
};