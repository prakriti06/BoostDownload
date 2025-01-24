#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <curl/curl.h>
#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <ncurses.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#define RESET "\033[0m"
#define BOLD_BLUE "\033[1;34m" // Bright Blue (Bold Blue)

/* =======================STRUCTS                              
                                 ============================= */
typedef struct {
  char *url;        // URL to download from
  char *filename;   // filename to save to
  int max_threads;  // maximum number of threads
} DLSettings;       // settings for downloader

typedef struct {
  int index;                 // thread index
  char *url;                 // URL to download from
  unsigned long long start;  // start byte
  unsigned long long end;    // end byte
} DLThreadArgs;              // arguments for each thread

typedef struct {
  pthread_t thread;    // thread handle
  DLThreadArgs *args;  // thread arguments
  CURL *curl;          // curl handle
  FILE *buffer;        // file handle
} DLThreadInfo;        // information about each thread

typedef struct {
  curl_off_t *total_bytes;       // total bytes to download
  curl_off_t *downloaded_bytes;  // downloaded bytes so far
} DLProgress;                    // progress information

/* =======================DEFS and GLOBALS
                                          ==================== */
#define DEFAULT_MAX_THREADS 4
#define CLEAR_SCREEN "\033[2J\033[1;1H"
#define CHECKMARK "\u2713"
#define CROSSMARK "\u2717"
#define BOLD "\033[1m"
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define CYAN "\033[36m"
#define WHITE "\033[37m"
#define GREY "\033[90m"

DLThreadInfo **thread_infos;      // global array of thread_infos
DLProgress progress;              // global progress
DLSettings settings;              // global settings
int window_width;                 // terminal width
int window_height;                // terminal height
char log_buffer[2048];            // buffer to store logs from threads
pthread_mutex_t completed_mutex;  // mutex for completed_counter
int completed_counter = 0;        // counter for completed threads
time_t start_time;                // start time of download
bool paused;                      // whether download is paused

/* ==================RENDERING and INTERFACE
                                            =================== */
// Calculate width and print at center
void print_center(char *str) {
  int padding = (window_width - strlen(str)) / 2;
  printf("%*s", padding, "");
  printf("%s", str);
}

// Function to print the header
void print_header() {
  for (int i = 0; i < window_width; i++) {
    printf("="); // Print the top border
  }
  
  printf(RESET "\n\n" BOLD_BLUE); // Set to BOLD_BLUE and reset after text
  
  print_center("MULTI-THREADED DOWNLOADER");
  
  printf("\n\n" RESET); // Reset after header text
  
  for (int i = 0; i < window_width; i++) {
    printf("="); // Print the bottom border
  }
  printf("\n\n" RESET); // Reset after the header section
}

// Print download info
void print_download_info() {
  printf(BOLD);
  print_center("[ Download Info ]");
  printf("\n\n" RESET CYAN BOLD);
  print_center(settings.url);
  printf("\n" GREEN);
  print_center(settings.filename);
  printf("\n\n" RESET);
}

// Clear screen
void clear_screen() { system("clear"); }

/* =======================DOWNLOAD SETUP
                                        ======================= */
// Use getopts to parse command line arguments
void parse_args(int argc, char *argv[]) {
  // ./mtdown -u <url> -o <filename> -n <max_threads>
  int opt;
  while ((opt = getopt(argc, argv, "u:o:n:")) != -1) {
    switch (opt) {
      case 'u':
        settings.url = optarg;
        break;
      case 'o':
        settings.filename = optarg;
        break;
      case 'n':
        // Check if optarg is a number using atoi
        if (atoi(optarg) == 0) {
          fprintf(stderr, "Error: max_threads must be a number\n");
          exit(EXIT_FAILURE);
        }
        // Check if optarg is valid (1 - 32)
        if (atoi(optarg) < 1 || atoi(optarg) > 32) {
          fprintf(stderr, "Error: max_threads must be between 1 and 32\n");
          exit(EXIT_FAILURE);
        }
        settings.max_threads = atoi(optarg);
        break;
      default:
        fprintf(stderr, "Usage: %s -u <url> -o <filename> -n <max_threads>\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  // Check if url is provided
  if (settings.url == NULL) {
    fprintf(stderr, "Usage: %s -u <url> -o <filename> -n <max_threads>\n",
            argv[0]);
    exit(EXIT_FAILURE);
  }

  // Check if filename is provided
  if (settings.filename == NULL) {
    fprintf(stderr, "Usage: %s -u <url> -o <filename> -n <max_threads>\n",
            argv[0]);
    exit(EXIT_FAILURE);
  }

  // Check if max_threads is provided
  if (settings.max_threads == 0) {
    settings.max_threads = DEFAULT_MAX_THREADS;
  }
}

// Callback function to disable writing from curl, this is to gather data about
// the server before actually downloading
size_t no_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
  return size * nmemb;
}

// Find max concurrent connection the server allows by sending a series of
// concurrent requests and then record when a thread fails to receive data
void *find_max_thread_worker() {
  // Setup curl
  CURL *curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, settings.url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, no_write_callback);
  curl_easy_setopt(curl, CURLOPT_MAX_RECV_SPEED_LARGE, (curl_off_t)1);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 1000);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
  curl_easy_perform(curl);
  int res = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res);
  curl_easy_cleanup(curl);
  // Return res as void pointer for later conversion
  return (void *)res;
}

int find_max_threads() {
  clear_screen();
  print_header();

  int max_threads = 1;

  printf(BOLD_BLUE "Testing server's capability to handle multiple threads...\n" RESET);

  // Find max concurrent connections by testing the maximum number of concurrent
  // connections the server allows before returning an error response.
  for (int i = 1; i <= settings.max_threads; i++) {
    printf("Using %dst threads... ", i);
    fflush(stdout);

    pthread_t threads[i];

    for (int j = 0; j < i; j++)
      pthread_create(&threads[j], NULL, find_max_thread_worker, NULL);

    for (int j = 0; j < i; j++) {
      void *res;
      pthread_join(threads[j], &res);
      if ((int)res != 200) {
        printf(RED "%s\n" RESET, CROSSMARK);
        return i - 1;
      }
    }
    printf(GREEN "%s\n" RESET, CHECKMARK);
    max_threads = i;

    // Sleep to give the server time to recover
    sleep(1);
  }

  return max_threads;
}

// Initialize download thread structure and prepare the download
void init_download_threads() {
  CURL *curl;
  CURLcode res;
  FILE *file;
  long file_size;

  curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, "Curl initialization failed!\n");
    exit(EXIT_FAILURE);
  }

  curl_easy_setopt(curl, CURLOPT_URL, settings.url);
  curl_easy_setopt(curl, CURLOPT_NOBODY, 1); // Do not download data yet
  curl_easy_setopt(curl, CURLOPT_HEADER, 1); // Get header info
  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "Error while fetching header: %s\n", curl_easy_strerror(res));
    exit(EXIT_FAILURE);
  }

  curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &file_size);
  if (file_size <= 0) {
    fprintf(stderr, "Failed to get content length\n");
    exit(EXIT_FAILURE);
  }

  settings.max_threads = find_max_threads();

  // Open the file for writing
  file = fopen(settings.filename, "wb");
  if (!file) {
    fprintf(stderr, "Failed to open file for writing\n");
    exit(EXIT_FAILURE);
  }

  curl_easy_cleanup(curl);
  fclose(file);
}

// Downloading file in multiple threads
void *download_thread(void *arg) {
  DLThreadArgs *args = (DLThreadArgs *)arg;
  CURL *curl;
  CURLcode res;
  FILE *file;

  curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, "Curl initialization failed!\n");
    return NULL;
  }

  // Set download range
  char range[100];
  snprintf(range, sizeof(range), "%llu-%llu", args->start, args->end);
  curl_easy_setopt(curl, CURLOPT_RANGE, range);
  curl_easy_setopt(curl, CURLOPT_URL, args->url);
  
  // Open file for appending
  file = fopen(settings.filename, "ab");
  if (!file) {
    fprintf(stderr, "Failed to open file for appending\n");
    return NULL;
  }
  
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "Download failed: %s\n", curl_easy_strerror(res));
  }

  fclose(file);
  curl_easy_cleanup(curl);

  // Update the completed thread count
  pthread_mutex_lock(&completed_mutex);
  completed_counter++;
  pthread_mutex_unlock(&completed_mutex);

  return NULL;
}

void start_download() {
  // Divide the file size among the threads
  long file_size = 1024 * 1024 * 100;  // Example: 100 MB file size
  unsigned long long chunk_size = file_size / settings.max_threads;
  thread_infos = malloc(settings.max_threads * sizeof(DLThreadInfo *));
  
  for (int i = 0; i < settings.max_threads; i++) {
    DLThreadArgs *args = malloc(sizeof(DLThreadArgs));
    args->index = i;
    args->url = settings.url;
    args->start = i * chunk_size;
    args->end = (i == settings.max_threads - 1) ? file_size : (i + 1) * chunk_size - 1;
    thread_infos[i] = malloc(sizeof(DLThreadInfo));
    thread_infos[i]->args = args;
    pthread_create(&thread_infos[i]->thread, NULL, download_thread, args);
  }

  // Wait for all threads to finish
  for (int i = 0; i < settings.max_threads; i++) {
    pthread_join(thread_infos[i]->thread, NULL);
  }

  printf("Download completed.\n");
}

int main(int argc, char *argv[]) {
    // Initialize Curl
    CURL *curl = curl_easy_init();
    if (curl) {
        curl_easy_cleanup(curl);
    }

    // Parse command-line arguments
    parse_args(argc, argv);

    // Initialize the download
    init_download_threads();

    // Start download
    start_download();

    // Add any other logic or cleanup as necessary
    printf("Program finished.\n");
    return 0;
}
