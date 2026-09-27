#define _DEFAULT_SOURCE
#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

const char* path = "/tmp/replay_files";
int max_op_value = 10;
const char* open_file_name = "/tmp/replay_files/open_file.txt";
const char *close_file_name = "/tmp/replay_files/close_file.txt";
const char *read_file_name = "/tmp/replay_files/read_file.txt";
const char* write_file_name = "/tmp/replay_files/write_file.txt";
int open_file_descriptor = 0;
int close_file_descriptor = 0;
int read_file_descriptor = 0;
int write_file_descriptor = 0;

//////////// Operations ////////////

/*Executes the open operation*/
void open_operation(char *file_path) {
  int fd = open(open_file_name, O_RDONLY);
}


void fopen_operation(char *file_path) {
  FILE *fp = fopen(file_path, "a+");
  if (fp == NULL) {
    perror("fopen");
  } else {
    fclose(fp);
  }
}

void fopen64_operation(char *file_path) {
  FILE *fp = fopen64(file_path, "a+");
  if (fp == NULL) {
    perror("fopen64");
  } else {
    fclose(fp);
  }
}


void openat_operation(char *file_path) {
  int fd = openat(AT_FDCWD, file_path, O_WRONLY | O_APPEND | O_CREAT);
  if (fd < 0) {
    perror("openat");
  } else {
    close(fd);
  }
}

/*Executes the close operation*/
void close_operation() {
  //int fd = rand() % 900 + 100;
  close(close_file_descriptor);
}

void fclose_operation() {
  /* rand() % 900 generates a random int between 0 and 899 */
  int fd = rand() % 900 + 100;
  printf("Closing file descriptor %d\n", fd);
  int res = fclose(fd);
  if (res != 0) {
    perror("fclose");
  } else {
    printf("Closed file descriptor %d\n", fd);
  }
  //if(fd==NULL)
  // fclose(fd);

}

#define CHUNK_SIZE 4096
/*Executes the read operation*/
void read_operation(int bytes) {
  //read_file_descriptor =  open(read_file_name, O_APPEND | O_CREAT, 0640);
  //printf("hixss: %d, %d\n", read_file_descriptor, bytes);
  if (bytes <= 0)
  {
    bytes = 0;
  }

  ssize_t bytes_read;
  size_t remaining = bytes;

  while (remaining > 0) {
    size_t to_read = remaining > CHUNK_SIZE ? CHUNK_SIZE : remaining;

    char buffer[to_read];
    bytes_read = read(read_file_descriptor, buffer, to_read);

    if (bytes_read == 0) {
        // EOF
        break;
    }
    remaining  -= bytes_read;
}

  /*
  ssize_t bytes_read;
  if (bytes > 8192)
  {
    char buffer[8192];
    bytes_read = read(read_file_descriptor, buffer, bytes);
  } else {
    char buffer[bytes];
    bytes_read = read(read_file_descriptor, buffer, bytes);
  }


  if (bytes_read < 0) {
    perror("read");
  } else {

  }*/
}

/*Executes the write operation*/
void write_operation(int bytes) {
  if (bytes <= 0)
  {
    bytes = 0;
  }
  ssize_t bytes_written;
  size_t remaining = bytes;


  while (remaining > 0) {
    size_t to_write = remaining > CHUNK_SIZE ? CHUNK_SIZE : remaining;

    char buffer[to_write];
    memset(buffer, 'A', sizeof(buffer));
    bytes_written =  write(write_file_descriptor, buffer, sizeof(buffer));


    if (bytes_written < 0) {
        // EOF
        break;
    }
    remaining  -= bytes_written;
}


  /*
  char buffer[bytes];

  memset(buffer, 'A', sizeof(buffer)); // Fill buffer with dummy data
  ssize_t bytes_written = write(write_file_descriptor, buffer, sizeof(buffer));
  if (bytes_written < 0) {
    perror("write");
  } else {
  }*/

  //close(fd);
}

/*
void replay_operation(char* file_path) {
  if (strcmp(op, "open") == 0 && num >= 3) {
    int fd = open(file_path, O_RDWR | O_CREAT, 0666);
    if (fd < 0)
      perror("open");
    else
      fd_map[0] = fd;
    printf("[%.3ld ms] open '%s' => %d\n", timestamp, arg1, fd);
  } else if (strcmp(op, "close") == 0 && num >= 3) {
    int id = atoi(arg1);
    close_operation("hi");
    printf("[%.3ld ms] close %d\n", timestamp, id);
  } else if (strcmp(op, "read") == 0 && num >= 4) {
    int id = atoi(arg1), size = atoi(arg2);
    ssize_t r = read(fd_map[id], buffer, size);
    printf("[%.3ld ms] read %d %d => %zd bytes\n", timestamp, id, size, r);
  } else if (strcmp(op, "write") == 0 && num >= 4) {
    int id = atoi(arg1), size = atoi(arg2);
    memset(buffer, 'A', size); // dummy data
    ssize_t w = write(fd_map[id], buffer, size);
    printf("[%.3ld ms] write %d %d => %zd bytes\n", timestamp, id, size, w);
  }  else {
    fprintf(stderr, "[%.3ld ms] Unknown or malformed op: %s", timestamp, line);
  }
}*/

typedef struct {
  char filename[256];
  long long global_start_time;
  long long offset_timestamp;
  long long start_time;
  long long duration;
} ThreadArgs;

long long get_current_time_ns() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

void *replay_file(void *arg) {
  ThreadArgs *args = (ThreadArgs *)arg;

  FILE *file = fopen(args->filename, "r");
  if (!file) {
    perror("Error opening file");
    return NULL;
  }

  if (args->start_time == -1){
        args->start_time = get_current_time_ns();
  }
  long long files_start_time = args->global_start_time;
  long long start_time = args->start_time;
  long long timestamp = -1;
  printf("Replaying file: %s\n", args->filename);

  char *line = NULL;
  size_t len = 0;

  //int i = 0;
  while (getline(&line, &len, file) != -1)
  {

    char operation[50];
    char response[256];


    if (sscanf(line, "%[^,],%lld,%*d,%*d,%*[^,],%s", operation, &timestamp, response) < 2) {
      continue; // Skip malformed lines
    }

    timestamp += args->offset_timestamp;

    // If not initialized with global_start_time, initialize with the first line
    if (args->global_start_time == -1) {      
      args->global_start_time = timestamp;
      files_start_time = timestamp;
    }

    // Se if we do not want to replay that line
    if ((timestamp - files_start_time) > args->duration){
      long long elapsed = get_current_time_ns() - start_time;
      long long wait_time = args->duration - elapsed;

      if (wait_time > 0) {
        struct timespec sleep_time = {wait_time / 1000000000,
                                      wait_time % 1000000000};
        nanosleep(&sleep_time, NULL);
      }
  
      break;
    }

    // Compute sleep time to synchronize replay
    long long elapsed = get_current_time_ns() - start_time;
    long long wait_time = (timestamp - files_start_time) - elapsed;

    if (wait_time > 0) {
      struct timespec sleep_time = {wait_time / 1000000000,
                                    wait_time % 1000000000};
      nanosleep(&sleep_time, NULL);
    }

    //sleep(1);
    // Simulate the operation (here, just print it)
    /*i++;
    if(i%5==0){
      printf("Thread for %s: %s at %lld with response %s\n", args->filename,
        operation, timestamp, response);

    }*/

    //nanosleep(1000);
    /* Simulate the operation (here, just print it) */
    if (strcmp(operation, "open") == 0){
      open_operation(open_file_name);
    } 
    else if (strcmp(operation, "open_var") == 0){
      open_operation(open_file_name);
    } 
    else if (strcmp(operation, "open_variadic") == 0){
      open_operation(open_file_name);
    } 
    else if (strcmp(operation, "open64_variadic") == 0){
      open_operation(open_file_name);
    } 
    else if (strcmp(operation, "open64_var") == 0){
      open_operation(open_file_name);
    } 
    else if (strcmp(operation, "openat") == 0)
    {
      open_operation(open_file_name);
    } 
    else if (strcmp(operation, "open64") == 0)
    {
      open_operation(open_file_name);
    }
    else if (strcmp(operation, "fopen") == 0)
    {
      open_operation(open_file_name);
    } 
    else if (strcmp(operation, "fopen64") == 0)
    {
      open_operation(open_file_name);
    } 
    else if (strcmp(operation, "fclose") == 0)
    {
      close_operation();
    }
    else if (strcmp(operation, "close") == 0)
    {
      close_operation();
    }
    else if (strcmp(operation, "read") == 0)
    {
      read_operation(atoi(response));
      //read_operation(atoi(response));
    } else if (strcmp(operation, "pread") == 0)
    {
      read_operation(atoi(response));
      //read_operation(atoi(response));
    }
    else if (strcmp(operation, "write") == 0)
    {
      write_operation(atoi(response));
    }
    else
    {
      //fprintf(stderr, "Unknown operation: %s\n", operation);
    }

    /*Execute each operation by invoking its respective function*/
    /*for(int j=0; j < reduc_ops; j++){
      char name[100];
      int op_name_value = j % max_op_value; 
      sprintf (name, "%stest_n%d_op%d.txt", path, i, op_name_value);
*/
      /*Execute operation*/
      //(*args->operation_func)(name);
   // }
  }

  
  if ((timestamp - files_start_time) <= args->duration){
    args->offset_timestamp = timestamp - files_start_time;
    replay_file(args);
    return NULL;
  }

  fclose(file);
  free(arg);
  return NULL;
}


void prep_system() {
  // Prepare the system for replay
  // This could include setting up directories, files, etc.
  // For now, we just create a temporary directory
  printf("Preparing system...\n");
  mkdir(path, 0700);  

  printf("Creating files...\n");
  // Create the files
  open_file_descriptor = open(open_file_name, O_RDONLY | O_APPEND | O_CREAT, 0777);
  close_file_descriptor = open(close_file_name, O_WRONLY | O_APPEND | O_CREAT, 0777);
  read_file_descriptor =  open(read_file_name, O_CREAT | O_RDONLY, 0777);
  write_file_descriptor = open(write_file_name, O_WRONLY | O_APPEND | O_CREAT, 0777);

  printf("Creating files.... \n Close File Descriptor: %d\n Read File Descriptor: %d\n Write File Descriptor: %d\n", close_file_descriptor, read_file_descriptor, write_file_descriptor);
}

int main(int argc, char **argv) {

  pthread_t thread;
  prep_system();


  ThreadArgs *args = malloc(sizeof(ThreadArgs));
  if (!args) {
    perror("malloc");
    return EXIT_FAILURE;
  }

  // If filename is just the path, adjust as needed
  strncpy(args->filename, argv[1], sizeof(args->filename) - 1);
  args->filename[sizeof(args->filename) - 1] = '\0';


  args->global_start_time = argv[2] ? atoll(argv[2]) : -1;
  args->offset_timestamp = 0;
  args->start_time=-1;
  args->duration = argv[3] ? atoll(argv[3]) * 1000000000 : -1;

  if (pthread_create(&thread, NULL, replay_file,args) != 0) {
    perror("Error creating thread");
    free(args);
    return EXIT_FAILURE;
  }

  pthread_join(thread, NULL);

  return EXIT_SUCCESS;
}
