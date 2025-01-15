#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <opencv2/opencv.hpp>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <GL/gl3w.h> // OpenGL loader for textures

#define SERVER_IP "192.168.102.188" // Replace with the server's IP address
#define PORT 8080
const int MAX_FRAME_SIZE = 150000;

void setup_imgui(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 300 es");
    ImGui::StyleColorsDark();
}

void cleanup_imgui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

// Helper function to create OpenGL texture from cv::Mat
GLuint createTextureFromMat(const cv::Mat& mat) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLenum inputFormat = mat.channels() == 3 ? GL_BGR : GL_RED;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, mat.cols, mat.rows, 0, inputFormat, GL_UNSIGNED_BYTE, mat.data);
    glBindTexture(GL_TEXTURE_2D, 0);

    return textureID;
}

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW." << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Signal Strength and Video", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window." << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    setup_imgui(window);

    // Networking setup
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation failed." << std::endl;
        return -1;
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_address.sin_addr) <= 0) {
        std::cerr << "Invalid server IP address." << std::endl;
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        std::cerr << "Connection to server failed." << std::endl;
        close(sock);
        return -1;
    }

    std::cout << "Connected to server." << std::endl;

    cv::Mat frame;
    GLuint frameTexture = 0;
    int total_bytes_received = 0;
    int signal_strength = 0;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Receive frame size
        int frame_size;
        int size_received = recv(sock, &frame_size, sizeof(frame_size), 0);
        if (size_received != sizeof(frame_size)) {
            std::cerr << "Failed to receive frame size." << std::endl;
            break;
        }

        frame_size = ntohl(frame_size);
        if (frame_size <= 0 || frame_size > MAX_FRAME_SIZE) {
            std::cerr << "Invalid frame size: " << frame_size << std::endl;
            break;
        }

        std::vector<uchar> buffer(frame_size);
        int bytes_received = 0;
        while (bytes_received < frame_size) {
            int chunk = recv(sock, buffer.data() + bytes_received, frame_size - bytes_received, 0);
            if (chunk <= 0) {
                std::cerr << "Error receiving frame data." << std::endl;
                break;
            }
            bytes_received += chunk;
        }

        total_bytes_received += bytes_received;

        // Mock signal strength logic
        signal_strength = rand() % 100;

        // Decode frame
        frame = cv::imdecode(buffer, cv::IMREAD_COLOR);

        if (!frame.empty()) {
            if (frameTexture) {
                glDeleteTextures(1, &frameTexture);
            }
            frameTexture = createTextureFromMat(frame);
        }

        // ImGui UI
        ImGui::Begin("Network Stats");
        ImGui::Text("Signal Strength: %d%%", signal_strength);
        ImGui::Text("Total Bytes Received: %d", total_bytes_received);
        if (frameTexture && !frame.empty()) {
            ImGui::Text("Video Stream:");
            ImGui::Image((void*)(intptr_t)frameTexture, ImVec2(frame.cols, frame.rows));
        }
        ImGui::End();

        // Render ImGui
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Cleanup
    if (frameTexture) {
        glDeleteTextures(1, &frameTexture);
    }
    cleanup_imgui();
    glfwDestroyWindow(window);
    glfwTerminate();
    close(sock);

    return 0;
}
