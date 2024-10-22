#include <iostream>
#include <cstdio>  // for popen and pclose
#include <memory>  // for smart pointers
#include <string>  // for strings

std::string execute_command(const char* command) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command, "r"), pclose);
    
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    
    return result;
}

int main() {
    try {
        // Here you run a WSL command like 'docker ps' using wsl.exe
        std::string command_output = execute_command("wsl.exe docker ps");

        // Print the output
        std::cout << "Command output: " << std::endl;
        std::cout << command_output << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}
