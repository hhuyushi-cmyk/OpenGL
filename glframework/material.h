#pragma once

#include "core.h"    // 包含GLAD, GLFW, GLM
#include "texture.h" // 包含Texture类，用于管理纹理

#include <string>
#include <map>
#include <iostream>
#include <algorithm> // 用于std::replace

// Material类：封装OBJ文件中的材质信息
class Material {
public:
    std::string name; // 材质名称 (例如 "2995d87e-3daa-4ba3-9b51-2fb26402e506")
    glm::vec3 ambient = glm::vec3(0.1f);  // 环境光颜色 (Ka) - 暂时不使用
    glm::vec3 diffuse = glm::vec3(1.0f);  // 漫反射颜色 (Kd) - 暂时不使用
    glm::vec3 specular = glm::vec3(0.333333f); // 镜面反射颜色 (Ks) - 暂时不使用
    float shininess = 32.0f; // 光泽度 (Ns) - 暂时不使用

    std::string diffuseTexturePath; // 漫反射纹理的相对路径 (例如 "materials_textures/xxx.png")
    Texture* diffuseTexture = nullptr; // 实际加载的纹理对象指针

    // 构造函数
    Material(const std::string& matName = "") : name(matName) {}

    // 析构函数：负责释放动态分配的Texture对象
    ~Material() {
        if (diffuseTexture) {
            delete diffuseTexture;
            diffuseTexture = nullptr;
        }
        std::cout << "Material '" << name << "' destroyed." << std::endl;
    }

    // 加载纹理：
    // - baseDir: 纹理文件所在的基目录（例如 "assets/models/"）
    // - textureUnit: 纹理将绑定到的OpenGL纹理单元
    void loadDiffuseTexture(const std::string& baseDir, unsigned int textureUnit) {
        if (!diffuseTexturePath.empty() && !diffuseTexture) {
            // 纹理路径可能包含反斜杠，需要转换为正斜杠以兼容
            std::string path = diffuseTexturePath;
            std::replace(path.begin(), path.end(), '\\', '/');
            std::string fullPath = baseDir + path; // 组合完整路径

            std::cout << "DEBUG: Attempting to load texture for material '" << name << "': " << fullPath << std::endl;
            diffuseTexture = new Texture(fullPath, textureUnit);

            if (diffuseTexture->getTextureID() == 0) {
                std::cerr << "WARNING: Failed to load texture for material '" << name << "' from " << fullPath << std::endl;
                delete diffuseTexture; // 清理失败的Texture对象
                diffuseTexture = nullptr;
            }
            else {
                std::cout << "DEBUG: Texture loaded successfully for material '" << name << "' (ID: " << diffuseTexture->getTextureID() << ")" << std::endl;
            }
        }
    }

    // 绑定纹理：激活纹理单元并绑定纹理对象
    void bindTexture() const {
        if (diffuseTexture) {
            diffuseTexture->bind();
        }
        else {
            // 如果没有纹理，可以绑定一个默认的白色纹理，或者解绑纹理
            // 为了简单，这里不做特殊处理，依赖着色器默认颜色或解绑
            glBindTexture(GL_TEXTURE_2D,0); // 解绑纹理，确保不会使用旧的纹理
        }
    }

    // 获取纹理ID
    GLuint getTextureID() const {
        return diffuseTexture ? diffuseTexture->getTextureID() : 0;
    }
};