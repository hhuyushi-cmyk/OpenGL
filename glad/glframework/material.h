#pragma once

#include "core.h"             // 包含GLAD, GLFW, GLM等核心库
#include "../wrapper/checkError.h" // 包含OpenGL错误检查宏
#include "texture.h"          // 引入Texture类来管理OpenGL纹理对象
#include "shader.h"           // 引入Shader类来管理OpenGL着色器对象
#include <string>             // 用于std::string
#include <map>                // 用于std::map存储纹理
#include <iostream>           // 用于std::cerr, std::cout进行调试输出

// Material类：负责解析.mtl文件，加载纹理，并管理材质属性
class Material {
public:
    // 构造函数：
    // - mtlFilePath: .mtl文件的完整路径（例如 "assets/models/building.mtl"）。
    // - baseDir: 材质文件所在的目录，用于构建纹理图片的绝对路径。
    Material(const std::string& mtlFilePath, const std::string& baseDir);
    ~Material();

    // 激活材质：
    // - shader: 当前激活的Shader程序。
    // 将材质的属性（如漫反射纹理）绑定到着色器。
    void use(Shader& shader);

    // 获取材质名称
    const std::string& getName() const { return m_name; }

private:
    // 解析.mtl文件，加载材质属性和纹理
    // - mtlFilePath: .mtl文件的路径。
    // - baseDir: 纹理图片所在的目录。
    void loadMtlFile(const std::string& mtlFilePath, const std::string& baseDir);

public:
    std::string m_name; // 材质名称 (从newmtl指令读取)
    glm::vec3 m_Ks = glm::vec3(0.333f); // 镜面反射颜色 (Ks)，默认值
    // TODO: 可以添加更多材质属性，如Kd (漫反射颜色), Ka (环境光颜色), Ns (高光指数), d (透明度) 等

    // 纹理贴图：目前只处理漫反射贴图 (map_Kd)
    // std::map<std::string, Texture*> m_textures; // 可以存储多种纹理
    Texture* m_diffuseTexture = nullptr; // 漫反射纹理 (map_Kd)
};