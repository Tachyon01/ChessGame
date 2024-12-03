/*
Author: Pulkit Goel
Class: ECE6122 (section)
Last Date Modified: 12/02/2024
Description:
Class for communicating with Komodo engine
*/

class ECE_ChessEngine {
public:
    ECE_ChessEngine(const std::string& path)
        : enginePath(path), engineReady(false) {}

    ~ECE_ChessEngine() {
        cleanupEngine();
    }

    bool initializeEngine() {
        std::cout << "Initializing engine..." << std::endl;

        if (!setupPipes() || !createChildProcess()) {
            return false;
        }
        std::cout << "Child process created successfully." << std::endl;

        engineReady = true;
        engineThread = std::thread(&ECE_ChessEngine::readEngineOutput, this);

        return initializeUCI();
    }

    bool sendMove(const std::string& moveHistory) {
        if (!engineReady) return false;
        std::string command = "position startpos moves " + moveHistory + "\n";
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

    void cleanupEngine() {
        engineReady = false;
        if (engineThread.joinable()) {
            engineThread.join();
        }
        CloseHandle(childStd_IN_Wr);
        CloseHandle(childStd_OUT_Rd);
        CloseHandle(piProcInfo.hProcess);
        CloseHandle(piProcInfo.hThread);
    }

    bool setupPipes() {
        SECURITY_ATTRIBUTES saAttr = {};
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        if (!CreatePipe(&childStd_OUT_Rd, &childStd_OUT_Wr, &saAttr, 0) ||
            !SetHandleInformation(childStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0) ||
            !CreatePipe(&childStd_IN_Rd, &childStd_IN_Wr, &saAttr, 0) ||
            !SetHandleInformation(childStd_IN_Wr, HANDLE_FLAG_INHERIT, 0)) {
            std::cerr << "Failed to set up pipes." << std::endl;
            return false;
        }

        return true;
    }

    bool createChildProcess() {
        ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

        STARTUPINFOA siStartInfo = {};
        ZeroMemory(&siStartInfo, sizeof(STARTUPINFOA));
        siStartInfo.cb = sizeof(STARTUPINFOA);
        siStartInfo.hStdError = childStd_OUT_Wr;
        siStartInfo.hStdOutput = childStd_OUT_Wr;
        siStartInfo.hStdInput = childStd_IN_Rd;
        siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

        std::string cmdLine = enginePath;
        char* cmdLineMutable = &cmdLine[0];

        BOOL success = CreateProcessA(
            NULL, cmdLineMutable, NULL, NULL, TRUE,
            CREATE_NO_WINDOW, NULL, NULL, &siStartInfo, &piProcInfo);

        if (!success) {
            reportError();
            return false;
        }

        CloseHandle(childStd_OUT_Wr);
        CloseHandle(childStd_IN_Rd);
        return true;
    }

    void reportError() {
        DWORD errorCode = GetLastError();
        LPVOID errorMsg;
        FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&errorMsg, 0, NULL);
        std::cerr << "CreateProcess failed with error (" << errorCode
            << "): " << (char*)errorMsg << std::endl;
        LocalFree(errorMsg);
    }

    bool initializeUCI() {
        sendCommand("uci\n");
        std::cout << "Sent 'uci' command to the engine." << std::endl;
        if (!waitForResponse(uciOkReceived, "uciok")) {
            return false;
        }

        sendCommand("isready\n");
        std::cout << "Sent 'isready' command to the engine." << std::endl;
        return waitForResponse(readyOkReceived, "readyok");
    }

    bool waitForResponse(bool& flag, const std::string& expectedResponse) {
        std::unique_lock<std::mutex> lock(engineMutex);
        if (!engineCondition.wait_for(lock, std::chrono::seconds(5),
            [&flag]() { return flag; })) {
            std::cerr << "Timeout waiting for '" << expectedResponse << "' from engine." << std::endl;
            return false;
        }
        std::cout << "Received '" << expectedResponse << "' from the engine." << std::endl;
        return true;
    }

    void readEngineOutput() {
        DWORD bytesRead;
        CHAR buffer[4096];
        std::string leftover;

        while (engineReady) {
            BOOL success = ReadFile(childStd_OUT_Rd, buffer, sizeof(buffer) - 1, &bytesRead, NULL);
            if (!success || bytesRead == 0) {
                break;
            }

            buffer[bytesRead] = '\0';
            std::string output(buffer);
            output = leftover + output;

            processOutput(output, leftover);
        }
    }

    void processOutput(std::string& output, std::string& leftover) {
        size_t pos = 0, newlinePos;

        while ((newlinePos = output.find_first_of("\r\n", pos)) != std::string::npos) {
            std::string line = output.substr(pos, newlinePos - pos);
            processEngineOutputLine(line);
            pos = output.find_first_not_of("\r\n", newlinePos);
            if (pos == std::string::npos) {
                pos = output.size();
            }
        }
        leftover = output.substr(pos);
    }

    void processEngineOutputLine(const std::string& line) {
        if (line.empty()) return;

        //        std::cout << "Engine Output: " << line << std::endl;
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
            std::string bestMove = line.find("bestmove (none)") != std::string::npos
                ? "none"
                : line.substr(9, 4);
            engineResponses.push_back(bestMove);
            engineCondition.notify_all();
        }
    }

    bool writeToEngine(const std::string& command) {
        DWORD bytesWritten;
        BOOL success = WriteFile(childStd_IN_Wr, command.c_str(), command.size(), &bytesWritten, NULL);
        return success && bytesWritten == command.size();
    }
};