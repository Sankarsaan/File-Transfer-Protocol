# Simple File Transfer Protocol (FTP) over TCP

A multithreaded client-server File Transfer Protocol (FTP) application built using C++17 and the standalone Asio networking library. This project implements reliable file transmission over TCP sockets, featuring interactive overwrite protection and bulk file transfer capabilities.

## Features

* **Core Commands:** Reliable single-file uploads (`PUT`) and downloads (`GET`).
* **Bulk Transfers:** Extension-based batch uploads (`MPUT`) and downloads (`MGET`) utilizing local directory iteration.
* **Interactive Overwrite Protection:** Before transferring, the system checks the destination for file existence and performs a synchronous handshake to prompt the user for permission to overwrite.
* **Concurrent Clients:** The server utilizes a thread-per-client model (`std::thread`), allowing multiple clients to connect and transfer files simultaneously without blocking each other.
* **Custom Protocol Framing:** Utilizes a fixed-size 280-byte binary header struct to manage TCP stream boundaries, preventing data corruption and packet fusion.

## Project Structure

* `protocol.hpp`: Contains the shared `Header` struct definition used for message framing and metadata (command, filename, filesize).
* `server.cpp`: The multithreaded server implementation. Listens for connections, processes commands, and manages local disk I/O.
* `client.cpp`: The interactive command-line interface. Connects to the server and parses user input to execute file transfers.

## Prerequisites

* **Compiler:** GCC/MinGW with C++17 support (required for `<filesystem>`).
* **Networking Library:** Standalone Asio (`#define ASIO_STANDALONE`).
* **Environment:** Windows (via MSYS2/MinGW).

## Compilation Instructions

To compile the executables on Windows via MSYS2, you must link the Windows Sockets API (`-lws2_32`). The `-static` flag is included to bundle necessary MinGW DLLs directly into the executable, preventing silent crashes when running outside the MSYS2 environment.

**Compile the Server:**
```bash
g++ server.cpp -o server.exe -std=c++17 -static -lws2_32
```

**Compile the Client:**
```bash
g++ client.cpp -o client.exe -std=c++17 -static -lws2_32
```

## Usage

### 1. Start the Server
The server requires a single command-line argument for the port number it should listen on.
```bash
# Syntax: ./server.exe <Port>
./server.exe 8080
```

### 2. Connect the Client
Open a separate terminal window and connect to the server using its IP address and port. Use `127.0.0.1` for local testing.
```bash
# Syntax: ./client.exe <Server IP Address> <Server Port number>
./client.exe 127.0.0.1 8080
```

### 3. FTP Commands
Once the client connects, you will drop into the `ftp>` prompt. The following commands are supported:

| Command | Syntax | Description |
| :--- | :--- | :--- |
| **PUT** | `PUT <filename>` | Uploads a specified local file to the server. Prompts for overwrite if it exists on the server. |
| **GET** | `GET <filename>` | Downloads a specified file from the server. Prompts for overwrite if it exists locally. |
| **MPUT** | `MPUT <.ext>` | Scans the local directory and uploads all files matching the specified extension (e.g., `MPUT .txt`). |
| **MGET** | `MGET <.ext>` | Requests all files matching the specified extension from the server. The server streams them sequentially. |

## Technical Implementation Details

* **Memory Efficiency:** Large files are not loaded entirely into RAM. Data is streamed continuously between the disk and the network using an 8KB (`8192` bytes) cyclic buffer.
* **Stream Separation:** Because TCP provides a continuous stream of bytes without boundaries, the exact `filesize` is transmitted in the protocol header. The receiving application counts every byte during read operations and strictly terminates the loop when the payload size is met, ensuring the next bytes read are correctly interpreted as the next command header.
