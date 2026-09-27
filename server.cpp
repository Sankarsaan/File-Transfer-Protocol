#define ASIO_STANDALONE
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <thread>
#include <asio.hpp>
#include "protocol.hpp"

using asio::ip::tcp;
namespace fs = std::filesystem;

// Socket is passed by value to trigger the move constructor, taking ownership
void handle_client(tcp::socket socket) {
    try {
        asio::error_code error;
        while (true) {
            Header header;
            asio::read(socket, asio::buffer(&header, sizeof(Header)), error);
            if (error == asio::error::eof) break; 
            else if (error) throw asio::system_error(error);

            std::string cmd(header.command);
            std::string arg(header.filename);

            if (cmd == "PUT") {
                Header response;
                if (fs::exists(arg)) {
                    std::strcpy(response.command, "EXISTS");
                    asio::write(socket, asio::buffer(&response, sizeof(Header)));
                    
                    Header decision;
                    asio::read(socket, asio::buffer(&decision, sizeof(Header)));
                    if (std::string(decision.command) == "NO") continue; 
                } else {
                    std::strcpy(response.command, "OK");
                    asio::write(socket, asio::buffer(&response, sizeof(Header)));
                }

                std::ofstream outfile(arg, std::ios::binary);
                std::vector<char> file_buffer(8192);
                uint64_t remaining = header.filesize;

                while (remaining > 0) {
                    size_t to_read = std::min(static_cast<uint64_t>(file_buffer.size()), remaining);
                    size_t bytes_read = asio::read(socket, asio::buffer(file_buffer.data(), to_read));
                    outfile.write(file_buffer.data(), bytes_read);
                    remaining -= bytes_read;
                }
                std::cout << "Received file: " << arg << "\n";
            } 
            else if (cmd == "GET") {
                Header response;
                if (!fs::exists(arg)) {
                    std::strcpy(response.command, "ERR");
                    asio::write(socket, asio::buffer(&response, sizeof(Header)));
                    continue;
                }

                std::strcpy(response.command, "OK");
                response.filesize = fs::file_size(arg);
                std::strcpy(response.filename, arg.c_str());
                asio::write(socket, asio::buffer(&response, sizeof(Header)));

                Header decision;
                asio::read(socket, asio::buffer(&decision, sizeof(Header)));
                if (std::string(decision.command) == "NO") continue;

                std::ifstream infile(arg, std::ios::binary);
                std::vector<char> file_buffer(8192);
                while (infile.read(file_buffer.data(), file_buffer.size()) || infile.gcount() > 0) {
                    asio::write(socket, asio::buffer(file_buffer.data(), infile.gcount()));
                }
            }
            else if (cmd == "MGET") {
                // arg contains the extension (e.g., ".txt")
                for (const auto& entry : fs::directory_iterator(".")) {
                    if (entry.is_regular_file() && entry.path().extension() == arg) {
                        Header mfile;
                        std::strcpy(mfile.command, "MFILE");
                        std::strcpy(mfile.filename, entry.path().filename().string().c_str());
                        mfile.filesize = fs::file_size(entry.path());
                        
                        // Send metadata for this specific file
                        asio::write(socket, asio::buffer(&mfile, sizeof(Header)));

                        // Wait for client to confirm overwrite
                        Header decision;
                        asio::read(socket, asio::buffer(&decision, sizeof(Header)));
                        if (std::string(decision.command) == "NO") continue; // Skip this file

                        // Send file payload
                        std::ifstream infile(entry.path(), std::ios::binary);
                        std::vector<char> file_buffer(8192);
                        while (infile.read(file_buffer.data(), file_buffer.size()) || infile.gcount() > 0) {
                            asio::write(socket, asio::buffer(file_buffer.data(), infile.gcount()));
                        }
                    }
                }
                // Signal end of MGET transmission
                Header mdone;
                std::strcpy(mdone.command, "MDONE");
                asio::write(socket, asio::buffer(&mdone, sizeof(Header)));
            }
        }
    } catch (std::exception& e) {
        std::cerr << "Client thread error: " << e.what() << "\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) return 1;

    try {
        asio::io_context io_context;
        int port = std::stoi(argv[1]);
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));
        std::cout << "Server listening on port " << port << "...\n";

        while (true) {
            tcp::socket socket(io_context);
            acceptor.accept(socket);
            std::cout << "Client connected.\n";
            // Spawn a new thread, move the socket into it, and detach
            std::thread(handle_client, std::move(socket)).detach();
        }
    } catch (std::exception& e) {
        std::cerr << "Server Exception: " << e.what() << "\n";
    }
    return 0;
}