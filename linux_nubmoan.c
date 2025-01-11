// To compile: gcc -Wall -g -lm -pthread -o linux-nubmoan linux_nubmoan.c
// you need root permissions to access the input device
// so run the compiled binary with sudo

#include <fcntl.h>
#include <linux/input.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_INTENSITY_LEVELS 10

volatile int current_intensity = 0;
volatile bool is_playing = false;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int calculateIntensity(int dx, int dy) {
  int intensity = (int)sqrt(dx * dx + dy * dy);
  if (intensity < 1) {
    intensity = 1;
  } else if (intensity > 10) {
    intensity = 10;
  }
  return intensity;
}

void playSoundFile(int intensity) {
  char command[50];
  snprintf(command, sizeof(command), "aplay moanswav/%d.wav", intensity);
  system(command);
}

void *soundPlayerThread(void *unused) {
  while (1) {
    pthread_mutex_lock(&mutex);

    while (current_intensity == 0) {
      pthread_cond_wait(&cond, &mutex);
    }

    int intensity_to_play = current_intensity;
    current_intensity = 0;
    is_playing = true;

    pthread_mutex_unlock(&mutex);

    playSoundFile(intensity_to_play);

    pthread_mutex_lock(&mutex);
    is_playing = false;
    pthread_mutex_unlock(&mutex);
  }
  return NULL;
}

void monitorDevice(const char *devicePath) {
  pthread_t player_thread;
  pthread_create(&player_thread, NULL, soundPlayerThread, NULL);

  int fd = open(devicePath, O_RDONLY);
  if (fd < 0) {
    perror("Failed to open device");
    return;
  }

  struct input_event ev;
  while (1) {
    ssize_t n = read(fd, &ev, sizeof(struct input_event));
    if (n < (ssize_t)sizeof(struct input_event)) {
      perror("Error reading input event");
      break;
    }

    if (ev.type == EV_REL) {
      if (ev.code == REL_X || ev.code == REL_Y) {
        int dx = (ev.code == REL_X) ? ev.value : 0;
        int dy = (ev.code == REL_Y) ? ev.value : 0;

        int new_intensity = calculateIntensity(dx, dy);

        pthread_mutex_lock(&mutex);
        if (!is_playing || new_intensity != current_intensity) {
          current_intensity = new_intensity;
          pthread_cond_signal(&cond);
        }
        pthread_mutex_unlock(&mutex);
      }
    }
  }

  close(fd);
}

int main() {
  const char *devicePath = "/dev/input/event13"; // Change this to the path of your input device
  monitorDevice(devicePath);
  return 0;
}

