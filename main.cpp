/*
Author: Pulkit Goel
Class: ECE6122 (section)
Last Date Modified: 11/28/2024
Description:
main function for game of chess
*/


// Include standard headers
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <iostream>
#include <sstream>
#include <string>
#include <GL/glew.h>
#include <glfw3.h>
GLFWwindow* window;
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;
#include "assimp_model.h"
#include "Windows.h"
#include <regex>

//#include <unistd.h>
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

using namespace std;

class ECE_ChessEngine {
public:
    ECE_ChessEngine(const std::string& path) : enginePath(path), engineReady(false) {}
    ~ECE_ChessEngine() {
        engineReady = false;
        if (engineThread.joinable()) {
            engineThread.join();
        }
        CloseHandle(childStd_IN_Wr);
        CloseHandle(childStd_OUT_Rd);
        CloseHandle(piProcInfo.hProcess);
        CloseHandle(piProcInfo.hThread);
    }

    bool initializeEngine() {
        std::cout << "Initializing engine..." << std::endl;

        SECURITY_ATTRIBUTES saAttr;
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE; // Allow inheritance
        saAttr.lpSecurityDescriptor = NULL;

        // Create a pipe for the child process's STDOUT.
        if (!CreatePipe(&childStd_OUT_Rd, &childStd_OUT_Wr, &saAttr, 0)) {
            std::cerr << "StdoutRd CreatePipe failed\n";
            return false;
        }

        // Ensure the read handle to the pipe for STDOUT is not inherited.
        if (!SetHandleInformation(childStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
            std::cerr << "Stdout SetHandleInformation failed\n";
            return false;
        }

        // Create a pipe for the child process's STDIN.
        if (!CreatePipe(&childStd_IN_Rd, &childStd_IN_Wr, &saAttr, 0)) {
            std::cerr << "Stdin CreatePipe failed\n";
            return false;
        }

        // Ensure the write handle to the pipe for STDIN is not inherited.
        if (!SetHandleInformation(childStd_IN_Wr, HANDLE_FLAG_INHERIT, 0)) {
            std::cerr << "Stdin SetHandleInformation failed\n";
            return false;
        }

        // Create the child process.
        if (!createChildProcess()) {
            std::cerr << "Failed to create child process\n";
            return false;
        }
        std::cout << "Child process created successfully." << std::endl;

        engineReady = true;
        engineThread = std::thread(&ECE_ChessEngine::readEngineOutput, this);

        sendCommand("uci\n");  // Initialize UCI mode
        std::cout << "Sent 'uci' command to the engine." << std::endl;

        // Wait for "uciok"
        {
            std::unique_lock<std::mutex> lock(engineMutex);
            if (!engineCondition.wait_for(lock, std::chrono::seconds(5),
                [this]() { return uciOkReceived; })) {
                std::cerr << "Timeout waiting for 'uciok' from engine." << std::endl;
                return false;
            }
        }
        std::cout << "Received 'uciok' from the engine." << std::endl;

        sendCommand("isready\n");  // Wait for the engine to be ready
        std::cout << "Sent 'isready' command to the engine." << std::endl;

        // Wait for "readyok"
        {
            std::unique_lock<std::mutex> lock(engineMutex);
            if (!engineCondition.wait_for(lock, std::chrono::seconds(5),
                [this]() { return readyOkReceived; })) {
                std::cerr << "Timeout waiting for 'readyok' from engine." << std::endl;
                return false;
            }
        }
        std::cout << "Received 'readyok' from the engine." << std::endl;

        return true;
    }

    bool sendMove(const std::string& moveHistory) {
        if (!engineReady) return false;
        std::string command = "position startpos moves" + moveHistory + "\n";
        return sendCommand(command) && sendCommand("go\n");
    }

    bool getResponseMove(std::string& responseMove) {
        std::unique_lock<std::mutex> lock(engineMutex);
        if (engineCondition.wait_for(lock, std::chrono::seconds(10),
            [this]() { return !engineResponses.empty(); })) {
            responseMove = engineResponses.front();
            engineResponses.erase(engineResponses.begin());
            return true;
        }
        return false;
    }

    // Function to send a generic command to the engine
    bool sendCommand(const std::string& command) {
        if (!engineReady) return false;
        return writeToEngine(command);
    }

private:
    std::string enginePath;
    PROCESS_INFORMATION piProcInfo;
    HANDLE childStd_IN_Rd = NULL;
    HANDLE childStd_IN_Wr = NULL;
    HANDLE childStd_OUT_Rd = NULL;
    HANDLE childStd_OUT_Wr = NULL;
    std::atomic<bool> engineReady;
    std::thread engineThread;
    std::mutex engineMutex;
    std::condition_variable engineCondition;
    std::vector<std::string> engineResponses;
    bool uciOkReceived = false;
    bool readyOkReceived = false;

    bool createChildProcess() {
        // Set up members of the PROCESS_INFORMATION structure.
        ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

        // Set up members of the STARTUPINFO structure.
        STARTUPINFOA siStartInfo;
        ZeroMemory(&siStartInfo, sizeof(STARTUPINFOA));
        siStartInfo.cb = sizeof(STARTUPINFOA);
        siStartInfo.hStdError = childStd_OUT_Wr;
        siStartInfo.hStdOutput = childStd_OUT_Wr;
        siStartInfo.hStdInput = childStd_IN_Rd;
        siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

        // Prepare mutable command line
        std::string cmdLine = enginePath;
        char* cmdLineMutable = &cmdLine[0]; // Get mutable C-string

        // Create the child process.
        BOOL success = CreateProcessA(
            NULL,               // Application name
            cmdLineMutable,     // Command line (mutable)
            NULL,               // Process security attributes
            NULL,               // Primary thread security attributes
            TRUE,               // Handles are inherited
            CREATE_NO_WINDOW,   // Creation flags (no console window)
            NULL,               // Use parent's environment
            NULL,               // Use parent's current directory
            &siStartInfo,       // STARTUPINFO pointer
            &piProcInfo         // Receives PROCESS_INFORMATION
        );

        if (!success) {
            DWORD errorCode = GetLastError();
            LPVOID errorMsg;
            FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                errorCode,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPSTR)&errorMsg,
                0,
                NULL
            );
            std::cerr << "CreateProcess failed with error (" << errorCode
                << "): " << (char*)errorMsg << std::endl;
            LocalFree(errorMsg);
            return false;
        }
        // Close handles to the child's STDIN and STDOUT pipes no longer needed by the parent.
        CloseHandle(childStd_OUT_Wr);
        CloseHandle(childStd_IN_Rd);
        return true;
    }

    void readEngineOutput() {
        DWORD bytesRead;
        CHAR buffer[4096];
        std::string leftover;
        while (engineReady) {
            BOOL success = ReadFile(childStd_OUT_Rd, buffer, sizeof(buffer) - 1,
                &bytesRead, NULL);
            if (!success || bytesRead == 0) {
                break;
            }
            buffer[bytesRead] = '\0';
            std::string output(buffer);

            output = leftover + output;

            // Process the output line by line
            size_t pos = 0;
            size_t newlinePos;
            while ((newlinePos = output.find_first_of("\r\n", pos)) != std::string::npos) {
                std::string line = output.substr(pos, newlinePos - pos);
                processEngineOutputLine(line);
                // Skip over consecutive \r\n characters
                pos = output.find_first_not_of("\r\n", newlinePos);
                if (pos == std::string::npos) {
                    pos = output.size();
                }
            }

            // Save any leftover output for next time
            leftover = output.substr(pos);
        }
    }

    void processEngineOutputLine(const std::string& line) {
        if (line.empty()) return; // Ignore empty lines
        std::cout << "Engine Output: " << line << std::endl; // Debug output
        std::lock_guard<std::mutex> lock(engineMutex);
        if (line == "uciok") {
            uciOkReceived = true;
            engineCondition.notify_all();
        }
        else if (line == "readyok") {
            readyOkReceived = true;
            engineCondition.notify_all();
        }
        else if (line.find("bestmove") == 0) {
            if (line.find("bestmove (none)") != std::string::npos) {
                // The engine has no move; checkmate or stalemate
                engineResponses.push_back("none");
                engineCondition.notify_all();
            }
            else {
                std::string bestMove = line.substr(9, 4);
                engineResponses.push_back(bestMove);
                engineCondition.notify_all();
            }
        }
    }

    bool writeToEngine(const std::string& command) {
        DWORD bytesWritten;
        BOOL success = WriteFile(childStd_IN_Wr, command.c_str(), command.size(),
            &bytesWritten, NULL);
        return success && bytesWritten == command.size();
    }
};

//Some global functions and variables
//Get constant values for setting up positions
float boxSize = 18.0f; // Size of each square
float ChessBoardSize = boxSize * 8;
float halfChessBoardSize = ChessBoardSize / 2.0f;
int start = 0;
bool specularDiffuseEnabled = true;
int killID = 0;
bool myMove = false;

string move_history = "";

//z for now
//will use only 95 to 104
float files[105];
float killFile[32];

//x axis
//took 0 as spare
float ranks[9];
float killRank[32];

// Camera parameters
struct camera
{
    float radius = 50.0f;
    float horizontalAngle = glm::radians(0.0f);
    float verticalAngle = glm::radians(20.0f);

    float x = 17.1;
    float y = 47.0;
    float z = 0.0;


    void get_Cartesian()
    {
        x = radius * sin(verticalAngle) * cos(horizontalAngle);
        z = radius * cos(verticalAngle);
        y = radius * sin(verticalAngle) * sin(horizontalAngle);
    }

    //bool specularDiffuseEnabled = true;
} camera;

// Light parameters
struct light
{
    float radius = 25.0f;
    float horizontalAngle = glm::radians(25.0f);
    float verticalAngle = glm::radians(25.0f);

    //Values here are cartesian
    float x = 25.0;
    float y = 25.0;
    float z = 25.0;

    void get_Cartesian()
    {
        x = radius * sin(verticalAngle) * cos(horizontalAngle);
        z = radius * cos(verticalAngle);
        y = radius * sin(verticalAngle) * sin(horizontalAngle);
    }

    float power = 3000.0f;
    //bool specularDiffuseEnabled = true;
} light;

//Components for 1 piece
struct piece
{
    glm::vec3 name;
    float xPos = 0.0f;
    float yPos = 0.0f;
    float zPos = 0.0f;

    char file = 'a';
    int rank = 1;

    glm::vec3 pos = glm::vec3(xPos, yPos, zPos);

    bool isWhite = true;
};

struct models
{
    CAssimpModel pawnModel_dark;
    CAssimpModel rookModel_dark;
    CAssimpModel knightModel_dark;
    CAssimpModel bishopModel_dark;
    CAssimpModel queenModel_dark;
    CAssimpModel kingModel_dark;
    CAssimpModel pawnModel_light;
    CAssimpModel rookModel_light;
    CAssimpModel knightModel_light;
    CAssimpModel bishopModel_light;
    CAssimpModel queenModel_light;
    CAssimpModel kingModel_light;
} pieceModels;

//Struct for all chess pieces
struct allPiece
{
    //Black pieces
    piece blackPawn1;
    piece blackPawn2;
    piece blackPawn3;
    piece blackPawn4;
    piece blackPawn5;
    piece blackPawn6;
    piece blackPawn7;
    piece blackPawn8;

    piece blackRook1; //Elephant
    piece blackKnight1; //Horse
    piece blackBishop1; //Camel
    piece blackQueen;
    piece blackKing;
    piece blackBishop2;
    piece blackKnight2; 
    piece blackRook2;

    //white pieces
    piece whitePawn1;
    piece whitePawn2;
    piece whitePawn3;
    piece whitePawn4;
    piece whitePawn5;
    piece whitePawn6;
    piece whitePawn7;
    piece whitePawn8;

    piece whiteRook1; //Elephant
    piece whiteKnight1; //Horse
    piece whiteBishop1; //Camel
    piece whiteQueen;
    piece whiteKing;
    piece whiteBishop2;
    piece whiteKnight2;
    piece whiteRook2;

    void updatePos(piece &p, char file, int rank)
    {
        p.file = file;
        p.rank = rank;

        p.zPos = 0.0f;
        p.xPos = ranks[rank];
        p.yPos = files[file];

        p.pos = glm::vec3(p.xPos, p.yPos, p.zPos);

        //std::cout << "Piece at r " << ranks[rank] << " file " << file <<" "<< files[file] << "\n";
    }

}allPieces;

void initialiseBoard(GLuint programID)
{
    //set values for ranks and files
    for (int i = 0; i < 9; i++)
    {
        ranks[i] = -1*( -halfChessBoardSize + boxSize / 2 + (i - 1) * boxSize);
        files[i + 96] = -1*( -halfChessBoardSize + boxSize / 2 + (i - 1) * boxSize);

        std::cout << "Rank " << i << " " << ranks[i] << " files  " << files[i + 96] << "\n";
    }
    for (int i = 0; i < 32; i++)
    {
        killRank[i] = 1000;
        killFile[i] = 1000;
    }
//HERE
    //for (int i = 0; i < 8; i++)
    //{
    //    for (int j=0; j<4; j++)
    //    {
    //        killFile[4 * i + j] = ;
    //        //killFile[4 * i + j] = (-1 * (-halfChessBoardSize + boxSize / 2 + (j - 1) * boxSize));
    //        switch (j) 
    //        {
    //            case 0:
    //            {
    //                killRank[4 * i + j] = 90;
    //                break;
    //            }
    //            case 1:
    //            {
    //                killRank[4 * i + j] = -75;
    //                break;
    //            }
    //            case 2:
    //            {
    //                killRank[4 * i + j] = 100;
    //                break;
    //            }
    //            case 3:
    //            {
    //                killRank[4 * i + j] = -85;
    //                break;
    //            }
    //            default:
    //                break;
    //        }
    //        std::cout << "Kill rank " << 4 * i + j << " " << killRank[4 * i + j] << " file  " << killFile[4 * i + j] << "\n";
    //    }
    //}


    //set pieces
    allPieces.updatePos(allPieces.blackPawn1, 'a', 7);
    allPieces.updatePos(allPieces.blackPawn2, 'b', 7);
    allPieces.updatePos(allPieces.blackPawn3, 'c', 7);
    allPieces.updatePos(allPieces.blackPawn4, 'd', 7);
    allPieces.updatePos(allPieces.blackPawn5, 'e', 7);
    allPieces.updatePos(allPieces.blackPawn6, 'f', 7);
    allPieces.updatePos(allPieces.blackPawn7, 'g', 7);
    allPieces.updatePos(allPieces.blackPawn8, 'h', 7);

    allPieces.updatePos(allPieces.blackRook1,   'a', 8);
    allPieces.updatePos(allPieces.blackKnight1, 'b', 8);
    allPieces.updatePos(allPieces.blackBishop1, 'c', 8);
    allPieces.updatePos(allPieces.blackQueen,   'd', 8);
    allPieces.updatePos(allPieces.blackKing,    'e', 8);
    allPieces.updatePos(allPieces.blackBishop2, 'f', 8);
    allPieces.updatePos(allPieces.blackKnight2, 'g', 8);
    allPieces.updatePos(allPieces.blackRook2,   'h', 8);

    allPieces.blackPawn1.isWhite = false;
    allPieces.blackPawn2.isWhite = false;
    allPieces.blackPawn3.isWhite = false;
    allPieces.blackPawn4.isWhite = false;
    allPieces.blackPawn5.isWhite = false;
    allPieces.blackPawn6.isWhite = false;
    allPieces.blackPawn7.isWhite = false;
    allPieces.blackPawn8.isWhite = false;
    allPieces.blackRook1.isWhite = false;
    allPieces.blackKnight1.isWhite = false;
    allPieces.blackBishop1.isWhite = false;
    allPieces.blackKing.isWhite = false;
    allPieces.blackQueen.isWhite = false;
    allPieces.blackBishop2.isWhite = false;
    allPieces.blackKnight2.isWhite = false;
    allPieces.blackRook2.isWhite = false;


    //Whites
    allPieces.updatePos(allPieces.whitePawn1, 'a', 2);
    allPieces.updatePos(allPieces.whitePawn2, 'b', 2);
    allPieces.updatePos(allPieces.whitePawn3, 'c', 2);
    allPieces.updatePos(allPieces.whitePawn4, 'd', 2);
    allPieces.updatePos(allPieces.whitePawn5, 'e', 2);
    allPieces.updatePos(allPieces.whitePawn6, 'f', 2);
    allPieces.updatePos(allPieces.whitePawn7, 'g', 2);
    allPieces.updatePos(allPieces.whitePawn8, 'h', 2);

    allPieces.updatePos(allPieces.whiteRook1,   'a', 1);
    allPieces.updatePos(allPieces.whiteKnight1, 'b', 1);
    allPieces.updatePos(allPieces.whiteBishop1, 'c', 1);
    allPieces.updatePos(allPieces.whiteQueen,   'd', 1);
    allPieces.updatePos(allPieces.whiteKing,    'e', 1);
    allPieces.updatePos(allPieces.whiteBishop2, 'f', 1);
    allPieces.updatePos(allPieces.whiteKnight2, 'g', 1);
    allPieces.updatePos(allPieces.whiteRook2,   'h', 1);
}


//Render the pieces
void renderPieces(glm::vec3 piecePosition, GLuint ModelMatrixID, GLuint MatrixID, GLuint ColorID, CAssimpModel pieceModel, bool isWhite)
{
    // Compute the camera position in Cartesian coordinates
    glm::vec3 positionCamera = glm::vec3(camera.x, camera.y, camera.z);
    // Camera looks at the origin
    glm::vec3 targetCamera = glm::vec3(0.0f, 0.0f, 0.0f);
    // Up vector
    glm::vec3 upVector = glm::vec3(0.0f, 0.0f, 1.0f);
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

   /* for (const auto& position : piecePosition)
    {*/
        glm::mat4 pieceModelMatrix = glm::mat4(1.0f);

        // Apply scaling
        float pieceScale = 0.3f;
        pieceModelMatrix = glm::scale(pieceModelMatrix, glm::vec3(pieceScale));

        
        // Translate the piece to its position
        pieceModelMatrix = glm::translate(pieceModelMatrix, piecePosition);
        //pieceModelMatrix = glm::translate(pieceModelMatrix, position);

        // Rotate the piece if necessary
        pieceModelMatrix = glm::rotate(pieceModelMatrix, glm::radians(-90.0f),
            glm::vec3(0.0f, 0.0f, 1.0f));
        pieceModelMatrix = glm::rotate(pieceModelMatrix, glm::radians(90.0f),
            glm::vec3(1.0f, 0.0f, 0.0f));

        // Compute MVP matrix
        glm::mat4 pieceMVP = ProjectionMatrix * ViewMatrix * pieceModelMatrix;

        // Send uniforms to the shader
        glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &pieceModelMatrix[0][0]);
        glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &pieceMVP[0][0]);

        if (isWhite) {
            glUniform4f(ColorID, 4.5f, 4.5f, 4.5f, 4.5f);
        }
        else {
            glUniform4f(ColorID, 1.5f, 1.5f, 1.5f, 1.5f);
        }

        // Render the piece
        pieceModel.RenderModel();
    //}
}

void renderAll(GLuint ModelMatrixID, GLuint MatrixID, GLuint ColorID)
{
    //Black
    renderPieces(allPieces.blackPawn1.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.pawnModel_dark, false);
    renderPieces(allPieces.blackPawn2.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.pawnModel_dark, false);
    renderPieces(allPieces.blackPawn3.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.pawnModel_dark, false);
    renderPieces(allPieces.blackPawn4.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.pawnModel_dark, false);
    renderPieces(allPieces.blackPawn5.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.pawnModel_dark, false);
    renderPieces(allPieces.blackPawn6.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.pawnModel_dark, false);
    renderPieces(allPieces.blackPawn7.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.pawnModel_dark, false);
    renderPieces(allPieces.blackPawn8.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.pawnModel_dark, false);
    renderPieces(allPieces.blackRook1.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.rookModel_dark, false);
    renderPieces(allPieces.blackKnight1.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.knightModel_dark, false);
    renderPieces(allPieces.blackBishop1.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.bishopModel_dark, false);
    renderPieces(allPieces.blackQueen.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.queenModel_dark, false);
    renderPieces(allPieces.blackKing.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.kingModel_dark, false);
    renderPieces(allPieces.blackBishop2.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.bishopModel_dark, false);
    renderPieces(allPieces.blackKnight2.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.knightModel_dark, false);
    renderPieces(allPieces.blackRook2.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.rookModel_dark, false);

    //White
    renderPieces(allPieces.whitePawn1.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.pawnModel_light, false);
    renderPieces(allPieces.whitePawn2.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.pawnModel_light, false);
    renderPieces(allPieces.whitePawn3.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.pawnModel_light, false);
    renderPieces(allPieces.whitePawn4.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.pawnModel_light, false);
    renderPieces(allPieces.whitePawn5.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.pawnModel_light, false);
    renderPieces(allPieces.whitePawn6.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.pawnModel_light, false);
    renderPieces(allPieces.whitePawn7.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.pawnModel_light, false);
    renderPieces(allPieces.whitePawn8.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.pawnModel_light, false);
    renderPieces(allPieces.whiteRook1.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.rookModel_light, false);
    renderPieces(allPieces.whiteKnight1.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.knightModel_light, false);
    renderPieces(allPieces.whiteBishop1.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.bishopModel_light, false);
    renderPieces(allPieces.whiteQueen.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.queenModel_light, false);
    renderPieces(allPieces.whiteKing.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.kingModel_light, false);
    renderPieces(allPieces.whiteBishop2.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.bishopModel_light, false);
    renderPieces(allPieces.whiteKnight2.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.knightModel_light, false);
    renderPieces(allPieces.whiteRook2.pos, ModelMatrixID, MatrixID, ColorID, pieceModels.rookModel_light, false);
}

// Function to handle camera inputs
void keyBinds() {
    // Camera speed
    float cameraSpeed = 0.05f;
    float rotationSpeed = 0.002f;

    // 'W' Move close
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        camera.radius -= cameraSpeed;
    }

    // 'S' Move farther
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        camera.radius += cameraSpeed;
    }

    // 'A' Rotate left
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        camera.horizontalAngle += rotationSpeed;
    }

    // 'D' Rotate right
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        camera.horizontalAngle -= rotationSpeed;
    }

    // Up Rotate up
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
    {
        camera.verticalAngle += rotationSpeed;
        if (camera.verticalAngle > glm::radians(89.0f)) camera.verticalAngle = glm::radians(89.0f);
    }

    // Down Rotate down
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
    {
        camera.verticalAngle -= rotationSpeed;
        if (camera.verticalAngle < glm::radians(-89.0f)) camera.verticalAngle = glm::radians(-89.0f);
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

piece* findPiece(allPiece& pieces, char file, int rank)
{
    piece* pieceArray[] = {
        &pieces.blackPawn1, &pieces.blackPawn2, &pieces.blackPawn3, &pieces.blackPawn4, &pieces.blackPawn5, &pieces.blackPawn6, &pieces.blackPawn7, &pieces.blackPawn8,
        &pieces.blackRook1, &pieces.blackKnight1, &pieces.blackBishop1, &pieces.blackQueen, &pieces.blackKing, &pieces.blackBishop2, &pieces.blackKnight2, &pieces.blackRook2,
        &pieces.whitePawn1, &pieces.whitePawn2, &pieces.whitePawn3, &pieces.whitePawn4, &pieces.whitePawn5, &pieces.whitePawn6, &pieces.whitePawn7, &pieces.whitePawn8,
        &pieces.whiteRook1, &pieces.whiteKnight1, &pieces.whiteBishop1, &pieces.whiteQueen, &pieces.whiteKing, &pieces.whiteBishop2, &pieces.whiteKnight2, &pieces.whiteRook2
    };

    for (auto& p : pieceArray) {
        if (p->file == file && p->rank == rank) {
            return p;
        }
    }
    return nullptr;
}

//Do Komodo's move as well 
bool toMove(string initial, string final)
{
    //Find piece
    int found = 0;
    piece* foundPiece_toMove = findPiece(allPieces, initial[0], int(initial[1] - '0'));
    piece* foundPiece_toKill = findPiece(allPieces, final[0], int(final[1] - '0'));
    //std::cout << "Check at palce "<<initial[0]<<" "<<int(initial[1] - '0')<<std::endl;
    if (foundPiece_toMove == nullptr)
    {
        std::cout << "No piece at given place\n";
        return false;
    }
    else
    {
        if (foundPiece_toKill)
        {
            if (foundPiece_toKill->isWhite == foundPiece_toMove->isWhite)
            {
                std::cout << "Cant destroy piece of same color\n";
                return false;
            }
            
            allPieces.updatePos(*foundPiece_toKill, killRank[killID], killFile[killID]);
            std::cout << "Piece destroyed\n";
            killID++;
        }
        allPieces.updatePos(*foundPiece_toMove, final[0], int(final[1] - '0'));
        std::cout << "Move done\n";

        //Add move to history
        move_history += " " + initial + final;

        return true;
    }

    //Check if valid move for this

    //Move

    //check if destroy or checkmate
    


}

void useKomodo(ECE_ChessEngine& chessEngine)
{
    if (chessEngine.sendMove(move_history))
    {
        std::string responseMove;
        if (chessEngine.getResponseMove(responseMove))
        {
            if (responseMove == "none")
            {
                std::cout << "Checkmate you win!!" << std::endl;
                return;
            }
            std::cout << "Engine move: " << responseMove << std::endl;

            // Add the engine's move to moveHistory
            toMove(responseMove.substr(0, 2), responseMove.substr(2, 2));
            //move_history += " " + responseMove;
        }
    }
}

//EDIT HERE
void parse(string in, GLuint programID, ECE_ChessEngine& chessEngine)
{
    if (in == "quit")
    {
        std::cout << "Thanks for playing!!";
        glfwSetWindowShouldClose(window, true);
    }
    if (in == "Play")
    {
        if (start == 1) 
        {
            std::cout << "Game already in progress\n";
            return;
        }
        else
        {
            start = 1;
            light.horizontalAngle = glm::radians(25.0);
            light.verticalAngle = glm::radians(5.0);
            light.radius = 50.0;
            light.get_Cartesian();
            light.power = 3000.0;

            GLuint LightID = glGetUniformLocation(programID, "LightPosition_worldspace");
            glUniform3f(LightID, light.x, light.y, light.z);
        }
        
    }
    
    else if (in.substr(0, 6) == "camera")
    {
        float theta = 0.0f, phi = 0.0f, r = 0.0f;
        if (sscanf_s(in.c_str(), "camera %f %f %f", &theta, &phi, &r) != 3)
        {
            std::cerr << "Some issue with input" << in << std::endl;
            return;
        }
        // Enforce ranges
        if (r <= 0.0f)
        {
            std::cerr << "Radius must be positive" << std::endl;
            return;
        }
        if (theta < 20.0f || theta > 80.0f)
        {
            std::cerr << "Theta (Horizontal Angle) must be between 20 and 80 degrees" << std::endl;
            return;
        }
        if (phi < 0.0f || phi > 360.0f)
        {
            std::cerr << "Phi (Vertical Angle) must be between 0 and 360 degrees" << std::endl;
            return;
        }
        camera.horizontalAngle = glm::radians(theta);
        camera.verticalAngle = glm::radians(phi);
        camera.radius = r;
        camera.get_Cartesian();
    }

    else if (in.substr(0, 5) == "light")
    {
        float theta = 0.0f, phi = 0.0f, r = 0.0f;
        if (sscanf_s(in.c_str(), "light %f %f %f", &theta, &phi, &r) != 3)
        {
            std::cerr << "in issue " << in << std::endl;
            return;
        }
        // Enforce ranges
        if (r <= 0.0f)
        {
            std::cerr << "Radiusmust be positive" << std::endl;
            return;
        }
        if (theta < 20.0f || theta > 80.0f)
        {
            std::cerr << "Theta (Horizontal Angle) must be between 20 and 80 degrees" << std::endl;
            return;
        }
        if (phi < 0.0f || phi > 360.0f)
        {
            std::cerr << "Phi (Vertical Angle) must be between 0 and 360 degrees" << std::endl;
            return;
        }
        light.horizontalAngle = glm::radians(theta);
        light.verticalAngle = glm::radians(phi);
        light.radius = r;
        light.get_Cartesian();

        GLuint LightID = glGetUniformLocation(programID, "LightPosition_worldspace");
        glUniform3f(LightID, light.x, light.y, light.z);
    }
    
    else if (in.substr(0, 5) == "power")
    {
        float pow = 0.0f;
        if (sscanf_s(in.c_str(), "power %f", &pow) != 1)
        {
            std::cerr << "Invalid Comamnd -> Power " << in << std::endl;
            return;
        }
        light.power = pow;
    }

    else if (in.substr(0, 4) == "move")
    {
        std::regex moveRegex("^move [a-h][1-8][a-h][1-8]$");
        std::string move, initial, final;
        if (!std::regex_match(in, moveRegex))
        { // Check if the length is sufficient to avoid out-of-bounds 
            std::cerr << "Invalid Command -> move " << in << std::endl;
            return;
        }    
        // Directly extract the move string from the input 
        move = in.substr(5); // Extracts "e2e3" // Use substr to separate initial and final positions 
        initial = move.substr(0, 2);
        final = move.substr(2, 2);
        //std::cout << "Moving from " << initial << " to " << final << std::endl;
        if (initial == final)
        {
            std::cerr << "Initial and final position can't be same " << in << std::endl;
            return;
        }
        bool move_done = false;
        move_done = toMove(initial, final);
        if (move_done)
        {
            myMove = true;
            //Sleep(1);
            //cout << "Calling Komodo\n";
            //useKomodo(chessEngine);
        }
    }
    else
    {
        std::cout << "Invalid command or move!!\n";
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

int main(void) 
{
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
    if (!chessboardModel.LoadModelFromFile("../ChessGame/Stone_Chess_Board/12951_Stone_Chess_Board_v1_L3.obj"))
    {
        fprintf(stderr, "Failed to load chessboard OBJ file!!\n");
        getchar();
        glfwTerminate();
        return -1;
    }

    //Load pieces model using the function defined
    //CAssimpModel pawnModel;
    if (!loadModel(pieceModels.pawnModel_light, "../ChessGame/Chess/pawn_light.obj"))
    {
        return -1;
    }
    if (!loadModel(pieceModels.kingModel_light, "../ChessGame/Chess/king_light.obj"))
    {
        return -1;
    }
    if (!loadModel(pieceModels.queenModel_light, "../ChessGame/Chess/queen_light.obj"))
    {
        return -1;
    }
    //CAssimpModel rookModel;
    if (!loadModel(pieceModels.rookModel_light, "../ChessGame/Chess/rook_light.obj"))
    {
        return -1;
    }
    //CAssimpModel knightModel;
    if (!loadModel(pieceModels.knightModel_light, "../ChessGame/Chess/knight_light.obj"))
    {
        return -1;
    }
    if (!loadModel(pieceModels.bishopModel_light, "../ChessGame/Chess/bishop_light.obj"))
    {
        return -1;
    }
    if (!loadModel(pieceModels.pawnModel_dark, "../ChessGame/Chess/pawn_dark.obj"))
    {
        return -1;
    }
    if (!loadModel(pieceModels.kingModel_dark, "../ChessGame/Chess/king_dark.obj"))
    {
        return -1;
    }
    if (!loadModel(pieceModels.queenModel_dark, "../ChessGame/Chess/queen_dark.obj"))
    {
        return -1;
    }
    //CAssimpModel rookModel;
    if (!loadModel(pieceModels.rookModel_dark, "../ChessGame/Chess/rook_dark.obj"))
    {
        return -1;
    }
    //CAssimpModel knightModel;
    if (!loadModel(pieceModels.knightModel_dark, "../ChessGame/Chess/knight_dark.obj"))
    {
        return -1;
    }
    if (!loadModel(pieceModels.bishopModel_dark, "../ChessGame/Chess/bishop_dark.obj"))
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

    GLuint lightPower = glGetUniformLocation(programID, "LightPower");


    // For frame timing
    double lastTime = glfwGetTime();
    int nFrames = 0;



    //initialise the board
    initialiseBoard(programID);

    //initialise engine
    std::string enginePath = "komodo-14.1-64bit.exe"; // Replace with the correct path
    // to your engine
    ECE_ChessEngine chessEngine(enginePath);

    // Output the engine path for verification
    std::cout << "Engine Path: " << enginePath << std::endl;

    if (!chessEngine.initializeEngine())
    {
        std::cerr << "Failed to initialize the chess engine." << std::endl;
        glfwTerminate();
        return 0;
    }

    string in;

    // Set the light position
    glUniform3f(LightID, light.x, light.y, light.z);

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

        // Use our shader
        glUseProgram(programID);

        // Compute the camera position in Cartesian coordinates
        glm::vec3 positionCamera;
        camera.get_Cartesian();
        positionCamera.x = camera.x;
        positionCamera.y = camera.y;
        positionCamera.z = camera.z;

        // Camera looks at the origin
        glm::vec3 targetCamera = glm::vec3(0.0f, 0.0f, 0.0f);

        // Up vector
        glm::vec3 upVector = glm::vec3(0.0f, 0.0f, 1.0f);

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
        glUniform1f(lightPower, light.power);


        // Chessboard rendering
        {
            glm::mat4 chessboardModelMatrix = glm::mat4(1.0f);

            // Apply scaling to the chessboard
            float chessboardScale = 1.0f; // Adjust as needed
            chessboardModelMatrix = glm::scale(chessboardModelMatrix, glm::vec3(chessboardScale));

            // Rotate the chessboard if necessary
            //chessboardModelMatrix = glm::rotate(chessboardModelMatrix, glm::radians(-90.0f),
                //glm::vec3(1.0, 0.0, 0.0));
            chessboardModelMatrix = glm::rotate(chessboardModelMatrix, glm::radians(90.0f),
                glm::vec3(0.0, 0.0, 1.0));
            glm::mat4 chessboardMVP = ProjectionMatrix * ViewMatrix * chessboardModelMatrix;

            // Send uniforms to the shader
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &chessboardModelMatrix[0][0]);
            glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &chessboardMVP[0][0]);

            // Render the chessboard
            chessboardModel.RenderModel();
        }

        //Render all the pieces by calling reference function
        renderAll(ModelMatrixID, MatrixID, ColorID);


        // Swap buffers
        glfwSwapBuffers(window);
        glfwPollEvents();

        if (!myMove)
        {
            if (start == 0)
                cout << "Type Play to start: ";
            else
                cout << "Please enter a command: ";
            std::getline(std::cin, in);
        }
        else
        {
            Sleep(1000);
            cout << "Calling Komodo\n";
            useKomodo(chessEngine);
            myMove = false;
        }

        //Parse the string input
        parse(in, programID, chessEngine);

        // Handle camera inputs
        //keyBinds();

    } while (!glfwWindowShouldClose(window));

    // Cleanup VBO and shader
    glDeleteProgram(programID);

    // Close OpenGL window and terminate GLFW
    glfwTerminate();

    return 0;
}
