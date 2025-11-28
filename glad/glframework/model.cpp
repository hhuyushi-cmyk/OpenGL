#include "model.h"
#include "material.h"
#include "shader.h" // 需要Shader类来设置uniforms

// 构造函数：加载模型数据，处理并设置OpenGL缓冲区
Model::Model(const std::string & filePath)
// 初始化成员变量
    : m_filePath(filePath),
    m_modelMatrix(1.0f), m_viewMatrix(1.0f), m_projectionMatrix(1.0f),
    m_currentPosition(0.0f), // 默认位置在世界原点
    m_currentRotation(1.0f, 0.0f, 0.0f, 0.0f), // 默认单位四元数，表示无旋转
    m_currentScale(1.0f)    // 默认不缩放
{
    // 提取OBJ文件所在的目录，用于加载MTL文件和纹理
    std::string objBaseDir = filePath.substr(0, filePath.find_last_of("/\\") + 1);

    // 1. 加载原始数据：从OBJ文件读取顶点、纹理坐标和面
    RawObjData rawData = loadRawData(filePath);

    // 检查是否成功加载数据
    if (rawData.positions.empty() || rawData.faces.empty()) {
        std::cerr << "ERROR: Model could not be loaded or is empty: " << filePath << std::endl;
        return;
    }

    // 2. 计算模型的边界框：确定模型的最小和最大坐标
    calculateBoundingBox(rawData.positions);

    // 3. 处理数据：对原始数据进行中心化和标准化缩放，并创建Mesh和Material对象
    processData(rawData, objBaseDir);

    // 4. 初始化模型矩阵
    updateModelMatrix();
    std::cout << "Model '" << filePath << "' loaded successfully." << std::endl;
}

// 析构函数：释放所有Mesh和Material资源
Model::~Model() {
    // 释放所有Mesh对象
    for (Mesh* mesh : m_meshes) {
        delete mesh;
    }
    m_meshes.clear();

    // 释放所有Material对象
    for (auto const& [key, val] : m_materials) {
        delete val;
    }
    m_materials.clear();
    std::cout << "Model '" << m_filePath << "' destroyed." << std::endl;
}

// 绘制模型
void Model::draw(Shader& shader) {
    // 确保有Mesh可绘制
    if (m_meshes.empty()) {
        std::cerr << "WARNING: Attempted to draw model with no meshes." << std::endl;
        return;
    }

    // 在绘制前确保模型矩阵是最新的
    updateModelMatrix();

    // MVP矩阵实现的依据及过程：
    // 顶点着色器中的最终位置计算为：gl_Position = ProjectionMatrix * ViewMatrix * ModelMatrix * VertexPosition_local
    // 1. Model Matrix (模型矩阵): 将模型从其局部空间转换到世界空间。
    //    它由平移 (m_currentPosition), 旋转 (m_currentRotation), 缩放 (m_currentScale) 组合而成。
    //    在updateModelMatrix()中计算。
    shader.setMatrix4x4("transform", m_modelMatrix);

    // 2. View Matrix (视图矩阵): 将世界空间中的物体转换到摄像机（观察者）空间。
    //    它由外部（通常是Camera类）计算，并由setViewMatrix()传入。
    shader.setMatrix4x4("viewMatrix", m_viewMatrix);

    // 3. Projection Matrix (投影矩阵): 将摄像机空间中的物体转换到裁剪空间。
    //    它由外部（通常是Camera类）计算，并由setProjectionMatrix()传入。
    shader.setMatrix4x4("projectionMatrix", m_projectionMatrix);

    // 遍历并绘制所有Mesh
    for (Mesh* mesh : m_meshes) {
        mesh->draw(shader);
    }
}

// 设置模型在世界空间中的平移量。
// 每次设置后，会重新计算模型矩阵。
void Model::setPosition(const glm::vec3& pos) {
    m_currentPosition = pos;
    updateModelMatrix(); // 更新模型矩阵以反映新的位置
}

// 设置模型在世界空间中的旋转。
// angle: 旋转角度（度）。
// axis: 旋转轴（例如 glm::vec3(0.0f, 1.0f, 0.0f) 表示绕Y轴）。
// 注意：这里是设置为一个绝对旋转，而不是累加。
void Model::setRotation(float angle, const glm::vec3& axis) {
    // 使用glm::angleAxis创建四元数，glm::radians将角度转换为弧度
    m_currentRotation = glm::angleAxis(glm::radians(angle), glm::normalize(axis));
    updateModelMatrix(); // 更新模型矩阵以反映新的旋转
}

// 设置模型在世界空间中的缩放因子。
// scale: 模型的缩放向量（例如 glm::vec3(2.0f) 表示放大两倍）。
void Model::setScale(const glm::vec3& scale) {
    m_currentScale = scale;
    updateModelMatrix(); // 更新模型矩阵以反映新的缩放
}

// 设置视图矩阵。
// 该矩阵通常由Camera类计算，并传递给Model。
void Model::setViewMatrix(const glm::mat4& view) {
    m_viewMatrix = view;
}

// 设置投影矩阵。
// 该矩阵通常由Camera类计算，并传递给Model。
void Model::setProjectionMatrix(const glm::mat4& proj) {
    m_projectionMatrix = proj;
}

// 获取模型的世界空间中心点，用于LOD计算
glm::vec3 Model::getWorldCenter() const {
    // 模型的世界空间中心点 = 模型矩阵 * 局部中心点
    return glm::vec3(m_modelMatrix * glm::vec4(m_localCenter, 1.0f));
}

// 更新模型矩阵：根据平移、旋转、缩放分量重新计算组合矩阵。
// 变换顺序：缩放 -> 旋转 -> 平移。
// 由于GLM的矩阵乘法是从右到左应用的，所以代码中的乘法顺序是：平移矩阵 * 旋转矩阵 * 缩放矩阵。
void Model::updateModelMatrix() {
    // 创建平移矩阵
    glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), m_currentPosition);
    // 将四元数转换为旋转矩阵
    glm::mat4 rotationMatrix = glm::mat4_cast(m_currentRotation);
    // 创建缩放矩阵
    glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), m_currentScale);

    // 组合模型矩阵：缩放 -> 旋转 -> 平移
    m_modelMatrix = translationMatrix * rotationMatrix * scaleMatrix;
}

// 从OBJ文件加载原始顶点位置(v)、纹理坐标(vt)和面索引(f)。
// 该函数只负责文件解析，不进行任何几何处理。
Model::RawObjData Model::loadRawData(const std::string& filePath) {
    RawObjData rawData;

    std::ifstream file(filePath); // 打开OBJ文件
    if (!file.is_open()) {
        std::cerr << "ERROR: Could not open OBJ file: " << filePath << std::endl;
        return rawData; // 文件打开失败，返回空的rawData
    }

    std::string line;
    std::string currentMaterialName = "default"; // 默认材质名
    rawData.meshGroups.push_back({ currentMaterialName, {} }); // 添加默认材质组

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "v") { // 顶点位置
            glm::vec3 pos;
            ss >> pos.x >> pos.y >> pos.z;
            rawData.positions.push_back(pos);
        }
        else if (type == "vt") { // 纹理坐标
            glm::vec2 uv;
            ss >> uv.x >> uv.y;
            rawData.texCoords.push_back(uv);
        }
        else if (type == "f") { // 面
            RawObjData::Face face;
            std::string vertexStr;
            while (ss >> vertexStr) {
                RawObjData::VertexIndices vi;
                // 解析 "v/vt" 格式
                size_t posSlash = vertexStr.find('/');
                size_t texCoordSlash = std::string::npos; // 假设没有法线，只有一个斜杠

                if (posSlash != std::string::npos) {
                    vi.posIndex = std::stoul(vertexStr.substr(0, posSlash)) - 1;
                    texCoordSlash = vertexStr.find('/', posSlash + 1); // 查找第二个斜杠
                    if (texCoordSlash != std::string::npos) { // 如果有第二个斜杠，说明有法线，忽略
                        vi.texCoordIndex = std::stoul(vertexStr.substr(posSlash + 1, texCoordSlash - (posSlash + 1))) - 1;
                    }
                    else { // 只有v/vt
                        vi.texCoordIndex = std::stoul(vertexStr.substr(posSlash + 1)) - 1;
                    }
                }
                else { // 只有v
                    vi.posIndex = std::stoul(vertexStr) - 1;
                    vi.texCoordIndex = 0; // 默认纹理坐标索引，可能无效
                }
                face.vertices.push_back(vi);
            }
            // 确保面是三角形 (如果不是，需要进行三角化处理，这里简化为只处理三角形)
            if (face.vertices.size() == 3) {
                rawData.faces.push_back(face);
                // 将当前面添加到当前材质组
                rawData.meshGroups.back().faceIndices.push_back(rawData.faces.size() - 1);
            }
            else {
                std::cerr << "WARNING: Skipping non-triangle face in OBJ file: " << line << std::endl;
            }
        }
        else if (type == "mtllib") { // 材质库文件
            ss >> rawData.mtlLibName;
            std::cout << "MTL Lib: " << rawData.mtlLibName << std::endl;
        }
        else if (type == "usemtl") { // 使用材质
            ss >> currentMaterialName;
            // 检查是否已有此材质组，如果没有则创建新的
            bool found = false;
            for (const auto& group : rawData.meshGroups) {
                if (group.materialName == currentMaterialName) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                rawData.meshGroups.push_back({ currentMaterialName, {} });
            }
            // 确保 currentMaterialName 是最后一个 meshGroup 的 materialName
            // 这是一个简化处理，假设usemtl总是切换到新的meshGroup
            // 实际可能需要更复杂的逻辑来处理多个usemtl指向同一个材质的情况
            // 这里我们直接创建一个新的meshGroup，并将其materialName设置为currentMaterialName
            if (rawData.meshGroups.back().materialName != currentMaterialName) {
                rawData.meshGroups.push_back({ currentMaterialName, {} });
            }
        }
    }
    file.close();

    std::cout << "Loaded " << rawData.positions.size() << " raw vertices, "
        << rawData.texCoords.size() << " raw texture coordinates, and "
        << rawData.faces.size() << " faces from " << filePath << std::endl;

    return rawData;
}

// 计算模型的边界框（min_coords和max_coords）。
// 边界框用于后续的中心化和标准化缩放。
void Model::calculateBoundingBox(const std::vector<glm::vec3>& rawPositions) {
    // 初始化最小坐标为最大浮点数，最大坐标为最小浮点数
    m_minCoords = glm::vec3(std::numeric_limits<float>::max());
    m_maxCoords = glm::vec3(std::numeric_limits<float>::lowest());

    if (rawPositions.empty()) {
        std::cerr << "WARNING: No raw positions to calculate bounding box." << std::endl;
        return;
    }

    // 遍历所有原始顶点位置，更新最小和最大坐标
    for (const auto& pos : rawPositions) {
        m_minCoords.x = std::min(m_minCoords.x, pos.x);
        m_minCoords.y = std::min(m_minCoords.y, pos.y);
        m_minCoords.z = std::min(m_minCoords.z, pos.z);
        m_maxCoords.x = std::max(m_maxCoords.x, pos.x);
        m_maxCoords.y = std::max(m_maxCoords.y, pos.y);
        m_maxCoords.z = std::max(m_maxCoords.z, pos.z);
    }
    m_localCenter = (m_minCoords + m_maxCoords) / 2.0f; // 计算局部中心点
    std::cout << "Bounding Box: Min(" << m_minCoords.x << ", " << m_minCoords.y << ", " << m_minCoords.z << ") "
        << "Max(" << m_maxCoords.x << ", " << m_maxCoords.y << ", " << m_maxCoords.z << ")" << std::endl;
}

// 处理原始数据：中心化、标准化缩放，并创建Mesh和Material对象。
void Model::processData(const RawObjData& rawData, const std::string& objBaseDir) {
    if (rawData.positions.empty()) {
        std::cerr << "WARNING: No raw positions to process." << std::endl;
        return;
    }

    // 计算模型的中心点
    glm::vec3 center = (m_minCoords + m_maxCoords) / 2.0f;
    // 计算模型的范围（各轴上的长度）
    glm::vec3 extent = m_maxCoords - m_minCoords;
    // 找出模型最大的维度
    float max_dim = std::max({ extent.x, extent.y, extent.z });
    // 计算缩放因子，使模型最大的维度约为2个单位，这样模型大致在[-1, 1]的范围内，方便观察
    float scale_factor = 2.0f / max_dim;

    // 构建初始的模型变换矩阵：先缩放，再平移（将模型中心移到原点）。
    // 这个变换会将模型从其原始坐标系转换到局部坐标系，使其中心位于(0,0,0)且大小标准化。
    glm::mat4 initialTransform = glm::mat4(1.0f);
    initialTransform = glm::scale(initialTransform, glm::vec3(scale_factor)); // 先缩放
    initialTransform = glm::translate(initialTransform, -center);             // 再平移到原点

    // --- 1. 加载材质 ---
    if (!rawData.mtlLibName.empty()) {
        std::string mtlFilePath = objBaseDir + rawData.mtlLibName;
        // 假设一个MTL文件只定义一个材质，且材质名与mtl文件中的newmtl一致
        // 实际可能需要解析MTL文件，获取所有材质定义
        Material* mat = new Material(mtlFilePath, objBaseDir + "materials_textures/"); // 纹理在子目录
        if (!mat->getName().empty()) {
            m_materials[mat->getName()] = mat;
        }
        else {
            delete mat; // 如果材质没有名字，则删除
        }
    }
    // 如果没有MTL文件或材质加载失败，创建一个默认材质
    if (m_materials.empty()) {
        std::cout << "No materials loaded, creating default material." << std::endl;
        // 创建一个名为"default"的默认材质，不带纹理
        Material* defaultMat = new Material("", ""); // 空路径，不会加载文件
        defaultMat->m_name = "default"; // 手动设置名称
        m_materials["default"] = defaultMat;
    }


    // --- 2. 根据材质组创建Mesh ---
    for (const auto& meshGroup : rawData.meshGroups) {
        std::vector<float> meshVertices; // 存储当前Mesh的顶点数据 (PosXYZ + UV)
        std::vector<unsigned int> meshIndices; // 存储当前Mesh的索引

        // 用于将OBJ的v/vt索引映射到Mesh的扁平化顶点数组的索引
        // key: (pos_idx, tex_idx) -> value: new_flat_vertex_idx
        std::map<std::pair<unsigned int, unsigned int>, unsigned int> uniqueVertices;
        unsigned int currentVertexCount = 0;

        // 遍历属于当前材质组的所有面
        for (unsigned int faceIdx : meshGroup.faceIndices) {
            const auto& face = rawData.faces[faceIdx];
            // 遍历面中的每个顶点
            for (const auto& vi : face.vertices) {
                std::pair<unsigned int, unsigned int> key = { vi.posIndex, vi.texCoordIndex };

                if (uniqueVertices.find(key) == uniqueVertices.end()) {
                    // 如果是新顶点，则添加
                    uniqueVertices[key] = currentVertexCount++;

                    // 获取原始位置并应用初始变换
                    glm::vec4 transformed_pos = initialTransform * glm::vec4(rawData.positions[vi.posIndex], 1.0f);
                    meshVertices.push_back(transformed_pos.x);
                    meshVertices.push_back(transformed_pos.y);
                    meshVertices.push_back(transformed_pos.z);

                    // 获取原始纹理坐标
                    if (vi.texCoordIndex < rawData.texCoords.size()) {
                        meshVertices.push_back(rawData.texCoords[vi.texCoordIndex].x);
                        meshVertices.push_back(rawData.texCoords[vi.texCoordIndex].y);
                    }
                    else {
                        // 如果纹理坐标索引无效，使用默认值
                        meshVertices.push_back(0.0f);
                        meshVertices.push_back(0.0f);
                    }
                }
                // 添加索引到Mesh
                meshIndices.push_back(uniqueVertices[key]);
            }
        }

        // 获取当前Mesh的材质
        Material* meshMaterial = nullptr;
        if (m_materials.count(meshGroup.materialName)) {
            meshMaterial = m_materials[meshGroup.materialName];
        }
        else {
            // 如果材质未找到，使用默认材质
            meshMaterial = m_materials["default"];
            std::cerr << "WARNING: Material '" << meshGroup.materialName << "' not found for mesh group, using 'default'." << std::endl;
        }

        // 创建Mesh对象并添加到列表中
        if (!meshVertices.empty() && !meshIndices.empty()) {
            m_meshes.push_back(new Mesh(meshVertices, meshIndices, meshMaterial));
        }
    }

    std::cout << "Model processed into " << m_meshes.size() << " meshes." << std::endl;
}