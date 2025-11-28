#include "mesh.h"
#include "shader.h" // 需要Shader类来设置uniforms

// 构造函数：初始化Mesh数据并设置OpenGL缓冲区
Mesh::Mesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, Material* material)
    : m_vertices(vertices), m_indices(indices), m_material(material),
    m_vao(0), m_vbo(0), m_ebo(0)
{
    setupBuffers(); // 设置OpenGL缓冲区
    std::cout << "Mesh created with " << m_vertices.size() / 5 << " vertices and "
        << m_indices.size() << " indices." << std::endl;
}

Mesh::~Mesh() {
    // 释放OpenGL缓冲区资源
    if (m_vao != 0) {
        GL_CALL(glDeleteVertexArrays(1, &m_vao));
    }
    if (m_vbo != 0) {
        GL_CALL(glDeleteBuffers(1, &m_vbo));
    }
    if (m_ebo != 0) {
        GL_CALL(glDeleteBuffers(1, &m_ebo));
    }
    // 注意：m_material的生命周期由Model或LODModel管理，这里不delete
    std::cout << "Mesh destroyed." << std::endl;
}

// 绘制Mesh：绑定VAO，激活材质，并发出绘制指令
void Mesh::draw(Shader& shader) {
    // 确保VAO已成功创建且有数据可绘制
    if (m_vao == 0 || m_indices.empty()) {
        std::cerr << "WARNING: Attempted to draw mesh with uninitialized VAO or empty indices." << std::endl;
        return;
    }

    // 激活材质（绑定纹理等）
    if (m_material) {
        m_material->use(shader);
    }
    else {
        // 如果没有材质，可以设置一个默认颜色或解绑纹理
        // shader.setVector3("u_DiffuseColor", 1.0f, 0.0f, 1.0f); // 紫色作为默认
         GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    }

    // 绑定VAO，激活其记录的所有顶点属性和缓冲区
    GL_CALL(glBindVertexArray(m_vao));
    // 发出绘制指令，使用索引缓冲区绘制三角形
    GL_CALL(glDrawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, 0));
    // 解绑VAO，防止后续操作意外修改此VAO状态
    GL_CALL(glBindVertexArray(0));
}

// 设置OpenGL缓冲区：生成并填充VAO, VBO, EBO
void Mesh::setupBuffers() {
    if (m_vertices.empty() || m_indices.empty()) {
        std::cerr << "ERROR: No data to setup OpenGL buffers for mesh." << std::endl;
        return;
    }

    // 1. 生成缓冲区对象ID
    GL_CALL(glGenBuffers(1, &m_vbo));  // 顶点数据VBO (位置+纹理坐标)
    GL_CALL(glGenBuffers(1, &m_ebo));     // 元素缓冲区EBO

    // 2. 生成VAO并绑定：VAO会记录所有后续的VBO/EBO绑定和顶点属性配置
    GL_CALL(glGenVertexArrays(1, &m_vao));
    GL_CALL(glBindVertexArray(m_vao));

    // 3. 绑定并填充顶点数据到VBO
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));
    // 将m_vertices向量的数据直接传入GPU
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(float), &m_vertices[0], GL_STATIC_DRAW));

    // 4. 配置顶点属性指针
    // 每个顶点的布局是：位置(vec3) + 纹理坐标(vec2) = 5个float
    GLsizei stride = sizeof(float) * 5; // 每个顶点数据块的总大小

    // 位置属性 (layout location = 0): 3个float
    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0));

    // 纹理坐标属性 (layout location = 1): 2个float
    // 偏移量是3个float (跳过位置数据)
    GL_CALL(glEnableVertexAttribArray(1));
    GL_CALL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 3)));

    // 5. 绑定并填充索引数据到EBO
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo));
    GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(unsigned int), &m_indices[0], GL_STATIC_DRAW));

    // 6. 解绑VAO：在配置完成后解绑VAO是一个好习惯。
    GL_CALL(glBindVertexArray(0));
    // 解绑VBO和EBO：一旦VAO记录了它们的绑定和配置，VBO和EBO就可以解绑了。
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}