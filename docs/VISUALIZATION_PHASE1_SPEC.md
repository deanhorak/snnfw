# SNNFW Visualization - Phase 1 Implementation Specification

**Version:** 1.0  
**Date:** 2025-11-23  
**Phase:** Foundation (Phase 1 of 5)

---

## Overview

Phase 1 establishes the foundational infrastructure for the SNNFW visualization subsystem. This includes OpenGL context creation, basic rendering capabilities, camera system, and ImGui integration.

---

## Dependencies Status

✅ **GLFW 3.3.9** - Window management (installed in `third_party/glfw`)  
✅ **GLAD (GL 4.5 Core)** - OpenGL loader (generated in `third_party/glad`)  
✅ **GLM 0.9.9.8** - Mathematics library (installed in `third_party/glm`)  
✅ **Dear ImGui 1.90.1** - GUI framework (installed in `third_party/imgui`)

---

## Phase 1 Deliverables

### 1. VisualizationManager Class

**File:** `include/snnfw/VisualizationManager.h`, `src/VisualizationManager.cpp`

**Responsibilities:**
- GLFW window creation and management
- OpenGL context initialization
- Main rendering loop
- Event handling (keyboard, mouse)
- ImGui integration
- View management (add/remove/switch views)

**Key Methods:**
```cpp
class VisualizationManager {
public:
    VisualizationManager(int width, int height, const std::string& title);
    ~VisualizationManager();
    
    void run();  // Main loop
    void close();
    bool shouldClose() const;
    
    void setTargetFPS(int fps);
    void enableVSync(bool enable);
    void setBackgroundColor(float r, float g, float b);
    
private:
    void initGLFW();
    void initGLAD();
    void initImGui();
    void processInput();
    void render();
    void cleanup();
    
    GLFWwindow* window_;
    int width_, height_;
    std::string title_;
    bool vsync_;
    int targetFPS_;
    glm::vec3 backgroundColor_;
};
```

---

### 2. Camera Class

**File:** `include/snnfw/Camera.h`, `src/Camera.cpp`

**Responsibilities:**
- View and projection matrix generation
- Camera transformations (orbit, pan, zoom)
- Look-at functionality
- Perspective/orthographic projection

**Key Methods:**
```cpp
class Camera {
public:
    Camera();
    Camera(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up);
    
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspect) const;
    
    void orbit(float deltaYaw, float deltaPitch);
    void pan(float deltaX, float deltaY);
    void zoom(float delta);
    void lookAt(const glm::vec3& target);
    
    void setPosition(const glm::vec3& position);
    void setTarget(const glm::vec3& target);
    void setFOV(float fov);
    void setNearFar(float near, float far);
    
    glm::vec3 getPosition() const;
    glm::vec3 getTarget() const;
    glm::vec3 getForward() const;
    glm::vec3 getRight() const;
    glm::vec3 getUp() const;
    
private:
    glm::vec3 position_;
    glm::vec3 target_;
    glm::vec3 up_;
    float fov_;
    float nearPlane_;
    float farPlane_;
    float distance_;  // Distance from target
    float yaw_;
    float pitch_;
};
```

---

### 3. ShaderManager Class

**File:** `include/snnfw/ShaderManager.h`, `src/ShaderManager.cpp`

**Responsibilities:**
- Shader compilation and linking
- Shader program management
- Uniform setting
- Error reporting

**Key Methods:**
```cpp
class ShaderManager {
public:
    ShaderManager();
    ~ShaderManager();
    
    GLuint loadShader(const std::string& name, 
                      const std::string& vertexPath, 
                      const std::string& fragmentPath);
    
    GLuint getShader(const std::string& name) const;
    void useShader(const std::string& name);
    void deleteShader(const std::string& name);
    
    // Uniform setters
    void setUniform(const std::string& name, int value);
    void setUniform(const std::string& name, float value);
    void setUniform(const std::string& name, const glm::vec3& value);
    void setUniform(const std::string& name, const glm::vec4& value);
    void setUniform(const std::string& name, const glm::mat4& value);
    
private:
    GLuint compileShader(const std::string& source, GLenum type);
    GLuint linkProgram(GLuint vertexShader, GLuint fragmentShader);
    std::string readFile(const std::string& path);
    void checkCompileErrors(GLuint shader, const std::string& type);
    
    std::map<std::string, GLuint> shaders_;
    GLuint currentShader_;
};
```

---

### 4. Basic Geometry Rendering

**File:** `include/snnfw/GeometryRenderer.h`, `src/GeometryRenderer.cpp`

**Responsibilities:**
- Render basic shapes (cube, sphere, line)
- Vertex buffer management
- Instanced rendering support

**Key Methods:**
```cpp
class GeometryRenderer {
public:
    GeometryRenderer();
    ~GeometryRenderer();
    
    void renderCube(const glm::mat4& transform, const glm::vec4& color);
    void renderSphere(const glm::mat4& transform, const glm::vec4& color, int segments = 16);
    void renderLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color);
    
    void cleanup();
    
private:
    void initCube();
    void initSphere(int segments);
    void initLine();
    
    GLuint cubeVAO_, cubeVBO_;
    GLuint sphereVAO_, sphereVBO_, sphereEBO_;
    GLuint lineVAO_, lineVBO_;
    int sphereVertexCount_;
};
```

---

### 5. Demo Application

**File:** `examples/visualization_demo.cpp`

**Purpose:** Demonstrate Phase 1 capabilities

**Features:**
- Rotating cube
- Camera controls (mouse orbit, WASD pan, scroll zoom)
- ImGui control panel (FPS, background color, cube color)
- Performance stats display

**Code Structure:**
```cpp
int main() {
    // Initialize visualization
    VisualizationManager vizManager(1280, 720, "SNNFW Visualization Demo");
    vizManager.setTargetFPS(60);
    vizManager.enableVSync(true);
    
    // Create camera
    Camera camera(glm::vec3(0, 0, 5), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    
    // Load shaders
    ShaderManager shaderManager;
    shaderManager.loadShader("basic", "shaders/basic.vert", "shaders/basic.frag");
    
    // Create geometry renderer
    GeometryRenderer geometryRenderer;
    
    // Main loop
    float rotation = 0.0f;
    while (!vizManager.shouldClose()) {
        // Update
        rotation += 0.01f;
        
        // Render
        glm::mat4 model = glm::rotate(glm::mat4(1.0f), rotation, glm::vec3(0, 1, 0));
        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 projection = camera.getProjectionMatrix(16.0f / 9.0f);
        
        shaderManager.useShader("basic");
        shaderManager.setUniform("model", model);
        shaderManager.setUniform("view", view);
        shaderManager.setUniform("projection", projection);
        
        geometryRenderer.renderCube(model, glm::vec4(0.2f, 0.5f, 1.0f, 1.0f));
        
        // ImGui
        ImGui::Begin("Controls");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::End();
        
        vizManager.render();
    }
    
    return 0;
}
```

---

## Shader Files

### Basic Vertex Shader (`shaders/basic.vert`)

```glsl
#version 450 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;
out vec3 Normal;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
```

### Basic Fragment Shader (`shaders/basic.frag`)

```glsl
#version 450 core

in vec3 FragPos;
in vec3 Normal;

uniform vec4 color;
uniform vec3 lightPos;
uniform vec3 viewPos;

out vec4 FragColor;

void main() {
    // Ambient
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * color.rgb;
    
    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * color.rgb;
    
    // Specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * vec3(1.0);
    
    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, color.a);
}
```

---

## CMake Integration

Add to `CMakeLists.txt`:

```cmake
# Visualization dependencies
set(GLFW_DIR ${CMAKE_SOURCE_DIR}/third_party/glfw)
set(GLAD_DIR ${CMAKE_SOURCE_DIR}/third_party/glad)
set(GLM_DIR ${CMAKE_SOURCE_DIR}/third_party/glm)
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)

# GLFW
add_subdirectory(${GLFW_DIR})

# GLAD
add_library(glad STATIC ${GLAD_DIR}/src/glad.c)
target_include_directories(glad PUBLIC ${GLAD_DIR}/include)

# ImGui
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw glad)

# Visualization sources
set(VISUALIZATION_SOURCES
    src/VisualizationManager.cpp
    src/Camera.cpp
    src/ShaderManager.cpp
    src/GeometryRenderer.cpp
)

# Add to snnfw library
target_sources(snnfw PRIVATE ${VISUALIZATION_SOURCES})
target_include_directories(snnfw PUBLIC ${GLM_DIR})
target_link_libraries(snnfw PUBLIC glfw glad imgui)

# Demo executable
add_executable(visualization_demo examples/visualization_demo.cpp)
target_link_libraries(visualization_demo snnfw)
```

---

## Testing Strategy

### Unit Tests

**File:** `tests/test_visualization_foundation.cpp`

**Tests:**
1. Camera matrix generation
2. Camera transformations (orbit, pan, zoom)
3. Shader compilation and linking
4. Geometry buffer creation

### Integration Tests

1. Window creation and OpenGL context
2. ImGui rendering
3. Basic geometry rendering
4. Camera controls

### Manual Testing

1. Run demo application
2. Verify smooth rotation
3. Test camera controls (mouse, keyboard)
4. Verify ImGui responsiveness
5. Check FPS counter accuracy

---

## Success Criteria

✅ Window opens with OpenGL 4.5 context  
✅ Rotating cube renders correctly  
✅ Camera controls work smoothly  
✅ ImGui panel displays and updates  
✅ FPS counter shows 60 FPS (with VSync)  
✅ No OpenGL errors  
✅ Clean shutdown (no memory leaks)

---

## Timeline

**Estimated Time:** 2-3 days

**Day 1:**
- VisualizationManager implementation
- Camera implementation
- Basic window and OpenGL setup

**Day 2:**
- ShaderManager implementation
- GeometryRenderer implementation
- Shader files creation

**Day 3:**
- Demo application
- Testing and debugging
- Documentation

---

## Next Phase

After Phase 1 completion, proceed to **Phase 2: Network Structure Visualization** which will implement:
- NetworkGraphRenderer
- Network data adapters
- Hierarchical layout algorithms
- Interactive selection

---

**Last Updated:** 2025-11-23  
**Status:** Ready for Implementation


