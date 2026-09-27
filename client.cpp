#define ASIO_STANDALONE
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <asio.hpp>
#include "protocol.hpp"

using asio::ip::tcp;
namespace fs = std::filesystem;

void send_file(tcp::socket& socket, const std::string& filename) {
    if (!fs::exists(filename)) {
        std::cerr << "Local file does not exist.\n";
        return;
    }

    Header header;
    std::strcpy(header.command, "PUT");
    std::strcpy(header.filename, filename.c_str());
    header.filesize = fs::file_size(filename);

    // Send PUT request
    asio::write(socket, asio::buffer(&header, sizeof(Header)));

    // Read server's overwrite check
    Header response;
    asio::read(socket, asio::buffer(&response, sizeof(Header)));

    if (std::string(response.command) == "EXISTS") {
        std::cout << "File exists on server. Overwrite? (YES/NO): ";
        std::string choice;
        std::cin >> choice;
        
        Header decision;
        if (choice == "YES") std::strcpy(decision.command, "YES");
        else {
            std::strcpy(decision.command, "NO");
            asio::write(socket, asio::buffer(&decision, sizeof(Header)));
            return;
        }
        asio::write(socket, asio::buffer(&decision, sizeof(Header)));
    }

    // Send file payload
    std::ifstream infile(filename, std::ios::binary);
    std::vector<char> file_buffer(8192);
    while (infile.read(file_buffer.data(), file_buffer.size()) || infile.gcount() > 0) {
        asio::write(socket, asio::buffer(file_buffer.data(), infile.gcount()));
    }
    std::cout << "Upload complete.\n";
}

void receive_file(tcp::socket& socket, const std::string& filename) {
    Header header;
    std::strcpy(header.command, "GET");
    std::strcpy(header.filename, filename.c_str());

    // Send GET request
    asio::write(socket, asio::buffer(&header, sizeof(Header)));

    // Read server metadata
    Header response;
    asio::read(socket, asio::buffer(&response, sizeof(Header)));

    if (std::string(response.command) == "ERR") {
        std::cerr << "File does not exist on server.\n";
        return;
    }

    Header decision;
    if (fs::exists(filename)) {
        std::cout << "File exists locally. Overwrite? (YES/NO): ";
        std::string choice;
        std::cin >> choice;
        if (choice != "YES") {
            std::strcpy(decision.command, "NO");
            asio::write(socket, asio::buffer(&decision, sizeof(Header)));
            return;
        }
    }
    
    std::strcpy(decision.command, "YES");
    asio::write(socket, asio::buffer(&decision, sizeof(Header)));

    // Read file payload
    std::ofstream outfile(filename, std::ios::binary);
    std::vector<char> file_buffer(8192);
    uint64_t remaining = response.filesize;

    while (remaining > 0) {
        size_t to_read = std::min(static_cast<uint64_t>(file_buffer.size()), remaining);
        size_t bytes_read = asio::read(socket, asio::buffer(file_buffer.data(), to_read));
        outfile.write(file_buffer.data(), bytes_read);
        remaining -= bytes_read;
    }
    std::cout << "Download complete.\n";
}


void receive_multiple_files(tcp::socket& socket, const std::string& extension) {
    Header req;
    std::strcpy(req.command, "MGET");
    std::strcpy(req.filename, extension.c_str());
    asio::write(socket, asio::buffer(&req, sizeof(Header)));

    while (true) {
        Header resp;
        asio::read(socket, asio::buffer(&resp, sizeof(Header)));
        
        if (std::string(resp.command) == "MDONE") {
            std::cout << "MGET complete.\n";
            break;
        }
        
        if (std::string(resp.command) == "MFILE") {
            std::string filename(resp.filename);
            Header decision;
            
            if (fs::exists(filename)) {
                std::cout << "File " << filename << " exists locally. Overwrite? (YES/NO): ";
                std::string choice;
                std::cin >> choice;
                std::strcpy(decision.command, choice == "YES" ? "YES" : "NO");
            } else {
                std::strcpy(decision.command, "YES");
            }
            
            asio::write(socket, asio::buffer(&decision, sizeof(Header)));

            if (std::string(decision.command) == "YES") {
                std::ofstream outfile(filename, std::ios::binary);
                std::vector<char> file_buffer(8192);
                uint64_t remaining = resp.filesize;

                while (remaining > 0) {
                    size_t to_read = std::min(static_cast<uint64_t>(file_buffer.size()), remaining);
                    size_t bytes_read = asio::read(socket, asio::buffer(file_buffer.data(), to_read));
                    outfile.write(file_buffer.data(), bytes_read);
                    remaining -= bytes_read;
                }
                std::cout << "Downloaded: " << filename << "\n";
            }
        }
    }
}


int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: client <Server IP Address> <Server Port number>\n";
        return 1;
    }

    try {
        asio::io_context io_context;
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(argv[1], argv[2]);

        tcp::socket socket(io_context);
        asio::connect(socket, endpoints);
        std::cout << "Connected to server.\n";

        std::string line, cmd, arg;
        while (true) {
            std::cout << "ftp> ";
            std::getline(std::cin, line);
            
            if (line.empty()) continue;

            // Parse the line into command and argument
            std::istringstream iss(line);
            iss >> cmd;

            // Handle commands that require an argument
            if (cmd == "PUT" || cmd == "GET" || cmd == "MPUT" || cmd == "MGET") {
                if (!(iss >> arg)) {
                    std::cerr << "Error: Command '" << cmd << "' requires an argument (e.g., " << cmd << " filename.txt)\n";
                    continue;
                }

                if (cmd == "PUT") {
                    send_file(socket, arg);
                } 
                else if (cmd == "GET") {
                    receive_file(socket, arg);
                }
                else if (cmd == "MPUT") {
                    bool found = false;
                    for (const auto& entry : fs::directory_iterator(".")) {
                        if (entry.is_regular_file() && entry.path().extension() == arg) {
                            std::cout << "Uploading: " << entry.path().filename().string() << "\n";
                            send_file(socket, entry.path().filename().string());
                            found = true;
                        }
                    }
                    if (!found) std::cout << "No local files found with extension " << arg << "\n";
                }
                else if (cmd == "MGET") {
                    receive_multiple_files(socket, arg);
                }
            } 
            // Optional: Handle a graceful exit command
            else if (cmd == "EXIT" || cmd == "QUIT") {
                std::cout << "Closing connection.\n";
                socket.close();
                break;
            } 
            // Handle unrecognized commands
            else {
                std::cerr << "Error: Unknown command '" << cmd << "'.\n";
                std::cout << "Available commands:\n"
                          << "  PUT <filename>   - Upload a file\n"
                          << "  GET <filename>   - Download a file\n"
                          << "  MPUT <extension> - Upload all files with extension\n"
                          << "  MGET <extension> - Download all files with extension\n"
                          << "  EXIT             - Close the client\n";
            }
        }
    } catch (std::exception& e) {
        std::cerr << "Client Exception: " << e.what() << "\n";
    }
    return 0;
}
