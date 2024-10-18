#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

#define MAX_DEVICES 32
#define COOLDOWN_PERIOD 1.0 // Cooldown period in seconds
#define MAX_PATH 260
#define STEP_SIZE 100 // Change file every 50 units of movement
#define EVENT_COUNT_LIMIT 6 // Number of events to track before playing a sound
#define MAX_FILES 10 // Number of available files

typedef struct {
    HANDLE device;
    char name[256];
    BOOL isTrackpad;
    BOOL isTrackpoint;
} DeviceInfo;

DeviceInfo devices[MAX_DEVICES];
int deviceCount = 0;
clock_t lastSoundTime = 0;
BOOL isPlaying = FALSE;

int cumulativeMovement = 0; // Track the cumulative movement
int eventCounter = 0;        // Counter for mouse events after cooldown

// Function to get a WAV file based on the intensity of mouse movement
const char* getWAVByMovement(int totalMovement, const char* folderPath) {
    int fileIndex = totalMovement / STEP_SIZE; // Increase file every 50 units
    if (fileIndex >= MAX_FILES) {
        fileIndex = MAX_FILES - 1; // Cap it to the max number of files
    }

    static char selectedFile[MAX_PATH];
    snprintf(selectedFile, MAX_PATH, "%s\\file%d.wav", folderPath, fileIndex + 1); // Files named file1.wav, file2.wav, ...
    return selectedFile;
}

void playWAV(const char* filePath, int totalMovement) {
    clock_t currentTime = clock();
    double elapsedSeconds = ((double)(currentTime - lastSoundTime)) / CLOCKS_PER_SEC;

    if (elapsedSeconds >= COOLDOWN_PERIOD) {
        // Play the WAV file using PlaySound
        if (PlaySound(filePath, NULL, SND_FILENAME | SND_ASYNC)) {
            printf("Playing WAV: %s | Total Movement: %d\n", filePath, totalMovement);
            lastSoundTime = currentTime;
            isPlaying = TRUE;
        } else {
            printf("Failed to play WAV: %s\n", filePath);
        }
        // Reset movement tracking after playing a sound
        cumulativeMovement = 0;
        eventCounter = 0;
    } else {
        printf("Sound cooldown active. %.2f seconds left.\n", COOLDOWN_PERIOD - elapsedSeconds);
    }
}

void initializeDevices() {
    UINT numDevices;
    GetRawInputDeviceList(NULL, &numDevices, sizeof(RAWINPUTDEVICELIST));

    PRAWINPUTDEVICELIST deviceList = malloc(sizeof(RAWINPUTDEVICELIST) * numDevices);
    GetRawInputDeviceList(deviceList, &numDevices, sizeof(RAWINPUTDEVICELIST));

    for (UINT i = 0; i < numDevices && deviceCount < MAX_DEVICES; i++) {
        if (deviceList[i].dwType == RIM_TYPEMOUSE) {
            RID_DEVICE_INFO deviceInfo;
            UINT deviceInfoSize = sizeof(RID_DEVICE_INFO);
            deviceInfo.cbSize = sizeof(RID_DEVICE_INFO);
            
            GetRawInputDeviceInfo(deviceList[i].hDevice, RIDI_DEVICEINFO, &deviceInfo, &deviceInfoSize);

            char deviceName[256];
            UINT nameSize = sizeof(deviceName);
            GetRawInputDeviceInfo(deviceList[i].hDevice, RIDI_DEVICENAME, deviceName, &nameSize);

            devices[deviceCount].device = deviceList[i].hDevice;
            strncpy(devices[deviceCount].name, deviceName, sizeof(devices[deviceCount].name));
            
            devices[deviceCount].isTrackpad = (strstr(deviceName, "TouchPad") != NULL);
            devices[deviceCount].isTrackpoint = (strstr(deviceName, "TrackPoint") != NULL);

            deviceCount++;
        }
    }

    free(deviceList);
}

const char* getDeviceType(HANDLE device) {
    for (int i = 0; i < deviceCount; i++) {
        if (devices[i].device == device) {
            if (devices[i].isTrackpad) return "Trackpad";
            if (devices[i].isTrackpoint) return "TrackPoint";
            return "Mouse";
        }
    }
    return "Unknown";
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static const char* wavFolderPath = "C:\\Users\\molly\\git\\nubmoan\\moanswav"; // Change this to your WAV folder path

    switch (message) {
        case WM_INPUT: {
            UINT dwSize;
            GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
            LPBYTE lpb = malloc(dwSize);
            if (lpb == NULL) return 0;

            if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
                printf("GetRawInputData doesn't return correct size!\n");

            RAWINPUT* raw = (RAWINPUT*)lpb;

if (raw->header.dwType == RIM_TYPEMOUSE) {
    HANDLE device = raw->header.hDevice;
    const char* deviceType = getDeviceType(device);
    
    // Check if there's actual mouse movement (non-zero dx or dy)
    if (raw->data.mouse.lLastX != 0 || raw->data.mouse.lLastY != 0) {
        printf("Mouse moved: dx=%ld, dy=%ld, Device: %s\n",
               raw->data.mouse.lLastX, raw->data.mouse.lLastY, deviceType);
        
        if (strcmp(deviceType, "Mouse") == 0) {
            // Track cumulative absolute movement for 6 events
            cumulativeMovement += abs(raw->data.mouse.lLastX) + abs(raw->data.mouse.lLastY);
            eventCounter++;

            if (eventCounter >= EVENT_COUNT_LIMIT) {
                // Get the appropriate WAV file based on cumulative movement
                const char* wavFile = getWAVByMovement(cumulativeMovement, wavFolderPath);
                playWAV(wavFile, cumulativeMovement);
            }
        }
    }
    // Ignore click events (we're not processing them anymore)
}

            free(lpb);
            return 0;
        }
        case WM_DESTROY:
            if (isPlaying) {
                PlaySound(NULL, 0, 0); // Stop playing any sound
            }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

int main() {
    srand(time(NULL)); // Initialize random seed
    initializeDevices();

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "RawInputWndClass";
    RegisterClass(&wc);

    HWND hWnd = CreateWindow(wc.lpszClassName, "RawInput", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, wc.hInstance, NULL);

    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x02;
    rid.dwFlags = RIDEV_INPUTSINK;
    rid.hwndTarget = hWnd;
    RegisterRawInputDevices(&rid, 1, sizeof(rid));

    printf("Mouse movement tracker started. Press Ctrl+C to exit.\n");

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
