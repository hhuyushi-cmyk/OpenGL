#pragma once

#include "core.h"             // 包含GLAD, GLFW, GLM等核心库
#include "material.h"
#include <fstream>      // <<< 添加此行，用于std::ifstream
#include <sstream>      // <<< 添加此行，用于std::stringstream
#include "shader.h" // 需要Shader类来设置uniforms

// 构造函数：解析.mtl文件并加载纹理
Material::Material(const std::string& mtlFilePath, const std::string& baseDir) {
    loadMtlFile(mtlFilePath, baseDir);
}

Material::~Material() {
    // 释放动态分配的纹理对象
    if (m_diffuseTexture) {
        delete m_diffuseTexture;
        m_diffuseTexture = nullptr;
    }
    std::cout << "Material '" << m_name << "' destroyed." << std::endl;
}

// 激活材质：将材质的属性（如漫反射纹理）绑定到着色器
void Material::use(Shader& shader) {
    // 绑定漫反射纹理到纹理单元0
    if (m_diffuseTexture) {
        m_diffuseTexture->bind(); // 激活纹理单元并绑定纹理
        shader.setInt("u_DiffuseSampler", 0); // 将纹理单元0传递给着色器中的uniform sampler
    }
    else {
        // 如果没有漫反射纹理，可以绑定一个默认的白色纹理，或者传递一个纯色
        // 这里为了简化，如果没有纹理就不绑定，着色器会使用默认颜色
         GL_CALL(glBindTexture(GL_TEXTURE_2D, 0)); // 解绑纹理
        // shader.setVector3("u_DiffuseColor", m_Kd.x, m_Kd.y, m_Kd.z); // 如果有Kd颜色，可以传递
    }
    // TODO: 如果有其他材质属性，如镜面反射颜色Ks，也在这里传递
    // shader.setVector3("u_Ks", m_Ks.x, m_Ks.y, m_Ks.z);
}

// 解析.mtl文件，加载材质属性和纹理
void Material::loadMtlFile(const std::string& mtlFilePath, const std::string& baseDir) {
    std::ifstream file(mtlFilePath);
    if (!file.is_open()) {
        std::cerr << "ERROR: Could not open MTL file: " << mtlFilePath << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "newmtl") {
            ss >> m_name; // 读取材质名称
            std::cout << "Loading material: " << m_name << std::endl;
        }
        else if (type == "map_Kd") { // 漫反射纹理贴图
            std::string textureRelativePath;
            ss >> textureRelativePath;
            // 构建纹理的绝对路径
            std::string textureFullPath = baseDir + "/" + textureRelativePath;
            // 加载纹理，绑定到纹理单元0
            m_diffuseTexture = new Texture(textureFullPath, 0);
            std::cout << "  Diffuse texture: " << textureFullPath << std::endl;
        }
        else if (type == "Ks") { // 镜面反射颜色
            ss >> m_Ks.x >> m_Ks.y >> m_Ks.z;
            std::cout << "  Ks: (" << m_Ks.x << ", " << m_Ks.y << ", " << m_Ks.z << ")" << std::endl;
        }
        // TODO: 可以添加对Kd, Ka, Ns等其他MTL属性的解析
    }
    file.close();
}