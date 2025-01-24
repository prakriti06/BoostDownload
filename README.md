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

2. Navigate to the project directory
   ```bash
   cd multithreaded-downloader
3. Install Dependencies
   ```bash
   sudo apt install libcurl4-openssl-dev
4. Build and Run the project
   ```bash
   make
   ./multithreadedDownloader -u <url> -o <filename> -n <number_of_threads>

## Video Demonstration

<video width="640" height="360" controls>
  <source src="[Demo.mp4](https://github.com/prakriti06/BoostDownload/blob/ccc221e83c4392e846aaaf1a4cee51ec9483aa3e/Demo.mp4.mp4)" type="video/mp4">
  Your browser does not support the video tag.
</video>

### Steps to add it to your project:
1. Create a new file named `README.md` in the root of your project.
2. Paste the content above into the `README.md` file.
3. Add and commit the file to Git:

```bash
git add README.md
git commit -m "Add README file"
git push



