/*
Author: Pulkit Goel
Class: ECE6122 (section)
Last Date Modified: 10/26/2024
Description:
main function for game of chess
*/


// Include standard headers
#include <stdio.h>
#include <stdlib.h>
#include <vector>

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <glfw3.h>
GLFWwindow* window;

// Include GLM
//#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;


#include "assimp_model.h"
//#include "texture.h"

/*
#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <cmath>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <cmath> 
*/

// Include Windows headers for process and pipe management
#include <windows.h>

//Some global functions and variables

// Camera parameters
float cameraRadius = 50.0f;
float horizontalAngle = glm::pi<float>();
float verticalAngle = glm::radians(-20.0f);
bool specularDiffuseEnabled = true;

//Render the pieces
void renderPieces(std::vector<glm::vec3> piecePosition, GLuint ModelMatrixID, GLuint MatrixID, GLuint ColorID, CAssimpModel pieceModel, bool isWhite)
{
    // Compute the camera position in Cartesian coordinates
    glm::vec3 positionCamera = glm::vec3(cameraRadius * cos(verticalAngle) * sin(horizontalAngle), cameraRadius * sin(verticalAngle), cameraRadius * cos(verticalAngle) * cos(horizontalAngle));   
    // Camera looks at the origin
    glm::vec3 targetCamera = glm::vec3(0.0f, 0.0f, 0.0f);
    // Up vector
    glm::vec3 upVector = glm::vec3(0.0f, 1.0f, 0.0f);
    //View matrix
    glm::mat4 ViewMatrix = glm::lookAt(
        positionCamera,
        targetCamera,
        upVector
    );
    //Projection matrix
    glm::mat4 ProjectionMatrix = glm::perspective(
        glm::radians(60.0f),
        1024.0f / 768.0f,
        0.1f,
        100.0f
    );

    for (const auto& position : piecePosition) 
    {
        glm::mat4 pieceModelMatrix = glm::mat4(1.0f);

        // Apply scaling
        float pieceScale = 0.3f; 
        pieceModelMatrix = glm::scale(pieceModelMatrix, glm::vec3(pieceScale));

        // Rotate the piece if necessary
        pieceModelMatrix = glm::rotate(pieceModelMatrix, glm::radians(0.0f),
            glm::vec3(1.0f, 0.0f, 0.0f));

        // Translate the piece to its position
        pieceModelMatrix = glm::translate(pieceModelMatrix, position);

        // Compute MVP matrix
        glm::mat4 pieceMVP = ProjectionMatrix * ViewMatrix * pieceModelMatrix;

        // Send uniforms to the shader
        glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &pieceModelMatrix[0][0]);
        glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &pieceMVP[0][0]);

        if (isWhite) {
            glUniform4f(ColorID, 5.f, 5.f, 5.f, 1.0f);
        }
        else {
            glUniform4f(ColorID, 1.0f, 1.0f, 1.0f, 1.0f);
        }

        // Render the piece
        pieceModel.RenderModel();
    }
}

// Function to handle camera inputs
void keyBinds() {
    // Camera speed
    float cameraSpeed = 0.05f;
    float rotationSpeed = 0.002f;

    // 'W' Move close
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) 
    {
        cameraRadius -= cameraSpeed;
    }

    // 'S' Move farther
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) 
    {
        cameraRadius += cameraSpeed;
    }

    // 'A' Rotate left
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) 
    {
        horizontalAngle += rotationSpeed;
    }

    // 'D' Rotate right
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) 
    {
        horizontalAngle -= rotationSpeed;
    }

    // Up Rotate up
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) 
    {
        verticalAngle += rotationSpeed;
        if (verticalAngle > glm::radians(89.0f)) verticalAngle = glm::radians(89.0f);
    }

    // Down Rotate down
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) 
    {
        verticalAngle -= rotationSpeed;
        if (verticalAngle < glm::radians(-89.0f)) verticalAngle = glm::radians(-89.0f);
    }

    // 'L' key: Toggle specular and diffuse lighting
    static bool lKeyPressedLastFrame = false;
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) 
    {
        if (!lKeyPressedLastFrame) 
        {
            specularDiffuseEnabled = !specularDiffuseEnabled;
        }
        lKeyPressedLastFrame = true;
    }
    else 
    {
        lKeyPressedLastFrame = false;
    }

    // Escape key: Close the window
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) 
    {
        glfwSetWindowShouldClose(window, true);
    }
}

bool loadModel(CAssimpModel& model, char* filePath) 
{
    if (!model.LoadModelFromFile(filePath)) 
    {
        fprintf(stderr, "Failed to load model from %s\n", filePath);
        getchar();
        glfwTerminate();
        return false;
    }
    return true;
}

int main(void) {
    // Initialize GLFW
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        getchar();
        return -1;
    }

    glfwWindowHint(GLFW_SAMPLES, 4); // 4x antialiasing
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    // Open a window and create its OpenGL context
    window = glfwCreateWindow(1024, 768, "The Game of Chess", NULL, NULL);
    if (window == NULL) {
        fprintf(stderr, "Failed to open GLFW window.\n");
        getchar();
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Initialize GLEW
    glewExperimental = true; // Needed for core profile
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        getchar();
        glfwTerminate();
        return -1;
    }

    //Enable sticky keys
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

    // background
    glClearColor(0.8f, 0.8f, 0.8f, 0.0f);

    // Enable depth test
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Cull triangles facing away from the camera
    glEnable(GL_CULL_FACE);
    // Load the chessboard model
    CAssimpModel chessboardModel;
    if (!chessboardModel.LoadModelFromFile("../Lab3/Stone_Chess_Board/12951_Stone_Chess_Board_v1_L3.obj")) 
    {
        fprintf(stderr, "Failed to load chessboard OBJ file!!\n");
        getchar();
        glfwTerminate();
        return -1;
    }

    //Load pieces model using the function defined
    CAssimpModel pawnModel;
    if (!loadModel(pawnModel, "../Lab3/Chess/pawn.obj")) 
    {
        return -1;
    }
    CAssimpModel kingModel;
    if (!loadModel(kingModel, "../Lab3/Chess/king.obj"))
    {
        return -1;
    }
    CAssimpModel queenModel;
    if (!loadModel(queenModel, "../Lab3/Chess/queen.obj"))
    {
        return -1;
    }
    CAssimpModel rookModel;
    if (!loadModel(rookModel, "../Lab3/Chess/rook.obj"))
    {
        return -1;
    }
    CAssimpModel knightModel;
    if (!loadModel(knightModel, "../Lab3/Chess/knight.obj"))
    {
        return -1;
    }
    CAssimpModel bishopModel;
    if (!loadModel(bishopModel, "../Lab3/Chess/bishop.obj"))
    {
        return -1;
    }

    // Finalize VBO after loading all models
    CAssimpModel::FinalizeVBO();

    // Create and compile our GLSL program from the shaders
    GLuint programID = LoadShaders("StandardShading.vertexshader", "StandardShading.fragmentshader");

    // Get handles for our uniforms and attributes
    GLuint MatrixID = glGetUniformLocation(programID, "MVP");
    GLuint ViewMatrixID = glGetUniformLocation(programID, "V");
    GLuint ModelMatrixID = glGetUniformLocation(programID, "M");
    GLuint LightID = glGetUniformLocation(programID, "LightPosition_worldspace");
    GLuint SpecularDiffuseEnabledID = glGetUniformLocation(programID, "specularDiffuseEnabled");
    GLuint ColorID = glGetUniformLocation(programID, "pieceColor");

    // For frame timing
    double lastTime = glfwGetTime();
    int nFrames = 0;

    //Get constant values for setting up positions
    float boxSize = 18.0f; // Size of each square
    float ChessBoardSize = boxSize * 8;
    float halfChessBoardSize = ChessBoardSize / 2.0f;

    //Black pieces
    std::vector<glm::vec3> blackPawn;
    std::vector<glm::vec3> blackKing;
    std::vector<glm::vec3> blackQueen;
    std::vector<glm::vec3> blackBishop;
    std::vector<glm::vec3> blackKnight;
    std::vector<glm::vec3> blackRook;

    //White pieces
    std::vector<glm::vec3> whitePawn;
    std::vector<glm::vec3> whiteKing;
    std::vector<glm::vec3> whiteQueen;
    std::vector<glm::vec3> whiteBishop;
    std::vector<glm::vec3> whiteKnight;
    std::vector<glm::vec3> whiteRook;

    // White pieces positions
    for (int i = 0; i < 8; ++i) {
        float x = -halfChessBoardSize + boxSize / 2 + i * boxSize;
        float z = halfChessBoardSize - 1.5f * boxSize;
        whitePawn.push_back(glm::vec3(x, 0.0f, z));
    }
    whiteRook.push_back(glm::vec3(-halfChessBoardSize + boxSize / 2 + 0 * boxSize, 0.0f, halfChessBoardSize - 0.5f * boxSize));
    whiteKnight.push_back(glm::vec3(-halfChessBoardSize + boxSize / 2 + 1 * boxSize, 0.0f, halfChessBoardSize - 0.5f * boxSize));
    whiteBishop.push_back(glm::vec3(-halfChessBoardSize + boxSize / 2 + 2 * boxSize, 0.0f, halfChessBoardSize - 0.5f * boxSize));
    whiteQueen.push_back(glm::vec3(-halfChessBoardSize + boxSize / 2 + 3 * boxSize, 0.0f, halfChessBoardSize - 0.5f * boxSize));
    whiteKing.push_back(glm::vec3(-halfChessBoardSize + boxSize / 2 + 4 * boxSize, 0.0f, halfChessBoardSize - 0.5f * boxSize));
    whiteBishop.push_back(glm::vec3(-halfChessBoardSize + boxSize / 2 + 5 * boxSize, 0.0f, halfChessBoardSize - 0.5f * boxSize));
    whiteKnight.push_back(glm::vec3(-halfChessBoardSize + boxSize / 2 + 6 * boxSize, 0.0f, halfChessBoardSize - 0.5f * boxSize));
    whiteRook.push_back(glm::vec3(-halfChessBoardSize + boxSize / 2 + 7 * boxSize, 0.0f, halfChessBoardSize - 0.5f * boxSize));
    

    // Black pieces positions
    for (int i = 0; i < 8; ++i) {
        float x = -halfChessBoardSize + boxSize / 2 + i * boxSize;
        float z = -halfChessBoardSize + 1.5f * boxSize;
        blackPawn.push_back(glm::vec3(x, 0.0f, z));
    }
    blackRook.push_back(glm::vec3(-halfChessBoardSize + boxSize / 2 + 0 * boxSize, 0.0f, -halfChessBoardSize + 0.5f * boxSize));
    blackKnight.push_back(glm::vec3(-halfChessBoardSize + boxSize / 2 + 1 * boxSize, 0.0f, -halfChessBoardSize + 0.5f * boxSize));
    blackBishop.push_back(glm::vec3(-halfChessBoardSize + boxSize / 2 + 2 * boxSize, 0.0f, -halfChessBoardSize + 0.5f * boxSize));
    blackQueen.push_back(glm::vec3(-halfChessBoardSize + boxSize / 2 + 3 * boxSize, 0.0f, -halfChessBoardSize + 0.5f * boxSize));
    blackKing.push_back(glm::vec3(-halfChessBoardSize + boxSize / 2 + 4 * boxSize, 0.0f, -halfChessBoardSize + 0.5f * boxSize));
    blackBishop.push_back(glm::vec3(-halfChessBoardSize + boxSize / 2 + 5 * boxSize, 0.0f, -halfChessBoardSize + 0.5f * boxSize));
    blackKnight.push_back(glm::vec3(-halfChessBoardSize + boxSize / 2 + 6 * boxSize, 0.0f, -halfChessBoardSize + 0.5f * boxSize));
    blackRook.push_back(glm::vec3(-halfChessBoardSize + boxSize / 2 + 7 * boxSize, 0.0f, -halfChessBoardSize + 0.5f * boxSize));

    // Main rendering loop
    do 
    {
        // Get render time
        double currentTime = glfwGetTime();
        nFrames++;
        if (currentTime - lastTime >= 1.0) {
            printf("%f ms/frame\n", 1000.0 / double(nFrames));
            nFrames = 0;
            lastTime += 1.0;
        }

        // Clear the screen
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Handle camera inputs
        keyBinds();

        // Use our shader
        glUseProgram(programID);
        
        // Compute the camera position in Cartesian coordinates
        glm::vec3 positionCamera;
        positionCamera.x = cameraRadius * cos(verticalAngle) * sin(horizontalAngle);
        positionCamera.y = cameraRadius * sin(verticalAngle);
        positionCamera.z = cameraRadius * cos(verticalAngle) * cos(horizontalAngle);
        
        // Camera looks at the origin
        glm::vec3 targetCamera = glm::vec3(0.0f, 0.0f, 0.0f);

        // Up vector
        glm::vec3 upVector = glm::vec3(0.0f, 1.0f, 0.0f);

        // Compute the ViewMatrix
        glm::mat4 ViewMatrix = glm::lookAt(
            positionCamera,
            targetCamera,
            upVector
        );

        // Compute the ProjectionMatrix
        glm::mat4 ProjectionMatrix = glm::perspective(
            glm::radians(60.0f),
            1024.0f / 768.0f,
            0.1f,
            100.0f
        );
        
// Set common uniforms
        glUniformMatrix4fv(ViewMatrixID, 1, GL_FALSE, &ViewMatrix[0][0]);
        glUniform1i(SpecularDiffuseEnabledID, specularDiffuseEnabled ? 1 : 0);

// Set the light position
        glUniform3f(LightID, 5, 25, 5);

// Chessboard rendering
        {
            glm::mat4 chessboardModelMatrix = glm::mat4(1.0f);

            // Apply scaling to the chessboard
            float chessboardScale = 1.0f; // Adjust as needed
            chessboardModelMatrix = glm::scale(chessboardModelMatrix, glm::vec3(chessboardScale));

            // Rotate the chessboard if necessary
            chessboardModelMatrix = glm::rotate(chessboardModelMatrix, glm::radians(-90.0f),
                glm::vec3(1.0, 0.0, 0.0));

            glm::mat4 chessboardMVP = ProjectionMatrix * ViewMatrix * chessboardModelMatrix;

            // Send uniforms to the shader
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &chessboardModelMatrix[0][0]);
            glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &chessboardMVP[0][0]);

            // Render the chessboard
            chessboardModel.RenderModel();
        }

//Render all the pieces by calling reference function
        renderPieces(whitePawn, ModelMatrixID, MatrixID, ColorID, pawnModel, true);
        renderPieces(whiteKing, ModelMatrixID, MatrixID, ColorID, kingModel, true);
        renderPieces(whiteQueen, ModelMatrixID, MatrixID, ColorID, queenModel, true);
        renderPieces(whiteBishop, ModelMatrixID, MatrixID, ColorID, bishopModel, true);
        renderPieces(whiteKnight, ModelMatrixID, MatrixID, ColorID, knightModel, true);
        renderPieces(whiteRook, ModelMatrixID, MatrixID, ColorID, rookModel, true);
        
        renderPieces(blackPawn, ModelMatrixID, MatrixID, ColorID, pawnModel, false);
        renderPieces(blackKing, ModelMatrixID, MatrixID, ColorID, kingModel, false);
        renderPieces(blackQueen, ModelMatrixID, MatrixID, ColorID, queenModel, false);
        renderPieces(blackBishop, ModelMatrixID, MatrixID, ColorID, bishopModel, false);
        renderPieces(blackKnight, ModelMatrixID, MatrixID, ColorID, knightModel, false);
        renderPieces(blackRook, ModelMatrixID, MatrixID, ColorID, rookModel, false);

        // Swap buffers
        glfwSwapBuffers(window);
        glfwPollEvents();

    } while (!glfwWindowShouldClose(window));

    // Cleanup VBO and shader
    glDeleteProgram(programID);

    // Close OpenGL window and terminate GLFW
    glfwTerminate();

    return 0;
}