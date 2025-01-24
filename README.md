# Multi-Threaded Downloader

A command-line utility for downloading files from a given URL using multiple threads to accelerate the download process. This downloader divides the file into smaller parts and downloads them concurrently, improving download speed on supported servers.

## Features

- Multi-threaded downloading (up to 32 threads, configurable)
- Resumes download if interrupted
- Displays download progress in the terminal
- Supports setting the URL and filename via command-line arguments
- Finds the maximum concurrent connections supported by the server

## Requirements

- **C Compiler** (GCC recommended)
- **libcurl** (used for handling HTTP requests)

## Installation

1. Clone the repository:
   ```bash
   git clone https://github.com/your-username/multithreaded-downloader.git
