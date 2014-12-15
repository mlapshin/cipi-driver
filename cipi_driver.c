#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>

#define SPIDEV "/dev/spidev0.0"
#define SOCKET_PATH "/tmp/cipi.socket"

#define DISPLAY_SIZE 8 * 8 * 5
/* #define DEBUG */

/* 0 - blank,
   1 - green
   2 - red
   3 - yellow (both) */
static unsigned char display[DISPLAY_SIZE];
static pthread_t draw_thread;
static pthread_t socket_thread;
static int connection_fd;
static int spidev = 0;
static unsigned char need_to_exit = 0;
static int socket_fd = 0;


void clear_screen() {
  unsigned char bytes[11] = {255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  write(spidev, bytes, 11);
}

void sigint_handler(int s) {
  printf("Caught SIGINT\n");
  pthread_cancel(socket_thread);
  pthread_cancel(draw_thread);
  need_to_exit = 1;
}

void cleanup() {
  clear_screen();
  close(connection_fd);
  close(spidev);
}

void setup_sigint_handler() {
  struct sigaction a;
  a.sa_handler = sigint_handler;
  a.sa_flags = 0;
  sigemptyset(&a.sa_mask);
  sigaction(SIGINT, &a, NULL);
}

unsigned char get_column_byte(unsigned char matrix_num,
                              unsigned char column_num,
                              unsigned char color) {
  unsigned char byte = 0;

  for(int i = 0; i < 8; i++) {
    unsigned char pixel = display[(matrix_num * 8 * 8) + (i * 8) + column_num];
    byte = byte | ((pixel == color || pixel == 3) ? (1 << i) : 0);
  }

  return byte;
}

unsigned char flip_byte(unsigned char b) {
  unsigned char res = 0;

  for(int i = 0; i < 8; i++) {
    res = res | ((b & (1 << (7 - i))) == 0 ? 0 : (1 << i));
  }

  return res;
}

#ifdef DEBUG
const char *byte_to_binary(int x) {
  static char b[9];
  b[0] = '\0';

  int z;
  for (z = 128; z > 0; z >>= 1) {
    strcat(b, ((x & z) == z) ? "1" : "0");
  }

  return b;
}
#endif

void draw_column(char column_num) {
  unsigned char bytes[11];
  signed char m = 0;
  bytes[0] = 1 << (7 - column_num);     /* first byte is a column number */

  for(m = 0; m < 5; m++) {
    bytes[1 + m] = flip_byte(get_column_byte((4 - m), column_num, 2));
  }

  for(m = 0; m < 5; m++) {
    bytes[1 + 5 + m] = get_column_byte(m, column_num, 1);
  }

#ifdef DEBUG
  for(m = 0; m < 11; m++) {
    printf("%s ", byte_to_binary(bytes[m]));
  }
  printf("\n");
#endif

  write(spidev, bytes, 11);
}

void* draw_thread_fn(void* arg) {
  while(!need_to_exit) {
    for(int i = 0; i < 8; i++) {
      draw_column(i);
    }
  }

  return NULL;
}

void* socket_thread_fn(void* arg) {
  int n_bytes = 0;
  unsigned char buffer[DISPLAY_SIZE];
  memset(buffer, 0, DISPLAY_SIZE);

  while(!need_to_exit) {
    if (connection_fd == 0) {
      if ((connection_fd = accept(socket_fd, NULL, NULL)) < 0) {
        perror("accept() error");
      }
    }

    n_bytes = read(connection_fd, buffer, DISPLAY_SIZE);

    if (n_bytes > 0) {
      memcpy(display, buffer, DISPLAY_SIZE);
      printf("read %d bytes from socket\n", n_bytes);
    } else {
      perror("read() error");
      close(connection_fd);
      connection_fd = 0;
    }
  }

  return NULL;
}

void create_socket() {
  struct sockaddr_un addr;

  if ((socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
    perror("socket() error");
    exit(-1);
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
  unlink(SOCKET_PATH);

  if (bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    perror("bind() error");
    exit(-1);
  }

  if (listen(socket_fd, 5) == -1) {
    perror("listen() error");
    exit(-1);
  }

  chmod(SOCKET_PATH, S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR|S_IWGRP|S_IWOTH);
}

int main(int argc, char** argv) {
  memset(display, 0, DISPLAY_SIZE);

  setup_sigint_handler();
  create_socket();

  spidev = open(SPIDEV, O_RDWR);
  pthread_create(&draw_thread, NULL, &draw_thread_fn, NULL);
  pthread_create(&socket_thread, NULL, &socket_thread_fn, NULL);

  pthread_join(draw_thread, NULL);
  pthread_join(socket_thread, NULL);

  printf("Bye!\n");
  cleanup();

  return 0;
}
