//Emulation requires a lot of different libraries, Here is where I add them to the file
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

//SDL3 Graphics Libraries. This will be for generating a Window later in the program
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

//1. Building the structure for the emulator.


//FONTSET
//Chip-8 uses a special set of fonts to actually build the Global Fonts needed for the emulator. Its a const value because these should never be changed
//In the event that the FONTSET is needed outside of a function, it is placed on its own

//Fact: Chip-8 only uses 16 characters. 0-9, and A-F. 
static const uint8_t FONTSET[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

//Chip-8 is not a true emulator, rather an Instruction Set. What that means is that it isn't emulating real hardware like you would see for a Gameboy or a NES
//Its often called the "Hello World" of Emulation because it is great for learning the basics to eventually build true emulators

// This struc itself is not the Instruction set, rather it holds the interpreter's state. 
typedef struct{
    uint8_t memory[4096]; //This sets up how much RAM we need to allocate to the Emulator. Chip-8 only requires 4096 bytes (4kB) to operate
    
    //General-purpose Register
    uint8_t V[16];        //Chip-8 uses characters 0-9, and A-F, requiring 16 for this

    uint16_t I;           //Memory Address
    //PC in this case means the Program Counter
    uint16_t pc;          //Points to current instructions. This is referred to as a "Program Counter"

    //Timers (Count Down at 60Hz) 
    uint8_t delay_timer;
    uint8_t sound_timer;  //When the timer above 0, it should make the computer "Beep". Chip-8 has very basic sounds

    // Stack and Stack Pointer
    //In Data Structures, Stack refers to "Last in, First out"
    uint16_t stack[16];   // Stores return addresses for subroutines. We need 16 bits for this (uint16_t), but the Emulator will only ever read 12 bits total
    uint8_t sp;           // Points to the top of the stack

    // Peripherals
    uint8_t keypad[16];         // Tracks the state (on/off) of the 16 keys. Like stated before, it only needs 0-F, therefor, 16 characters
    //Chip-8 only has two colours, Black and White, so I didnt need the video to be 32bit. However it does make it better for scaling up in SDL3
    uint32_t video[64 * 32];    // Chip-8 uses a very small screen size of 64x32 pixels. On modern screens this size is tiny, so we will need to scale it up once we add the graphics library
}Chip_8;

//2. Main Function
int main(int argc, char *argv[]){
    // Setting up the variables
    Chip_8 cpu; //Here I assigned the Chip8 structure to a varaible called cpu
    
    // Load into Memory. memset sets the memory, while memcpy copies the address to another in the memory
    memset(&cpu, 0, sizeof(Chip_8)); //memset is a function within <string.h> to fill a block of memory with a specified value. For us, we want the Chip_8 struc
    memcpy(cpu.memory, FONTSET, sizeof(FONTSET)); //memcpy is also from <string.h> and its for copying raw blocks of data to a source memory location. For us, that data is FONTSET
    //Setting the beginning for the Program Counter
    cpu.pc = 0x200;   //0x200 is the beginning where the Emulator will start from. 0x200 is the default for Chip-8

    // Loading the files into the Emulator
    FILE *file = fopen("CHIP-8/ROM-Files/2-ibm-logo.ch8", "rb"); 
    // fopen opens up the file when the command is called. "rb" refers to what we want to do with the file. In this case, "Read Binary" as we want the raw data of the .ch8 file. 
    // Fact: All rom files, be it .nes (NES), .sfc (SNES), and .iso (For consoles with CD games) are raw binary data, r would corrupt them
    if (file == NULL) { // NULL because the file variable is a pointer
        printf("Error: Could not open ROM file. Check your path!\n");
        return 1;
    }

    // Measure file size
    fseek(file, 0, SEEK_END); // Chip-8 has a very strict size limit of 4kB. Going above this results in a memory crash. since FONTSET is loaded into memory and the PC starts at 0x200, we only have 3584 bytes left to use
    long rom_size = ftell(file); //Not all Chip-8 ROM files are the same. the long here is to dynamically find the right size
    rewind(file); //This i so we can read the binary data of the ch8 file from the start

    // Calculate maximum available space (4096 total bytes - 512 reserved bytes)
    long max_size = 4096 - 0x200; // this only means 3584 bytes can be used
    if (rom_size > max_size) { //Condiontional if we exceed memory threshold
        printf("Error: ROM file is too large (%ld bytes). Max allowed is %ld bytes.\n", rom_size, max_size);
        fclose(file);
        return 1; // When we have an error in C, we always have to make sure it returns 1 to exit
    }

    // Read bytes directly into the RAM array starting at index 512 (0x200). 0x200 is 512 bytes. Thats the amount the emulator needs to read on statup
    fread(&cpu.memory[0x200], sizeof(uint8_t), rom_size, file);
    fclose(file);
    printf("Successfully loaded %ld bytes of ROM data into RAM.\n", rom_size); //Good Habit when starting off to have a console line to know if it was successful

    // Graphics Library:
    // Now is finally when we can load up the graphics library. This is just the setup for it. Given we are using SDL3, the implementation is far less verbose compared
    // to OpenGL, which require more set up
    // SDL3 Subsystem & Window Creation
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    bool done = false;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL Initialization Failed: %s\n", SDL_GetError());
        return 1;
    }

    // Create a 640x320 window (scaled 10x up so we can see it) along with the renderer. Like stated before, the actual screen size of Chip-8 is tiny on modern screens
    if (!SDL_CreateWindowAndRenderer("Chip-8 Emulator", 640, 320, 0, &window, &renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create window/renderer: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create a 64x32 streaming texture that maps to our uint32_t video array
    SDL_Texture* texture = SDL_CreateTexture(
        renderer, 
        SDL_PIXELFORMAT_RGBA8888, 
        SDL_TEXTUREACCESS_STREAMING, 
        64, 
        32
    );

    if (texture == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create texture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    } 

    // **Fetch, Decode, and Execute Loop ** //
       //An Emulator's main function is to run an infinite loop for three tasks in succession:
    // Fetch the Instructions from Memory at the current pc(Program Counter)
    // Decode the Instruction to figure out what needs to be done
    // Execute the instruction to do what is told of it

    // Normally for loops are good, but because we dont have a set time when the loop will end, while works better, and is easier to write down
    while(!done){
        SDL_Event event;

       // Handle window events (like clicking the close 'X' button). We arent going to close the windiw until the exit button is pressed. 
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                done = true;
            }
        }

        // FETCH:
        // Here we are going to get everything needed and ensure that the emulator is not going above its memory pool
        // Safety guard: Ensure the PC isn't running out of bounds of the 4KB memory array
        if (cpu.pc >= 4095) {
            printf("Program Counter reached end of memory. Halting emulation.\n");
            done = true; //This is where the loop will end. 
            continue;
        }

        uint16_t opcode = (cpu.memory[cpu.pc] << 8) | cpu.memory[cpu.pc + 1]; // This is the proper fetch command to get everything. 8 bitss make up one byte. We need to shift it by 8 so they dont collide instead of sitting together

        //PC needs to increment by 2 so it can fetch the next opcode. Its better to do it here than in execute as it can be prone to errors. 
        //It also means we dont have to do as much code duplication, which is good
        cpu.pc += 2;
        
        
        // DECODE:
        // --- DECODE VARIABLES ---
        uint8_t  X   = (opcode & 0x0F00) >> 8;  // The second nibble. Used to look up one of the 16 registers (VX) from V0 through VF.
        uint8_t  Y   = (opcode & 0x00F0) >> 4;  // The third nibble. Also used to look up one of the 16 registers (VY) from V0 through VF.
        uint8_t  N   = (opcode & 0x000F);       // The fourth nibble. A 4-bit number.
        uint8_t  NN  = (opcode & 0x00FF);       // The second byte (third and fourth nibbles). An 8-bit immediate number.
        uint16_t NNN = (opcode & 0x0FFF);       // The second, third and fourth nibbles. A 12-bit immediate memory address.

        //EXECUTE:

        switch (opcode & 0xF000) {
            
            case 0x0000:
                if (opcode == 0x00E0) {
                    memset(cpu.video, 0, sizeof(cpu.video));
                    printf("Executed: Clear Screen (00E0)\n");
                } 
                else if (opcode == 0x00EE) {
                    cpu.sp--;
                    cpu.pc = cpu.stack[cpu.sp];
                    printf("Executed: Return from Subroutine (00EE)\n");
                } 
                else {
                    printf("Unknown 0x0000 opcode: 0x%04X\n", opcode);
                }
                break;

            case 0x1000:
                cpu.pc = NNN - 2;
                printf("Executed: Jump to 0x%03X\n", NNN);
                break;

            case 0x6000:
                cpu.V[X] = NN;
                printf("Executed: Set V[%X] = 0x%02X\n", X, NN);
                break;

            case 0x7000:
                cpu.V[X] += NN;
                printf("Executed: Add 0x%02X to V[%X]\n", NN, X);
                break;

            case 0xA000:
                cpu.I = NNN;
                printf("Executed: Set I = 0x%03X\n", NNN);
                break;

            case 0xD000: {
                uint8_t x_pos = cpu.V[X] % 64; 
                uint8_t y_pos = cpu.V[Y] % 32; 
                cpu.V[0xF] = 0;

                for (int row = 0; row < N; row++) {
                    uint8_t sprite_byte = cpu.memory[cpu.I + row];

                    for (int col = 0; col < 8; col++) {
                        uint8_t sprite_pixel = (sprite_byte & (0x80 >> col));

                        if (sprite_pixel != 0) {
                            int screen_x = (x_pos + col) % 64;
                            int screen_y = (y_pos + row) % 32;
                            int video_index = (screen_y * 64) + screen_x;

                            if (cpu.video[video_index] == 0xFFFFFFFF) {
                                cpu.V[0xF] = 1; 
                            }
                            cpu.video[video_index] ^= 0xFFFFFFFF;
                        }
                    }
                }
                printf("Executed: Draw Sprite at (%d, %d) with height %d\n", x_pos, y_pos, N);
                break;
            }

            default:
                printf("Unknown or Unimplemented Opcode: 0x%04X\n", opcode);
                // Safe crash alternative: stop the loop when encountering an unprogrammed block
                done = true; 
                break;
        }

        // --- RENDERING TO VISUAL SCREEN ---
        SDL_UpdateTexture(texture, NULL, cpu.video, 64 * sizeof(uint32_t));
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        
        // Add a slight delay just so the console print statements don't completely lock up your machine
        SDL_Delay(1); 
    }
    



    

    // 5. Clean up allocations on Exit
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();


    return 0;
}