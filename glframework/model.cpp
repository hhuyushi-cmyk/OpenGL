#include "model.h"
#include "shader.h" // 需要Shader类来设置uniforms

// 构造函数：加载模型数据，处理并设置OpenGL缓冲区
Model::Model(const std::string& objFilePath, const std::string& textureBaseDir)
// 初始化成员变量
    : m_vao(0), m_vbo(0), m_ebo(0),
    m_modelMatrix(1.0f), m_viewMatrix(1.0f), m_projectionMatrix(1.0f),
    m_currentPosition(0.0f), // 默认位置在世界原点
    m_currentRotation(1.0f, 0.0f, 0.0f, 0.0f), // 默认单位四元数，表示无旋转
    m_currentScale(1.0f),    // 默认不缩放
    m_textureBaseDir(textureBaseDir) // 存储纹理基目录
{
    std::cout << "DEBUG: Model constructor called for " << objFilePath << std::endl;

    // 1. 加载OBJ原始数据：包括v, vt, f，并解析mtllib和usemtl
    if (!loadObjRawData(objFilePath)) {
        std::cerr << "ERROR: Failed to load OBJ raw data from " << objFilePath << std::endl;
        return;
    }

    // 2. 计算模型的边界框：确定模型的最小和最大坐标
    calculateBoundingBox();

    // 3. 处理数据：对原始数据进行中心化和标准化缩放，并优化顶点
    processData();

    // 4. 设置OpenGL缓冲区：将处理后的数据上传到GPU
    setupBuffers();

    // 5. 初始化模型矩阵：根据默认的平移、旋转、缩放设置初始模型矩阵
    updateModelMatrix();

    std::cout << "DEBUG: Model '" << objFilePath << "' loaded successfully. VAO ID: " << m_vao << std::endl;
}

// 析构函数：释放OpenGL缓冲区资源和动态分配的Material对象
Model::~Model() {
    // 确保VAO ID有效，然后删除
    if (m_vao != 0) {
        GL_CALL(glDeleteVertexArrays(1, &m_vao));
    }
    // 确保VBO ID有效，然后删除
    if (m_vbo != 0) {
        GL_CALL(glDeleteBuffers(1, &m_vbo));
    }
    // 确保EBO ID有效，然后删除
    if (m_ebo != 0) {
        GL_CALL(glDeleteBuffers(1, &m_ebo));
    }

    // 释放所有动态分配的Material对象
    for (auto const& [key, val] : m_materials) {
        delete val;
    }
    m_materials.clear();

    std::cout << "DEBUG: Model destroyed." << std::endl;
}

// 绘制模型
void Model::draw(Shader& shader) {
    // 确保VAO已成功创建且有数据可绘制
    if (m_vao == 0 || m_indices.empty()) {
        std::cerr << "WARNING: Attempted to draw model with uninitialized VAO or empty indices." << std::endl;
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

    // 绑定VAO，激活其记录的所有顶点属性和缓冲区
    GL_CALL(glBindVertexArray(m_vao));

    // 遍历模型的每个网格部分，为每个部分绑定其材质并绘制
    for (const auto& mesh : m_meshes) {
        auto it = m_materials.find(mesh.materialName);
        if (it != m_materials.end() && it->second->diffuseTexture && it->second->diffuseTexture->getTextureID() != 0) {
            // 绑定材质的漫反射纹理
            it->second->bindTexture();
            // 将纹理单元0传递到着色器的sampler uniform
            shader.setInt("sampler", 0); // 假设所有纹理都绑定到GL_TEXTURE0
        }
        else {
            // 如果没有找到材质或纹理，或者纹理加载失败，绑定一个默认的白色纹理（或解绑）
            // 为了简单，这里解绑纹理，着色器会使用默认颜色
            GL_CALL(glBindTexture(GL_TEXTURE_2D, 0)); // 解绑纹理
            shader.setInt("sampler", 0); // 仍然设置sampler，但它将采样到0号单元的默认/空纹理
        }

        // 发出绘制指令，使用索引缓冲区绘制当前网格部分
        // - GL_TRIANGLES: 绘制三角形图元
        // - mesh.indexCount: 当前网格部分要绘制的索引数量
        // - GL_UNSIGNED_INT: 索引数据类型
        // - (void*)(mesh.baseIndex * sizeof(unsigned int)): 在EBO中的起始偏移量
        GL_CALL(glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, (void*)(static_cast<size_t>(mesh.baseIndex) * sizeof(unsigned int))));
    }

    // 解绑VAO，防止后续操作意外修改此VAO状态
    GL_CALL(glBindVertexArray(0));
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

// 从OBJ文件加载原始顶点位置、纹理坐标和面索引。
// 同时解析mtllib和usemtl指令。
bool Model::loadObjRawData(const std::string& objFilePath) {
    std::cout << "DEBUG: Entering loadObjRawData for " << objFilePath << std::endl;
    std::ifstream file(objFilePath);
    if (!file.is_open()) {
        std::cerr << "ERROR: Could not open OBJ file: " << objFilePath << std::endl;
        std::cerr << "  System error (errno): " << errno << " - " << strerror(errno) << std::endl;
        return false;
    }

    std::string line;
    std::string currentMaterialName = "default"; // 默认材质名称

    // 清理之前的临时数据，确保重新加载时是干净的
    m_tempPositions.clear();
    m_tempTexCoords.clear();
    m_vertices.clear();
    m_indices.clear();
    m_meshes.clear();
    for (auto const& [key, val] : m_materials) { delete val; } // 清理旧材质
    m_materials.clear();

    // 用于在解析OBJ面时，将v/vt/vn索引组合成唯一的Vertex，并获取其在m_vertices中的索引
    std::map<Vertex, unsigned int> uniqueVertices;

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "v") { // 顶点位置
            glm::vec3 pos;
            ss >> pos.x >> pos.y >> pos.z;
            m_tempPositions.push_back(pos);
        }
        else if (type == "vt") { // 纹理坐标
            glm::vec2 tex;
            ss >> tex.x >> tex.y;
            m_tempTexCoords.push_back(tex);
        }
        else if (type == "f") { // 面数据 (v/vt 格式)
            // 如果是新的材质或第一个面，创建一个新的MeshPart
            if (m_meshes.empty() || m_meshes.back().materialName != currentMaterialName) {
                if (!m_meshes.empty()) { // 结束上一个meshPart
                    m_meshes.back().indexCount = static_cast<unsigned int>(m_indices.size() - m_meshes.back().baseIndex);
                }
                m_meshes.push_back({ static_cast<unsigned int>(m_indices.size()), 0, currentMaterialName });
            }

            std::string vertexString;
            // OBJ面可以是三角形或四边形。这里假设为三角形 (f v/vt v/vt v/vt)
            // 如果是四边形，需要进行三角化处理。
            for (int i = 0; i < 3; ++i) { // 读取3个顶点
                if (!(ss >> vertexString)) {
                    std::cerr << "ERROR: Malformed face line (expected 3 vertices) in OBJ file: " << line << std::endl;
                    // 如果读取失败，回退已添加的索引
                    m_indices.resize(m_indices.size() - i);
                    return false;
                }

                // 解析 v/vt 格式
                size_t pos_slash = vertexString.find('/');
                size_t tex_slash = vertexString.find('/', pos_slash + 1); // 找到第二个斜杠，如果有的话

                unsigned int v_idx = std::stoul(vertexString.substr(0, pos_slash));
                unsigned int vt_idx = 0;
                if (pos_slash != std::string::npos) { // 如果有纹理坐标
                    vt_idx = std::stoul(vertexString.substr(pos_slash + 1, tex_slash - (pos_slash + 1)));
                }
                // unsigned int vn_idx = 0; // 如果有法线

                // OBJ索引是1-based，转换为0-based
                Vertex tempVertex;
                if (v_idx > 0 && v_idx <= m_tempPositions.size()) {
                    tempVertex.position = m_tempPositions[v_idx - 1];
                }
                else {
                    std::cerr << "ERROR: Invalid vertex position index " << v_idx << " in OBJ file: " << line << std::endl;
                    return false;
                }
                if (vt_idx > 0 && vt_idx <= m_tempTexCoords.size()) {
                    tempVertex.texCoord = m_tempTexCoords[vt_idx - 1];
                }
                else {
                    // 如果没有纹理坐标或索引无效，给一个默认值
                    tempVertex.texCoord = glm::vec2(0.0f, 0.0f);
                    if (vt_idx != 0) { // 只有当vt_idx存在但无效时才警告
                        std::cerr << "WARNING: Invalid texture coordinate index " << vt_idx << " in OBJ file: " << line << std::endl;
                    }
                }
                // 如果有法线，这里也要处理 tempVertex.normal = m_tempNormals[vn_idx - 1];

                // 优化顶点：如果这个顶点（位置+纹理坐标）已经存在，则重用其索引
                // 否则，添加新顶点并记录新索引
                if (uniqueVertices.count(tempVertex) == 0) {
                    uniqueVertices[tempVertex] = static_cast<unsigned int>(m_vertices.size());
                    m_vertices.push_back(tempVertex);
                }
                m_indices.push_back(uniqueVertices[tempVertex]);
            }
        }
        else if (type == "mtllib") { // 材质库文件
            std::string mtlFileName;
            ss >> mtlFileName;
            // 假设mtl文件和obj文件在同一目录
            size_t lastSlash = objFilePath.find_last_of("/\\");
            std::string objBaseDir = (lastSlash == std::string::npos) ? "" : objFilePath.substr(0, lastSlash + 1);
            loadMaterials(objBaseDir + mtlFileName, objBaseDir); // 传递mtl文件路径和mtl文件所在的目录
        }
        else if (type == "usemtl") { // 使用材质
            ss >> currentMaterialName;
        }
        // 忽略其他OBJ行类型（如#注释, s平滑组等）
    }
    file.close();

    // 结束最后一个meshPart的indexCount
    if (!m_meshes.empty()) {
        m_meshes.back().indexCount = static_cast<unsigned int>(m_indices.size() - m_meshes.back().baseIndex);
    }

    std::cout << "DEBUG: OBJ raw data loaded. Positions: " << m_tempPositions.size()
        << ", TexCoords: " << m_tempTexCoords.size()
        << ", Final Vertices: " << m_vertices.size()
        << ", Final Indices: " << m_indices.size() << std::endl;
    return true;
}

// 从MTL文件中加载材质信息
bool Model::loadMaterials(const std::string& mtlFilePath, const std::string& mtlBaseDir) {
    std::cout << "DEBUG: Entering loadMaterials for " << mtlFilePath << std::endl;
    std::ifstream file(mtlFilePath);
    if (!file.is_open()) {
        std::cerr << "ERROR: Could not open MTL file: " << mtlFilePath << std::endl;
        std::cerr << "  System error (errno): " << errno << " - " << strerror(errno) << std::endl;
        return false;
    }

    std::string line;
    Material* currentMaterial = nullptr; // 当前正在解析的材质

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "newmtl") { // 新材质定义
            std::string matName;
            ss >> matName;
            currentMaterial = new Material(matName);
            m_materials[matName] = currentMaterial; // 存储到map中
        }
        else if (currentMaterial) { // 只有在定义了新材质后才解析其属性
            if (type == "map_Kd") { // 漫反射纹理贴图
                std::string textureRelativePath;
                ss >> textureRelativePath;
                currentMaterial->diffuseTexturePath = textureRelativePath;
            }
            else if (type == "Ks") { // 镜面反射颜色
                ss >> currentMaterial->specular.x >> currentMaterial->specular.y >> currentMaterial->specular.z;
            }
            // 忽略其他材质属性（如Ka, Kd, Ns等）
        }
    }
    file.close();

    // 加载所有材质的纹理
    unsigned int textureUnitCounter = 0; // 假设所有纹理都使用GL_TEXTURE0
    for (auto const& [name, mat] : m_materials) {
        if (mat->diffuseTexturePath.empty()) {
            std::cout << "DEBUG: Material '" << name << "' has no diffuse texture path." << std::endl;
        }
        else {
            mat->loadDiffuseTexture(m_textureBaseDir, textureUnitCounter); // 使用Model构造函数传入的textureBaseDir
            // textureUnitCounter++; // 如果有多个纹理需要绑定到不同单元，则递增
        }
    }

    std::cout << "DEBUG: Loaded " << m_materials.size() << " materials from " << mtlFilePath << std::endl;
    return true;
}


// 计算模型的边界框（min_coords和max_coords）。
// 边界框用于后续的中心化和标准化缩放。
void Model::calculateBoundingBox() {
    // 初始化最小坐标为最大浮点数，最大坐标为最小浮点数
    m_minCoords = glm::vec3(std::numeric_limits<float>::max());
    m_maxCoords = glm::vec3(std::numeric_limits<float>::lowest());

    if (m_tempPositions.empty()) { // 使用原始位置计算边界框
        std::cerr << "WARNING: No raw positions to calculate bounding box." << std::endl;
        return;
    }

    // 遍历所有原始顶点位置，更新最小和最大坐标
    for (const auto& pos : m_tempPositions) {
        m_minCoords.x = std::min(m_minCoords.x, pos.x);
        m_minCoords.y = std::min(m_minCoords.y, pos.y);
        m_minCoords.z = std::min(m_minCoords.z, pos.z);
        m_maxCoords.x = std::max(m_maxCoords.x, pos.x);
        m_maxCoords.y = std::max(m_maxCoords.y, pos.y);
        m_maxCoords.z = std::max(m_maxCoords.z, pos.z);
    }
    std::cout << "DEBUG: Bounding Box: Min(" << m_minCoords.x << ", " << m_minCoords.y << ", " << m_minCoords.z << ") "
        << "Max(" << m_maxCoords.x << ", " << m_maxCoords.y << ", " << m_maxCoords.z << ")" << std::endl;
}

// 处理原始数据：中心化、标准化缩放，并优化顶点。
// 这里的m_vertices和m_indices已经在loadObjRawData中填充了优化后的数据。
// 此函数主要负责对这些优化后的顶点进行最终的局部空间变换。
void Model::processData() {
    if (m_vertices.empty()) {
        std::cerr << "WARNING: No vertices to process for final transformation." << std::endl;
        return;
    }

    // 计算模型的中心点 (使用之前计算的边界框)
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

    // 遍历所有优化后的顶点，应用初始变换
    for (auto& vertex : m_vertices) {
        glm::vec4 transformed_pos = initialTransform * glm::vec4(vertex.position, 1.0f);
        vertex.position = glm::vec3(transformed_pos);
    }

    std::cout << "DEBUG: Final vertices processed (centered and scaled)." << std::endl;
}

// 设置OpenGL缓冲区（VAO, VBO, EBO）。
// 将处理后的Vertex结构体和索引数据上传到GPU。
void Model::setupBuffers() {
    if (m_vertices.empty() || m_indices.empty()) {
        std::cerr << "ERROR: No data to setup OpenGL buffers for model." << std::endl;
        return;
    }
    std::cout << "DEBUG: Entering setupBuffers." << std::endl;

    // 1. 生成缓冲区对象ID
    GL_CALL(glGenBuffers(1, &m_vbo));  // 顶点VBO (存储Vertex结构体)
    GL_CALL(glGenBuffers(1, &m_ebo));     // 元素缓冲区EBO

    // 2. 生成VAO并绑定：VAO会记录所有后续的VBO/EBO绑定和顶点属性配置
    GL_CALL(glGenVertexArrays(1, &m_vao));
    GL_CALL(glBindVertexArray(m_vao));

    // 3. 绑定并填充顶点数据到VBO (m_vertices)
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));
    // 将m_vertices向量的数据直接传入GPU。&m_vertices[0]获取底层Vertex数组的指针。
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(Vertex), &m_vertices[0], GL_STATIC_DRAW));

    // 4. 配置顶点属性指针
    // 4.1 位置属性 (layout location = 0)
    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position)));

    // 4.2 纹理坐标属性 (layout location = 2)
    GL_CALL(glEnableVertexAttribArray(2)); // 注意：这里是2，因为位置是0，法线（如果存在）是1
    GL_CALL(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord)));

    // 5. 绑定并填充索引数据到EBO
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo));
    // 将m_indices向量的数据直接传入GPU。
    GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(unsigned int), &m_indices[0], GL_STATIC_DRAW));

    // 6. 解绑VAO：在配置完成后解绑VAO是一个好习惯，防止在其他地方意外修改它的状态。
    GL_CALL(glBindVertexArray(0));
    // 解绑VBO和EBO：一旦VAO记录了它们的绑定和配置，VBO和EBO就可以解绑了。
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
    std::cout << "DEBUG: Exiting setupBuffers. VAO ID: " << m_vao << std::endl;
}