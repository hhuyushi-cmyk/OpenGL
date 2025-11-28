#pragma once

#include "core.h"             // 包含GLAD, GLFW, GLM等核心库
#include "../wrapper/checkError.h" // 包含OpenGL错误检查宏
#include "mesh.h"             // 引入Mesh类
#include "material.h"         // 引入Material类

#include <string>             // 用于std::string
#include <vector>             // 用于std::vector
#include <fstream>            // 用于std::ifstream
#include <sstream>            // 用于std::stringstream
#include <limits>             // 用于std::numeric_limits，在计算边界框时使用
#include <algorithm>          // 用于std::min, std::max
#include <map>                // 用于存储材质
#include <iostream>           // 用于std::cerr, std::cout进行调试输出

// 前向声明 Shader 类
class Shader;
class Camera; // 前向声明Camera类，用于LOD计算

// Model类：负责加载OBJ文件（包含v, vt, f数据），处理数据，管理多个Mesh和Material，
// 并封装模型变换矩阵。
class Model {
public:
    // 构造函数：
    // - filePath: OBJ模型文件的路径（例如 "assets/models/building.obj"）。
    // 在构造时完成模型的加载、数据处理和OpenGL缓冲区的设置。
    Model(const std::string& filePath);

    // 析构函数：
    // 负责释放所有Mesh和Material资源。
    ~Model();

    // 绘制模型：
    // - shader: 用于渲染当前模型的Shader程序。
    // 在此函数内部，会更新模型矩阵，并将MVP矩阵传输到着色器，然后遍历并绘制所有Mesh。
    void draw(Shader& shader);

    // 设置模型在世界空间中的平移量。
    void setPosition(const glm::vec3& pos);

    // 设置模型在世界空间中的旋转。
    void setRotation(float angle, const glm::vec3& axis);

    // 设置模型在世界空间中的缩放因子。
    void setScale(const glm::vec3& scale);

    // 设置视图矩阵（通常由Camera类或main函数计算并传入）。
    void setViewMatrix(const glm::mat4& view);

    // 设置投影矩阵（通常由Camera类或main函数计算并传入）。
    void setProjectionMatrix(const glm::mat4& proj);

    // 获取模型的世界空间中心点，用于LOD计算
    glm::vec3 getWorldCenter() const;

    // 获取当前模型的模型矩阵。
    const glm::mat4& getModelMatrix() const { return m_modelMatrix; }
    // 获取当前视图矩阵。
    const glm::mat4& getViewMatrix() const { return m_viewMatrix; }
    // 获取当前投影矩阵。
    const glm::mat4& getProjectionMatrix() const { return m_projectionMatrix; }

private:
    // 从OBJ文件中加载原始顶点位置(v)、纹理坐标(vt)和面索引(f)。
    // filePath: OBJ模型文件的路径。
    // 返回值：一个包含所有原始数据的结构体。
    struct RawObjData {
        std::vector<glm::vec3> positions; // 原始顶点位置
        std::vector<glm::vec2> texCoords; // 原始纹理坐标
        // OBJ文件中的面数据，每个元素代表一个顶点在面中的引用
        // 例如：f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3
        // 这里只处理 v/vt
        struct VertexIndices {
            unsigned int posIndex;
            unsigned int texCoordIndex;
        };
        struct Face {
            std::vector<VertexIndices> vertices; // 通常是3个或4个顶点
        };
        std::vector<Face> faces; // 所有面

        // 用于存储材质组 (usemtl)
        struct MeshGroup {
            std::string materialName;
            std::vector<unsigned int> faceIndices; // 属于此材质组的面索引
        };
        std::vector<MeshGroup> meshGroups;
        std::string mtlLibName; // .mtl文件名称
    };
    RawObjData loadRawData(const std::string& filePath);

    // 根据原始顶点数据计算模型的边界框（最小和最大坐标）。
    void calculateBoundingBox(const std::vector<glm::vec3>& rawPositions);

    // 处理原始数据：
    // - 对模型进行中心化（使其中心位于局部坐标系原点）。
    // - 对模型进行标准化缩放（使其尺寸在一个合理范围内）。
    // - 根据材质组创建并填充Mesh对象。
    // rawData: 从OBJ文件加载的原始数据。
    // objBaseDir: OBJ文件所在的目录，用于加载MTL文件和纹理。
    void processData(const RawObjData& rawData, const std::string& objBaseDir);

    // 更新模型矩阵：
    // 根据m_currentPosition, m_currentRotation, m_currentScale重新计算m_modelMatrix。
    void updateModelMatrix();

private:
    std::string m_filePath; // OBJ文件路径

    // 模型的多个几何体部分
    std::vector<Mesh*> m_meshes;
    // 模型的材质库
    std::map<std::string, Material*> m_materials;

    // 变换矩阵
    glm::mat4 m_modelMatrix;      // 模型矩阵 (Model Matrix)
    glm::mat4 m_viewMatrix;       // 视图矩阵 (View Matrix)
    glm::mat4 m_projectionMatrix; // 投影矩阵 (Projection Matrix)

    // 模型变换的组成部分，用于方便地修改模型矩阵
    glm::vec3 m_currentPosition; // 模型在世界空间中的平移
    glm::quat m_currentRotation; // 模型在世界空间中的旋转（使用四元数避免万向节锁）
    glm::vec3 m_currentScale;    // 模型在世界空间中的缩放

    // 模型的原始边界框，用于初始中心化和标准化大小
    glm::vec3 m_minCoords; // 模型的最小坐标
    glm::vec3 m_maxCoords; // 模型的最大坐标
    glm::vec3 m_localCenter; // 模型在局部坐标系中的中心点
};