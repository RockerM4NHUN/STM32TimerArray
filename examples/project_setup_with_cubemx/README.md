# Project setup with STM32CubeMX tutorial
This file walks through the creation of an STM32 microcontroller based project with PLatformIO and CubeMX, using C/C++ language.
You will need to install VSCode, STM32CubeMX and the PlatformIO extension for VSCode before proceeding further.

### 1. Create a PlatformIO project
- In VSCode open PlatformIO (PIO) Home and click *New Project*.
- Choose a fitting project name.
- Select your STM32 board.
- Select *STM32Cube* framework.
- Optionally specify the project location.
- Click *Finish* to create the project.

### 2. Create STM32CubeMX project
- In CubeMX select your STM32 board and start a project.
- Make the settings you need for your application (you can update this later easily).
- Optionally set the baud rate of the default UART/USART serial port to 115200 for decent speed.
- Navigate to the *Project Manager* tab.
- Under *Project* select the *Other Toolchains (GPDSC)* toolchain.
- Save the CubeMX project file in the PIO project's root directory. (You want to overwrite it.)
- Click *Generate Code* to create the source files.

### 3. Link the PlatformIO and CubeMX projects
- Open the *platformio.ini* file.
- Under build options for your board add in a new line: `build_flags = -IInc`
- Optionally if you want to use the PIO Monitor for serial communication, also add this: `monitor_speed = 115200`
  (the speed has to match the settings for the serial port in CubeMX)

### 4. Compile with the STM32TimerArray library
- In the *platformio.ini* file add: `lib_deps = STM32TimerArray`
- From the folder of the readme file copy *app.cpp* to *src* and *app.h* to *Inc*
- In *src/main.c* add `#include "app.h"` between `/* USER CODE BEGIN Includes */` and `/* USER CODE END Includes */`.
- Also add `app_start();` between `/* USER CODE BEGIN 2 */` and `/* USER CODE END 2 */`, before the while loop in the main function.
- (For C++ headers use .hpp extension to have correct VSCode language detection).
- At this point your project should compile and the library is accessable from *app.cpp*.

### 5. Run examples
- Follow the instructions in the example's readme file, copy *app.cpp* provided for the example to *src* (overwriting this example).
- Connect your STM32 board.
- Click PlatformIO Upload.
- The project should compile, upload and execute.
- In this project setup example the user LED will blink, using the HAL environment.
