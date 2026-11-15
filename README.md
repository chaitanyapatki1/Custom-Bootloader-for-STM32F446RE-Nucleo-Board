STM32F446RE IAP Bootloader Documentation
Comprehensive Guide & In-Depth Technical Design Reference

_PART 1: High-Level Overview & Architecture Guide_

1. Project Overview & Architecture
This project implements an In-Application Programming (IAP) bootloader for the STM32F446RE Nucleo board (ARM Cortex-M4, 128KB RAM, 512KB Flash). Instead of a monolithic firmware file, the memory is split into two independent projects within the same workspace:
•	The Bootloader (CustomBootloaderF446RENucleo): Occupies the first 32KB of flash (0x08000000 to 0x08007FFF). It handles hardware startup checks, user button inputs, a UART command-line interface (CLI), application vector/stack validation, and the final execution jump.
•	The User Application (UserAppF446RE): Occupies the remaining flash starting from 0x08008000. It runs its own independent logic, relocated interrupt vectors, and application features.

2. Memory Mapping & Linker Configuration
•	Bootloader Linker Setup (0x08000000): The bootloader remains on the default STM32 memory layout, starting at the base flash address (FLASH: ORIGIN = 0x08000000, LENGTH = 32K; RAM: ORIGIN = 0x20000000, LENGTH = 128K).
•	User Application Linker Setup (0x08008000): To prevent the application from overwriting the bootloader, its linker script (STM32F446RETX_FLASH.ld) was shifted forward by 32KB (FLASH: ORIGIN = 0x08008000, LENGTH = 480K; RAM: ORIGIN = 0x20000000, LENGTH = 128K).

3. Core Implementation Details
•	A. Bootloader Startup & Timeout Window: Upon hardware reset, the bootloader initializes system peripherals and prints a startup message over USART2 (115200 baud). It enters a 3-second timeout loop checking the blue user button (B1 / PC13). If pressed, it enters Bootloader CLI Mode; if timeout expires, it automatically attempts to jump to the user app.
•	B. Vector Table Relocation: Because the user app executes from 0x08008000, the processor's Vector Table must be explicitly relocated at runtime so interrupts map correctly (SCB->VTOR = FLASH_BASE | 0x8000;).
•	C. Application Validation & Jump Routine (go2APP): Before jumping, the bootloader verifies that a valid stack pointer exists in RAM boundaries (0x20000000 - 0x20020000), retrieves the Reset Handler entry address, de-initializes HAL and clock peripherals (HAL_RCC_DeInit(), HAL_DeInit()), disables global interrupts (__disable_irq()), sets Main Stack Pointer (__set_MSP), and jumps.
•	D. UART Command-Line Interface (CLI): When staying in bootloader mode, an interactive loop provides responsive CLI parser support (help/?, version, jump, erase stub).

4. End-to-End Workflow Verification
•	Flashing: Bootloader compiled & programmed to 0x08000000; User Application compiled & programmed independently to 0x08008000.
•	Memory Verification: STM32CubeIDE Memory View at 0x08008000 confirms valid initial stack pointer headers (e.g. little-endian 00000220 resolving to RAM address 0x20000220).
•	Execution & Jump: Timeout expiration or typing 'jump' in CLI triggers go2APP(), cleanly passing control to the application to print '>>> Hello from the User Application! <<<'.

 _PART 2: Deep Technical Design Reference & Code Implementation_
1. System Architecture & Memory Map
The STM32F446RE microcontroller features an ARM Cortex-M4 core operating up to 180 MHz, with 512 KB internal Flash and 128 KB SRAM. Physical memory partitioning guarantees operational integrity during updates:
Region	Flash Start Address	Size	Purpose
Bootloader Sector	0x08000000	32 KB	Hardware Init, CLI, Validation, Jump Logic
User Application Sector	0x08008000	480 KB	Target Firmware Execution Space

2. Linker Script Configurations (.ld)
Spatial separation is enforced at compile-time by tailoring the MEMORY definitions within STM32F446RETX_FLASH.ld for each respective project:
Bootloader Linker Configuration (0x08000000):
MEMORY
{
  RAM    (xrw)    : ORIGIN = 0x20000000,   LENGTH = 128K
  FLASH  (rx)     : ORIGIN = 0x08000000,   LENGTH = 32K
}

User Application Linker Configuration (UserAppF446RE):
MEMORY
{
  RAM    (xrw)    : ORIGIN = 0x20000000,   LENGTH = 128K
  FLASH  (rx)     : ORIGIN = 0x08008000,   LENGTH = 480K
}

3. Core Bootloader Sequence & Implementation
A. Boot Window & Button Sampling Logic
Upon hardware reset (NRST), system clocks and peripherals (USART2, GPIO) are initialized. A 3-second non-blocking tick window monitors the blue user button (B1 / PC13):
uint32_t start_tick = HAL_GetTick();
uint8_t stay_in_bootloader = 0;

while (HAL_GetTick() - start_tick < 3000)
{
    // Nucleo button pulls pin LOW (GPIO_PIN_RESET) when pressed
    if (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET)
    {
        stay_in_bootloader = 1;
        break;
    }
}

B. Interactive UART CLI Parser
If stay_in_bootloader is asserted, control enters an infinite blocking command parsing loop utilizing UART byte reception over USART2 (115200 baud):
if (stay_in_bootloader)
{
    char *stay_msg = "-> Bootloader Mode Active (Type 'help' for commands).\r\n > ";
    HAL_UART_Transmit(&huart2, (uint8_t*)stay_msg, strlen(stay_msg), HAL_MAX_DELAY);

    char cmd_buffer[32];
    memset(cmd_buffer, 0, sizeof(cmd_buffer));
    uint8_t idx = 0;

    while (1)
    {
        uint8_t rx_byte;
        if (HAL_UART_Receive(&huart2, &rx_byte, 1, 100) == HAL_OK)
        {
            HAL_UART_Transmit(&huart2, &rx_byte, 1, HAL_MAX_DELAY); // Echo

            if (rx_byte == '\b' || rx_byte == 127) { // Backspace
                if (idx > 0) cmd_buffer[--idx] = '\0';
            }
            else if (rx_byte == '\r' || rx_byte == '\n') { // Enter
                HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);
                cmd_buffer[idx] = '\0';

                if (strcmp(cmd_buffer, "help") == 0 || strcmp(cmd_buffer, "?") == 0) {
                    char *help_text = "Available commands:\r\n"
                                      "  help     - Show this help menu\r\n"
                                      "  version  - Display bootloader version\r\n"
                                      "  jump     - Jump to user application\r\n"
                                      "  erase    - Erase application sector (stub)\r\n";
                    HAL_UART_Transmit(&huart2, (uint8_t*)help_text, strlen(help_text), HAL_MAX_DELAY);
                }
                else if (strcmp(cmd_buffer, "version") == 0) {
                    char *ver_text = "STM32F446RE Custom Bootloader v1.0\r\n";
                    HAL_UART_Transmit(&huart2, (uint8_t*)ver_text, strlen(ver_text), HAL_MAX_DELAY);
                }
                else if (strcmp(cmd_buffer, "jump") == 0) {
                    char *jump_text = "Attempting manual jump to application...\r\n";
                    HAL_UART_Transmit(&huart2, (uint8_t*)jump_text, strlen(jump_text), HAL_MAX_DELAY);
                    go2APP();
                    char *fail_text = "Jump failed: No valid application found at 0x08008000.\r\n";
                    HAL_UART_Transmit(&huart2, (uint8_t*)fail_text, strlen(fail_text), HAL_MAX_DELAY);
                }
                else if (strcmp(cmd_buffer, "erase") == 0) {
                    char *erase_text = "Flash erase routine not yet implemented.\r\n";
                    HAL_UART_Transmit(&huart2, (uint8_t*)erase_text, strlen(erase_text), HAL_MAX_DELAY);
                }
                else if (strlen(cmd_buffer) > 0) {
                    char *err_text = "Unknown command. Type 'help' for options.\r\n";
                    HAL_UART_Transmit(&huart2, (uint8_t*)err_text, strlen(err_text), HAL_MAX_DELAY);
                }

                memset(cmd_buffer, 0, sizeof(cmd_buffer));
                idx = 0;
                HAL_UART_Transmit(&huart2, (uint8_t*)" > ", 3, HAL_MAX_DELAY);
            }
            else {
                if (idx < sizeof(cmd_buffer) - 1) cmd_buffer[idx++] = rx_byte;
            }
        }
    }
}

4. Application Validation & Vector Table Architecture
A. Vector Table Relocation (SCB->VTOR)
Because the application binary starts at 0x08008000, exception vectors must be updated in Cortex-M4 System Control Block (SCB) early in main() of UserAppF446RE:
// Relocate the Vector Table to the application's flash start address
SCB->VTOR = FLASH_BASE | 0x8000; // Resolves to 0x08008000

B. Safety Validation & Jump Mechanics Routine (go2APP)
To avoid hard fault conditions, go2APP() performs double-validation before branching:
typedef void (*pFunction)(void);

void go2APP(void) {
    uint32_t app_address = 0x08008000;

    // 1. Validate Stack Pointer: Must point to a valid RAM address (0x20000000 - 0x20020000)
    uint32_t msp = *(__IO uint32_t*)app_address;
    if ((msp >= 0x20000000) && (msp <= 0x20020000)) {

        // 2. Extract Reset Handler entry address from vector table (+4 offset)
        uint32_t jump_address = *(__IO uint32_t*)(app_address + 4);
        pFunction jump_to_application = (pFunction)jump_address;

        // 3. Clean up hardware state & reset clock/peripherals
        HAL_RCC_DeInit();
        HAL_DeInit();
        __disable_irq();

        // 4. Load application's Main Stack Pointer (MSP)
        __set_MSP(msp);

        // 5. Branch/Jump to Application's Reset Handler
        jump_to_application();
    }
}

Key Operational Takeaway: De-initializing peripheral hardware (HAL_DeInit, HAL_RCC_DeInit) and calling __disable_irq() prior to resetting the Main Stack Pointer (__set_MSP) guarantees that stale interrupts or peripheral DMA requests from the bootloader do not corrupt application initialization.

5. End-to-End Verification Flow
•	1. Compilation & Memory Placement: Linker scripts enforce memory boundaries (Bootloader <32KB at 0x08000000, App at 0x08008000).
•	2. Memory Inspection: STM32CubeIDE Memory Monitor at 0x08008000 verifies valid MSP word layout (e.g. 00000220 -> 0x20000220 SRAM).
•	3. Execution Hand-off: Bootloader verifies valid RAM bounds, de-initializes hardware, sets MSP, branches to application Reset Handler, and prints application banner over UART.
