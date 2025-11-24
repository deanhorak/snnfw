# SNNFW Visualization Quick Start Guide

This guide will help you get started with the SNNFW visualization subsystem.

---

## Installation

### 1. Install System Dependencies (Linux)

```bash
# Run the provided installation script
./scripts/install_visualization_deps.sh

# Or manually install:
sudo apt-get install -y \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    libx11-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev
```

### 2. Build the Project

```bash
cd build
cmake ..
make snnfw -j$(nproc)
make visualization_demo -j$(nproc)
```

### 3. Run the Demo

```bash
./visualization_demo
```

---

## Basic Usage

### Creating a Visualization Window

```cpp
#include "snnfw/VisualizationManager.h"

// Create a window (width, height, title)
VisualizationManager vizManager(1280, 720, "My Visualization");

// Configure
vizManager.setTargetFPS(60);
vizManager.enableVSync(true);
vizManager.setBackgroundColor(0.1f, 0.1f, 0.15f);

// Run the main loop
vizManager.run();
```

### Setting Up a Camera

```cpp
#include "snnfw/Camera.h"
#include <glm/glm.hpp>

// Create camera (position, target, up)
Camera camera(
    glm::vec3(0, 2, 5),    // Position
    glm::vec3(0, 0, 0),    // Look at target
    glm::vec3(0, 1, 0)     // Up vector
);

// Configure
camera.setFOV(45.0f);
camera.setNearPlane(0.1f);
camera.setFarPlane(100.0f);

// Get matrices for rendering
glm::mat4 view = camera.getViewMatrix();
glm::mat4 projection = camera.getProjectionMatrix(aspectRatio);
```

### Camera Controls

```cpp
// In your update callback:
vizManager.setUpdateCallback([&](double deltaTime) {
    GLFWwindow* window = vizManager.getWindow();
    
    // Mouse orbit (left button)
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        float deltaX = /* mouse movement X */ * 0.01f;
        float deltaY = /* mouse movement Y */ * 0.01f;
        camera.orbit(deltaX, -deltaY);
    }
    
    // Keyboard pan (WASD)
    float panSpeed = 2.0f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.pan(0, panSpeed);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.pan(0, -panSpeed);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.pan(-panSpeed, 0);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.pan(panSpeed, 0);
    
    // Zoom
    camera.zoom(scrollDelta * 0.5f);
});
```

### Loading and Using Shaders

```cpp
#include "snnfw/ShaderManager.h"

ShaderManager shaderManager;

// Load shader from files
GLuint shader = shaderManager.loadShader(
    "basic",                    // Name
    "shaders/basic.vert",       // Vertex shader path
    "shaders/basic.frag"        // Fragment shader path
);

// Use shader
shaderManager.useShader("basic");

// Set uniforms
shaderManager.setUniform("model", modelMatrix);
shaderManager.setUniform("view", viewMatrix);
shaderManager.setUniform("projection", projectionMatrix);
shaderManager.setUniform("color", glm::vec4(1.0f, 0.5f, 0.2f, 1.0f));
shaderManager.setUniform("lightPos", glm::vec3(5.0f, 5.0f, 5.0f));
shaderManager.setUniform("viewPos", camera.getPosition());
```

### Rendering Geometry

```cpp
#include "snnfw/GeometryRenderer.h"
#include <glm/gtc/matrix_transform.hpp>

GeometryRenderer geometryRenderer;

// In your render callback:
vizManager.setRenderCallback([&](double deltaTime) {
    // Activate shader
    shaderManager.useShader("basic");
    
    // Set view/projection
    shaderManager.setUniform("view", camera.getViewMatrix());
    shaderManager.setUniform("projection", camera.getProjectionMatrix(aspectRatio));
    
    // Render a cube
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0, 0, 0));
    model = glm::rotate(model, rotation, glm::vec3(0, 1, 0));
    
    shaderManager.setUniform("model", model);
    shaderManager.setUniform("color", glm::vec4(0.2f, 0.5f, 1.0f, 1.0f));
    
    geometryRenderer.renderCube();
    
    // Render a sphere
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(2, 0, 0));
    
    shaderManager.setUniform("model", model);
    shaderManager.setUniform("color", glm::vec4(1.0f, 0.3f, 0.2f, 1.0f));
    
    geometryRenderer.renderSphere();
});
```

### Adding ImGui Controls

```cpp
#include <imgui.h>

// In your render callback (after rendering geometry):
ImGui::Begin("My Controls");

ImGui::Text("FPS: %.1f", vizManager.getFPS());

static bool showCube = true;
ImGui::Checkbox("Show Cube", &showCube);

static float color[4] = {0.2f, 0.5f, 1.0f, 1.0f};
ImGui::ColorEdit4("Cube Color", color);

static float rotationSpeed = 1.0f;
ImGui::SliderFloat("Rotation Speed", &rotationSpeed, 0.0f, 5.0f);

ImGui::End();
```

---

## Complete Example

```cpp
#include "snnfw/VisualizationManager.h"
#include "snnfw/Camera.h"
#include "snnfw/ShaderManager.h"
#include "snnfw/GeometryRenderer.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

int main() {
    // Create visualization manager
    VisualizationManager vizManager(1280, 720, "SNNFW Visualization");
    vizManager.setTargetFPS(60);
    vizManager.enableVSync(true);
    
    // Create camera
    Camera camera(glm::vec3(0, 2, 5), glm::vec3(0, 0, 0));
    
    // Create shader manager and load shaders
    ShaderManager shaderManager;
    shaderManager.loadShader("basic", "shaders/basic.vert", "shaders/basic.frag");
    
    // Create geometry renderer
    GeometryRenderer geometryRenderer;
    
    // State
    float rotation = 0.0f;
    
    // Update callback
    vizManager.setUpdateCallback([&](double deltaTime) {
        rotation += deltaTime;
        
        // Camera controls here...
    });
    
    // Render callback
    vizManager.setRenderCallback([&](double deltaTime) {
        // Use shader
        shaderManager.useShader("basic");
        
        // Set matrices
        shaderManager.setUniform("view", camera.getViewMatrix());
        shaderManager.setUniform("projection", 
            camera.getProjectionMatrix(vizManager.getAspectRatio()));
        shaderManager.setUniform("viewPos", camera.getPosition());
        shaderManager.setUniform("lightPos", glm::vec3(5, 5, 5));
        
        // Render cube
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::rotate(model, rotation, glm::vec3(0, 1, 0));
        
        shaderManager.setUniform("model", model);
        shaderManager.setUniform("color", glm::vec4(0.2f, 0.5f, 1.0f, 1.0f));
        
        geometryRenderer.renderCube();
        
        // ImGui
        ImGui::Begin("Controls");
        ImGui::Text("FPS: %.1f", vizManager.getFPS());
        ImGui::Text("Rotation: %.2f", rotation);
        ImGui::End();
    });
    
    // Run
    vizManager.run();
    
    return 0;
}
```

---

## Shader Format

### Vertex Shader (basic.vert)

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

### Fragment Shader (basic.frag)

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
    vec3 ambient = 0.3 * color.rgb;
    
    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * color.rgb;
    
    // Specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = 0.5 * spec * vec3(1.0);
    
    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, color.a);
}
```

---

## Troubleshooting

### Build Errors

**Error:** `RandR headers not found`
```bash
# Install X11 development libraries
sudo apt-get install libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

**Error:** `glad/glad.h not found`
```bash
# Regenerate GLAD
cd third_party/glad_src
python -m glad --generator=c --spec=gl --profile=core --api=gl=4.5 --out-path=../glad
```

### Runtime Errors

**Error:** `Failed to initialize GLFW`
- Check that X11 is running (for Linux)
- Check that display is available

**Error:** `Failed to initialize GLAD`
- Check OpenGL version: `glxinfo | grep "OpenGL version"`
- Ensure OpenGL 4.5 is supported

**Error:** `Failed to load shaders`
- Check that shader files exist in `shaders/` directory
- Check shader syntax for errors

### Performance Issues

**Low FPS:**
- Disable VSync: `vizManager.enableVSync(false);`
- Reduce geometry complexity
- Check GPU usage

**High CPU usage:**
- Enable VSync: `vizManager.enableVSync(true);`
- Reduce target FPS: `vizManager.setTargetFPS(30);`

---

## Next Steps

1. **Run the demo:** `./visualization_demo`
2. **Experiment with controls:** Mouse orbit, WASD pan
3. **Modify colors:** Use ImGui color pickers
4. **Read the design docs:** `docs/VISUALIZATION_DESIGN.md`
5. **Explore Phase 2:** Network structure visualization (coming soon)

---

## API Reference

See the full API documentation in:
- `docs/VISUALIZATION_DESIGN.md` - Complete architecture
- `docs/VISUALIZATION_PHASE1_SPEC.md` - Phase 1 specification
- Header files in `include/snnfw/` - Inline documentation

---

**For more information, see:**
- `docs/VISUALIZATION_DESIGN.md`
- `docs/VISUALIZATION_IMPLEMENTATION_STATUS.md`
- `examples/visualization_demo.cpp`


