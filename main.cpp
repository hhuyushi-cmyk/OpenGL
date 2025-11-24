#include <iostream>

// 引入自定义框架和第三方库的头文件
#include "glframework/core.h"        // 核心库头文件 (GLAD, GLFW, GLM)
#include "glframework/shader.h"      // 自定义Shader类
#include "glframework/model.h"       // <<< 引入自定义Model类
// #include "glframework/texture.h" // <<< 移除：Texture现在由Model/Material管理
#include "application/Application.h" // 自定义Application单例类
#include "wrapper/checkError.h"      // OpenGL错误检查宏和函数

// 引入相机+控制器
#include "application/camera/perspectiveCamera.h"
#include "application/camera/orthographicCamera.h"
#include "application/camera/trackBallCameraControl.h"
#include "application/camera/GameCameraControl.h"



// 全局变量声明：
// -----------------------------------------------------------------------------
Shader* shader = nullptr; // Shader类实例指针，管理着色器程序
Model* myModel = nullptr; // Model类实例指针，管理模型数据和MVP矩阵

// 摄像机和控制器实例
PerspectiveCamera* camera = nullptr;
TrackBallCameraControl* cameraControl = nullptr;

// 用于计算deltaTime
double g_lastFrameTime = 0.0;

// -----------------------------------------------------------------------------


// OnResize 回调函数：
// --------------------
void OnResize(int width, int height) {
    GL_CALL(glViewport(0, 0, width, height));
    std::cout << "OnResize" << std::endl;
}

// OnKey 回调函数：
// -----------------
void OnKey(int key, int action, int mods) {
    if (cameraControl) {
        cameraControl->onKey(key, action, mods);
    }
}

// OnMouse 回调函数：
// --------------------
void OnMouse(int button, int action, int mods) {
    double x, y;
    app->getCursorPosition(&x, &y);
    if (cameraControl) {
        cameraControl->onMouse(button, action, x, y);
    }
}

// OnCursor 回调函数：
// --------------------
void OnCursor(double xpos, double ypos) {
    if (cameraControl) {
        cameraControl->onCursor(xpos, ypos);
    }
}

// OnScroll 回调函数：
// --------------------
void OnScroll(double offset) {
    if (cameraControl) {
        cameraControl->onScroll(static_cast<float>(offset));
    }
}

// prepareShader 函数：
// --------------------
void prepareShader() {
    shader = new Shader("assets/shaders/vertex.glsl", "assets/shaders/fragment.glsl");
}

// prepareModel 函数：
// ------------------
void prepareModel() {
    // 创建Model对象，传入OBJ文件路径和纹理基目录
    // 纹理基目录通常是OBJ文件所在的目录，或者MTL文件中map_Kd路径的根目录
    // 假设您的OBJ文件在 assets/models/your_new_model.obj
    // MTL文件在 assets/models/your_new_model.mtl
    // 纹理在 assets/models/materials_textures/
    // 那么 textureBaseDir 应该是 "assets/models/"
    myModel = new Model("C:/Users/16344/Desktop/DEHHALKAJ000160N/lod3.obj", "C:/Users/16344/Desktop/DEHHALKAJ000160N" ); // <<< 确保文件路径和纹理基目录正确 !!!

    if (myModel) {
        myModel->setPosition(glm::vec3(0.0f, 0.0f, 0.0f)); // 模型在世界原点
        myModel->setRotation(0.0f, glm::vec3(0.0f, 1.0f, 0.0f)); // 初始无旋转
        myModel->setScale(glm::vec3(1.0f)); // 默认缩放
    }
}

// prepareCameraAndControl 函数：
// -----------------------------
void prepareCameraAndControl() {
    camera = new PerspectiveCamera(
        60.0f,
        (float)app->getWidth() / (float)app->getHeight(),
        0.1f,
        1000.0f
    );
    cameraControl = new TrackBallCameraControl();
    cameraControl->setCamera(camera);
    cameraControl->setSensitivity(0.4f);
}

// prepareState 函数：
// --------------------
void prepareState() {
    GL_CALL(glEnable(GL_DEPTH_TEST));
    GL_CALL(glDepthFunc(GL_LESS));
}

// render 函数：
// -------------
void render() {
    GL_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    shader->begin();

    // 将摄像机的视图矩阵和投影矩阵传递给Model对象
    // Model::draw() 会负责将这些矩阵和它自己的模型矩阵一起发送到着色器
    // 并且Model::draw() 也会负责绑定其内部的纹理
    if (myModel && camera) {
        myModel->setViewMatrix(camera->getViewMatrix());
        myModel->setProjectionMatrix(camera->getProjectionMatrix());
        myModel->draw(*shader); // 绘制模型
    }

    shader->end();
}


// main 函数：
// -----------
int main() {
    if (!app->init(800, 600)) {
        return -1;
    }

    app->setResizeCallback(OnResize);
    app->setKeyBoardCallback(OnKey);
    app->setMouseCallback(OnMouse);
    app->setCursorCallback(OnCursor);
    app->setScrollCallback(OnScroll);

    GL_CALL(glViewport(0, 0, app->getWidth(), app->getHeight()));
    GL_CALL(glClearColor(0.2f, 0.3f, 0.3f, 1.0f));

    prepareShader();
    // prepareVAO(); // <<< 移除：VAO现在由Model管理
    // prepareTexture(); // <<< 移除：Texture现在由Model/Material管理
    prepareModel();
    prepareCameraAndControl();
    prepareState();

    g_lastFrameTime = glfwGetTime();

    while (app->update()) {
        cameraControl->update();
        render();
    }

    app->destroy();

    return 0;
}