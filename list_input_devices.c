// To compile: gcc -Wall -g -o list_input_devices list_input_devices.c
// run the compiled binary with sudo, e.g. sudo ./list_input_devices
// note the device path of the input device you want to monitor
// i.e. the trackpoint in this case. for me it was /dev/input/event13
// Modify the string in linux_nubmoan.c main function to the device path you
// found

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define DEV_INPUT_PATH "/dev/input"
#define EVENT_PREFIX "event"
#define DEVICE_PATH_MAX 300 // Increased buffer size

void listInputDevices() {
  DIR *dir = opendir(DEV_INPUT_PATH);
  if (!dir) {
    perror("Failed to open /dev/input");
    return;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (strncmp(entry->d_name, EVENT_PREFIX, strlen(EVENT_PREFIX)) == 0) {
      char devicePath[DEVICE_PATH_MAX];

      // Truncate the device name if it's too long
      snprintf(devicePath, sizeof(devicePath), "%s/%.*s", DEV_INPUT_PATH,
               (int)(sizeof(devicePath) - strlen(DEV_INPUT_PATH) - 2),
               entry->d_name);

      int fd = open(devicePath, O_RDONLY);
      if (fd < 0) {
        perror("Failed to open device");
        continue;
      }

      char deviceName[256] = "Unknown Device";
      if (ioctl(fd, EVIOCGNAME(sizeof(deviceName)), deviceName) < 0) {
        perror("Failed to get device name");
      }

      printf("Device: %s | Path: %s\n", deviceName, devicePath);
      close(fd);
    }
  }

  closedir(dir);
}

int main() {
  printf("Listing input devices...\n");
  listInputDevices();
  return 0;
}
