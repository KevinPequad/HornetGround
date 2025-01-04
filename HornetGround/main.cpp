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

#define SERVER_IP "192.168.102.188" // Replace with the server's IP address
#define PORT 8080
const int MAX_FRAME_SIZE = 150000; // 10kb

void setup_imgui(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    ImGui::StyleColorsDark();
}

void cleanup_imgui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW." << std::endl;
        return -1;
    }

    // Create a GLFW window
    GLFWwindow* window = glfwCreateWindow(800, 600, "Signal Strength and Bytes", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window." << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Setup ImGui
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
    int total_bytes_received = 0;
    int signal_strength = 0; // Mock value for now; you can implement signal strength logic

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Step 1: Receive frame size
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

        // Mock signal strength logic (replace with actual implementation if available)
        signal_strength = rand() % 100;

        // Step 3: Decode frame
        frame = cv::imdecode(buffer, cv::IMREAD_COLOR);

        // Render the frame using ImGui
        if (!frame.empty()) {
            cv::imshow("Received Video", frame);
            if (cv::waitKey(1) == 'q') {
                break;
            }
        }

        // ImGui UI
        ImGui::Begin("Network Stats");
        ImGui::Text("Signal Strength: %d%%", signal_strength);
        ImGui::Text("Total Bytes Received: %d", total_bytes_received);
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
    cleanup_imgui();
    glfwDestroyWindow(window);
    glfwTerminate();
    close(sock);

    return 0;
}
