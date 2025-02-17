#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <random>
#include <ctime>

// Constants
const int WINDOW_WIDTH = 1366;
const int WINDOW_HEIGHT = 768;
const float GALAXY_SIZE = 100000.0f; // Light years
const int NUM_STARS = 1000000;
const double G = 6.67430e-11; // Gravitational constant
const float SIMULATION_SPEED = 1.0f; // Years per second
const float BLACK_HOLE_MASS = 4.154e6; // Solar masses (Sagittarius A*)

// Vertex shader
const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in float aSize;
    layout (location = 2) in vec3 aColor;
    
    uniform mat4 projection;
    uniform mat4 view;
    
    out vec3 Color;
    
    void main() {
        gl_Position = projection * view * vec4(aPos, 1.0);
        gl_PointSize = aSize / gl_Position.w;
        Color = aColor;
    }
)";

// Fragment shader
const char* fragmentShaderSource = R"(
    #version 330 core
    in vec3 Color;
    out vec4 FragColor;
    
    void main() {
        vec2 circCoord = 2.0 * gl_PointCoord - 1.0;
        float circle = 1.0 - step(1.0, dot(circCoord, circCoord));
        FragColor = vec4(Color, circle);
    }
)";

struct Star {
    glm::vec3 position;
    glm::vec3 velocity;
    float mass;
    float size;
    bool isBlackHole;
};

class GalaxySimulation {
private:
    std::vector<Star> stars;
    GLuint VAO, VBO;
    GLuint shaderProgram;
    
    // Camera parameters
    glm::vec3 cameraPos;
    glm::vec3 cameraFront;
    glm::vec3 cameraUp;
    
    float lastX = WINDOW_WIDTH / 2.0f;
    float lastY = WINDOW_HEIGHT / 2.0f;
    float yaw = -90.0f;
    float pitch = 0.0f;
    
    void initializeShaders() {
        // Vertex shader compilation
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);
        
        // Fragment shader compilation
        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);
        
        // Shader program
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);
        
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }
    
    void generateStars() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> massDist(1.0f, 0.5f); // Solar masses
        std::uniform_real_distribution<float> posDist(-GALAXY_SIZE/2, GALAXY_SIZE/2);
        std::uniform_real_distribution<float> velDist(-100.0f, 100.0f); // km/s
        
        // Create central black hole
        Star blackHole;
        blackHole.position = glm::vec3(0.0f);
        blackHole.velocity = glm::vec3(0.0f);
        blackHole.mass = BLACK_HOLE_MASS;
        blackHole.size = 20.0f; // Larger visible size for visualization
        blackHole.isBlackHole = true;
        stars.push_back(blackHole);
        
        // Generate random stars
        for (int i = 0; i < NUM_STARS; i++) {
            Star star;
            star.position = glm::vec3(posDist(gen), posDist(gen), posDist(gen));
            star.velocity = glm::vec3(velDist(gen), velDist(gen), velDist(gen));
            star.mass = std::max(0.1f, massDist(gen));
            star.size = 2.0f + star.mass * 0.5f; // Visual size based on mass
            star.isBlackHole = false;
            stars.push_back(star);
        }
    }
    
    void updateStarPositions(float deltaTime) {
        #pragma omp parallel for
        for (size_t i = 0; i < stars.size(); i++) {
            if (stars[i].isBlackHole) continue;
            
            glm::vec3 totalForce(0.0f);
            
            // Calculate gravitational force from black hole (index 0)
            glm::vec3 r = stars[0].position - stars[i].position;
            float distance = glm::length(r);
            float forceMagnitude = G * stars[0].mass * stars[i].mass / (distance * distance);
            totalForce += forceMagnitude * glm::normalize(r);
            
            // Update velocity and position
            stars[i].velocity += totalForce / stars[i].mass * deltaTime;
            stars[i].position += stars[i].velocity * deltaTime;
        }
    }
    
public:
    GalaxySimulation() {
        generateStars();
        initializeShaders();
        
        // Initialize camera
        cameraPos = glm::vec3(0.0f, GALAXY_SIZE/4, GALAXY_SIZE/4);
        cameraFront = glm::vec3(0.0f, -1.0f, -1.0f);
        cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
        
        // Initialize OpenGL buffers
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        
        // Buffer layout setup
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, stars.size() * sizeof(Star), &stars[0], GL_DYNAMIC_DRAW);
        
        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Star), (void*)0);
        glEnableVertexAttribArray(0);
        
        // Size attribute
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(Star), (void*)offsetof(Star, size));
        glEnableVertexAttribArray(1);
    }
    
    void render() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);
        
        // Update view/projection matrices
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 
            (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, 0.1f, GALAXY_SIZE * 2.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        
        // Set uniforms
        GLuint projLoc = glGetUniformLocation(shaderProgram, "projection");
        GLuint viewLoc = glGetUniformLocation(shaderProgram, "view");
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        
        // Draw stars
        glBindVertexArray(VAO);
        glDrawArrays(GL_POINTS, 0, stars.size());
    }
    
    void update(float deltaTime) {
        updateStarPositions(deltaTime * SIMULATION_SPEED);
        
        // Update VBO with new positions
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, stars.size() * sizeof(Star), &stars[0]);
    }
    
    void processInput(GLFWwindow* window, float deltaTime) {
        const float cameraSpeed = 1000.0f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            cameraPos += cameraSpeed * cameraFront;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            cameraPos -= cameraSpeed * cameraFront;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    }
};

int main() {
    // Initialize GLFW and OpenGL
    if (!glfwInit()) {
        return -1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Galaxy Simulation", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    
    if (glewInit() != GLEW_OK) {
        return -1;
    }
    
    // Enable depth testing and point sprites
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Create and initialize simulation
    GalaxySimulation simulation;
    
    float lastFrame = 0.0f;
    
    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        
        simulation.processInput(window, deltaTime);
        simulation.update(deltaTime);
        simulation.render();
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    glfwTerminate();
    return 0;
}
