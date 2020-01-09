# Blinky
This example shows the basic steps for setting up a timer using STM32TimerArray and CubeMX.\
A ready to use baseline project is required to continue.

See the `project_setup_with_cubemx` example for getting started and setting up a baseline project.\
If you are experienced with the STM32 HAL environment and with writing setup code, see `project_setup_with_hal`.

### 1. Configure the hardware
- Open the STM32CubeMX configuration file.
- Under *Timers* open *TIM2* and set the clock source to *Internal Clock*. (By default the frequency of TIM2 will match the CPU's.)
- Under *NVIC Settings* enable *TIM2 global interrupt*.
- Make sure, that the user LED is named `LD2` and configured as output. (This is the case by default.)
- Click *Generate Code* to update settings in source.

### 2. Setup software
- Copy the contents of *blinky.cpp* to *src/app.cpp* in your project.
- Click PlatformIO Upload.
- The user LED should flash once a second.

### 3. Modify the code
- Change the delay of `t_toggle` to 50 ticks, and reupload.
- The user LED should flash ten times a second.
