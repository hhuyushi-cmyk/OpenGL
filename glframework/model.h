#pragma once

#include "core.h"             // 包含GLAD, GLFW, GLM等核心库
#include "texture.h"          // 包含Texture类
#include "material.h"         // 包含Material类
#include "../wrapper/checkError.h" // 包含OpenGL错误检查宏

#include <string>             // 用于std::string
#include <vector>             // 用于std::vector
#include <fstream>            // 用于std::ifstream
#include <sstream>            // 用于std::stringstream
#include <limits>             // 用于std::numeric_limits，在计算边界框时使用
#include <algorithm>          // 用于std::min, std::max
#include <iostream>           // 用于std::cerr, std::cout进行调试输出
#include <map>                // 用于std::map，存储材质和优化顶点

// 前向声明 Shader 类
class Shader;

// Vertex结构体：定义单个顶点包含的数据
// 这是OpenGL渲染所需的最终顶点格式
struct Vertex {
    glm::vec3 position; // 顶点位置 (layout location = 0)
    glm::vec2 texCoord; // 纹理坐标 (layout location = 2)
    // glm::vec3 normal; // 法线向量 (layout location = 1) - 暂时不使用，但为未来光照预留

    // 用于std::map的比较操作符，确保Vertex是可排序的
    // 这用于在加载OBJ时，查找和合并重复的顶点
    bool operator<(const Vertex& other) const {
        if (position.x != other.position.x) return position.x < other.position.x;
        if (position.y != other.position.y) return position.y < other.position.y;
        if (position.z != other.position.z) return position.z < other.position.z;
        if (texCoord.x != other.texCoord.x) return texCoord.x < other.texCoord.x;
        if (texCoord.y != other.texCoord.y) return texCoord.y < other.texCoord.y;
        // if (normal.x != other.normal.x) return normal.x < other.normal.x; // 如果有法线
        return false; // 两个顶点完全相同
    }
    // 也可以实现operator==，但operator<对于map/set足够
};

// MeshPart结构体：表示模型的一个部分，可能使用不同的材质
struct MeshPart {
    unsigned int baseIndex;  // 在m_indices向量中的起始索引
    unsigned int indexCount; // 此部分包含的索引数量
    std::string materialName; // 此部分使用的材质名称
};


class Model {
public:
    // 构造函数：
    // - objFilePath: OBJ模型文件的路径（例如 "assets/models/your_model.obj"）
    // - textureBaseDir: 纹理文件所在的基目录（例如 "assets/models/"），用于解析MTL中纹理的相对路径
    // 在构造时完成模型的加载、数据处理和OpenGL缓冲区的设置。
    Model(const std::string& objFilePath, const std::string& textureBaseDir);

    // 析构函数：
    // 负责释放OpenGL缓冲区（VAO, VBO, EBO）资源和动态分配的Material对象，防止内存泄漏。
    ~Model();

    // 绘制模型：
    // - shader: 用于渲染当前模型的Shader程序。
    // 在此函数内部，会更新模型矩阵，并将MVP矩阵传输到着色器，然后绑定VAO并发出绘制指令。
    void draw(Shader& shader);

    // 设置模型在世界空间中的平移量。
    // pos: 模型的新的世界坐标位置。
    void setPosition(const glm::vec3& pos);

    // 设置模型在世界空间中的旋转。
    // angle: 旋转角度（度）。
    // axis: 旋转轴（例如 glm::vec3(0.0f, 1.0f, 0.0f) 表示绕Y轴）。
    void setRotation(float angle, const glm::vec3& axis);

    // 设置模型在世界空间中的缩放因子。
    // scale: 模型的缩放向量（例如 glm::vec3(2.0f) 表示放大两倍）。
    void setScale(const glm::vec3& scale);

    // 设置视图矩阵（通常由Camera类或main函数计算并传入）。
    // view: 摄像机的视图矩阵。
    void setViewMatrix(const glm::mat4& view);

    // 设置投影矩阵（通常由Camera类或main函数计算并传入）。
    // proj: 投影矩阵。
    void setProjectionMatrix(const glm::mat4& proj);

    // 获取当前模型的模型矩阵。
    const glm::mat4& getModelMatrix() const { return m_modelMatrix; }
    // 获取当前视图矩阵。
    const glm::mat4& getViewMatrix() const { return m_viewMatrix; }
    // 获取当前投影矩阵。
    const glm::mat4& getProjectionMatrix() const { return m_projectionMatrix; }

private:
    // 从指定路径的OBJ文件中加载原始顶点位置、纹理坐标和面索引。
    // objFilePath: OBJ模型文件的路径。
    // 返回值：一个bool，表示加载是否成功。
    bool loadObjRawData(const std::string& objFilePath);

    // 从MTL文件中加载材质信息。
    // mtlFilePath: MTL文件的路径。
    // mtlBaseDir: MTL文件所在的目录，用于解析纹理的相对路径。
    bool loadMaterials(const std::string& mtlFilePath, const std::string& mtlBaseDir);

    // 根据原始顶点数据计算模型的边界框（最小和最大坐标）。
    void calculateBoundingBox();

    // 处理原始数据：
    // - 对模型进行中心化和标准化缩放。
    // - 将原始的v/vt/vn索引合并为OpenGL友好的Vertex结构体，并优化重复顶点。
    void processData();

    // 设置OpenGL缓冲区：
    // - 生成并绑定VAO (Vertex Array Object)。
    // - 生成并填充VBO (Vertex Buffer Object) 来存储Vertex结构体。
    // - 生成并填充EBO (Element Buffer Object) 来存储索引。
    void setupBuffers();

    // 更新模型矩阵：
    // 根据m_currentPosition, m_currentRotation, m_currentScale重新计算m_modelMatrix。
    void updateModelMatrix();

private:
    // 原始数据（从OBJ文件直接读取，未处理）
    std::vector<glm::vec3> m_tempPositions; // 原始顶点位置
    std::vector<glm::vec2> m_tempTexCoords; // 原始纹理坐标
    // std::vector<glm::vec3> m_tempNormals; // 原始法线 (如果需要)

    // 经过处理和优化的最终顶点数据
    std::vector<Vertex> m_vertices;       // 唯一的Vertex结构体列表
    std::vector<unsigned int> m_indices;  // 索引列表，指向m_vertices

    // 模型包含的网格部分（每个部分可能使用不同材质）
    std::vector<MeshPart> m_meshes;

    // 材质信息
    std::map<std::string, Material*> m_materials; // 存储加载的材质，由Model管理生命周期
    std::string m_textureBaseDir; // 纹理文件所在的基目录（例如 "assets/models/"）

    // OpenGL缓冲区对象ID
    GLuint m_vao;       // 顶点数组对象ID
    GLuint m_vbo;       // 顶点缓冲区对象ID (存储Vertex结构体)
    GLuint m_ebo;       // 元素缓冲区对象ID (索引)

    // 变换矩阵
    glm::mat4 m_modelMatrix;      // 模型矩阵 (Model Matrix)
    glm::mat4 m_viewMatrix;       // 视图矩阵 (View Matrix)
    glm::mat4 m_projectionMatrix; // 投影矩阵 (Projection Matrix)

    // 模型变换的组成部分，用于方便地修改模型矩阵
    glm::vec3 m_currentPosition; // 模型在世界空间中的平移
    glm::quat m_currentRotation; // 模型在世界空间中的旋转（使用四元数）
    glm::vec3 m_currentScale;    // 模型在世界空间中的缩放

    // 模型的边界框，用于初始中心化和标准化大小
    glm::vec3 m_minCoords; // 模型的最小坐标
    glm::vec3 m_maxCoords; // 模型的最大坐标
};