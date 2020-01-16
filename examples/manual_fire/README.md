# Manual fire
This example shows the usage of manual fire functionality, using STM32TimerArray and CubeMX.\
A ready to use baseline project is required to continue.

See the `project_setup_with_cubemx` example for getting started and setting up a baseline project.\
If you are experienced with the STM32 HAL environment and with writing setup code, see `project_setup_with_hal`.

### 1. Configure the hardware
- Open the STM32CubeMX configuration file.
- Under *Timers* open *TIM2* and set the clock source to *Internal Clock*. (By default the frequency of TIM2 will match the CPU's.)
- Under *NVIC Settings* enable *TIM2 global interrupt*.
- Make sure, that the user LED is named `LD2` and configured as *GPIO_Output*. (This is the case by default.)
- Set the pin corresponding to the user button as *GPIO_Input* and set `B1` as label.
- Click *Generate Code* to update settings in source.

### 2. Setup software
- Copy the contents of *manual_fire.cpp* to *src/app.cpp* in your project.
- Click PlatformIO Upload.
- The user LED should flash long once every 5 seconds and flash short every second.
- If the user button is pushed, the LED flashes long immedietely, then waits 5 seconds.

### 3. Modify the code
- Change the behavior to flash immedietely on request, but do not restart the 5 second cycle.\
  Instead of:  \*1234\*1234\*12@\*1234\*123... (where \* is a long flash, @ is a button push)\
  Create this: \*1234\*1234\*12@\*34\*123...
- Remove the short flashes and implement the two distinct behaviors without counting the callbacks.
