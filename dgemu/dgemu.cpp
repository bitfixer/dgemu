// dgemu: a digital group system emulator
// by Michael Hill 2011
// 
// dgemu emulates a digital group computer system using a simulated cassette for
// program loading and saving.
// It uses a modified version of the audio ZE rom for bootstrapping.
//
// the purpose of this emulator is to preserve the legacy of an interesting computer system
// which is underrepresented in the field of computer history.
// this is a work in progress and advice and contributions from the community are welcomed.
// Michael Hill
// bitfixer@bitfixer.com

#include <iostream>
#include <fcntl.h>   /* File control definitions */
// simple curses example; keeps drawing the inputted characters, in columns
// downward, shifting rightward when the last row is reached, and
// wrapping around when the rightmost column is reached

int infd, numread;
FILE *fp;

#ifdef _WIN32
#include "curses.h"
#include <time.h>
#include <windows.h>

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

struct timezone
{
  int  tz_minuteswest; /* minutes W of Greenwich */
  int  tz_dsttime;     /* type of dst correction */
};

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
// Define a structure to receive the current Windows filetime
  FILETIME ft;
 
// Initialize the present time to 0 and the timezone to UTC
  unsigned __int64 tmpres = 0;
  static int tzflag = 0;
 
  if (NULL != tv)
  {
    GetSystemTimeAsFileTime(&ft);
 
// The GetSystemTimeAsFileTime returns the number of 100 nanosecond 
// intervals since Jan 1, 1601 in a structure. Copy the high bits to 
// the 64 bit tmpres, shift it left by 32 then or in the low 32 bits.
    tmpres |= ft.dwHighDateTime;
    tmpres <<= 32;
    tmpres |= ft.dwLowDateTime;
 
// Convert to microseconds by dividing by 10
    tmpres /= 10;
 
// The Unix epoch starts on Jan 1 1970.  Need to subtract the difference 
// in seconds from Jan 1 1601.
    tmpres -= DELTA_EPOCH_IN_MICROSECS;
 
// Finally change microseconds to seconds and place in the seconds value. 
// The modulus picks up the microseconds.
    tv->tv_sec = (long)(tmpres / 1000000UL);
    tv->tv_usec = (long)(tmpres % 1000000UL);
  }
 
  if (NULL != tz)
  {
    if (!tzflag)
    {
      _tzset();
      tzflag++;
    }
  
// Adjust for the timezone west of Greenwich
      tz->tz_minuteswest = _timezone / 60;
    tz->tz_dsttime = _daylight;
  }
 
  return 0;
}

#else
#include <curses.h>  // required
#include <unistd.h>  /* UNIX standard function definitions */
#include <sys/time.h>
#include <termios.h> /* POSIX terminal control definitions */
#endif

#include <stdio.h>   /* Standard input/output definitions */
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <errno.h>   /* Error number definitions */
#include "z80ex.h"
#include "z80program.h"

//static Z80Context context;

Z80EX_BYTE context_mem_read_callback(Z80EX_CONTEXT *cpu, Z80EX_WORD addr, int m1_state, void *user_data);
void context_mem_write_callback(Z80EX_CONTEXT *cpu, Z80EX_WORD addr, Z80EX_BYTE value, void *user_data);
Z80EX_BYTE InZ80(Z80EX_CONTEXT *cpu, Z80EX_WORD Port, void *user_data);
void OutZ80(Z80EX_CONTEXT *cpu, Z80EX_WORD Port, Z80EX_BYTE Value, void *user_data);

Z80EX_CONTEXT *context;


#define DELETE 127
#define BACKSPACE 8

Z80EX_BYTE *RAM;
int program_index;

int r,c,  // current row and column (upper-left is (0,0))
    nrows,  // number of rows in window
    ncols;  // number of columns in window
    
unsigned char last_key_pressed;
int count;
bool last_msb;
double cpu_speed;


int num_tape_bytes_remaining;
char tape_filename[100];
char tempstring[100];
unsigned char progbuffer[64000];
unsigned char keybuffer[5];
unsigned char buffersize;
int progbytes;
FILE *tapefile;
FILE *tapebin;
bool tape_writing;
bool tape_reading;
bool message_display;

timeval message_start;
timeval last_tape_write;
timeval last_tape_read;
WINDOW *wnd;

void init_emulator()
{
/*
    context.memRead = context_mem_read_callback;
    context.memWrite = context_mem_write_callback;
    context.ioRead = InZ80;
    context.ioWrite = OutZ80;
*/
    
    context = z80ex_create(context_mem_read_callback, NULL, 
                           context_mem_write_callback, NULL, 
                           InZ80, NULL, 
                           OutZ80, NULL, 
                           NULL, NULL);
}

void open_console()
{
}

int read_console(int *buffer, int numchars)
{
    int ch;
    ch = getch();
	
	if (ch != ERR)
	{
        if (ch == 10)
        {
            ch = 13;
        }
        *buffer = ch;
        return 1;
	}
	else
	{
		return 0;
	}
}

    
void clearscr()
{
    c = 0;
    r = 0;
    clear();
    move(r,c);
}

void cursorback()
{
    c--;
    if (c < 0)
    {
        c = ncols-1;
        r--;
        if (r < 0)
        {
            r = nrows - 1;
        }
    }
    move(r,c);
}

void cursorforward()
{
    c++; // go to next column
    if (c == ncols)
    {
        c = 0;
        r++;
        if (r == nrows) r = 0;
    }
    move(r,c);
}

void draw(char dc)
{  
    move(r,c);  // curses call to move cursor to row r, column c
    addch(dc);
    refresh();  // curses call to update screen
}

void write_command_string(char *str)
{
    // move to command section
    mvaddstr(18, 0, str);
    refresh();
}

char *get_input_string()
{
    char *inputstring = tempstring;
    int inkey;
    int numread, pos;
    int x,y;
    
    numread = 0;
    pos = 0;
    
    numread = read_console(&inkey, 1);
    while (inkey != 13 && inkey != 10)
    {
        if (numread > 0)
        {
            if (inkey != BACKSPACE && inkey != DELETE)
            {
                addch(inkey);
                refresh();
                inputstring[pos] = inkey;
                inputstring[pos+1] = 0;
                pos++;
            }
            else 
            {
                if (pos > 0)
                {
                    inputstring[pos] = 0;
                    getyx(wnd, x, y);
                    fprintf(fp, "move to %d %d\n",x,y);
                    fprintf(fp, "input string is now %s\n", inputstring);
                    fflush(fp);
                    move(x, y-1);
                    delch();
                    refresh();
                    pos--;
                }
                    
            }

        }

        //numread = read(infd, &inkey, 1);
        numread = read_console(&inkey, 1);
    }
    fflush(fp);
    return inputstring;
}

void get_tape_file(bool write)
{
    char *str;
    int size;
    message_display = false;
    write_command_string("                               ");
    write_command_string("Tape File:");
    str = get_input_string();
    strcpy(tape_filename, str);
    fflush(fp);
    
    if (write == false)
    {
        tapefile = fopen(str, "rb");
        fseek(tapefile, 0L, SEEK_END);
        size = ftell(tapefile);
    
        fseek(tapefile, 0L, SEEK_SET);
        fprintf(fp, "size of file: %d\n",size);
        num_tape_bytes_remaining = size;
    }
    else 
    {
        tapefile = fopen(str, "w");
    }

    write_command_string("                                                ");
}

void convert_bit_file()
{
    int size, i;
    unsigned char line[10];
    unsigned char byte;
    // open bit file for reading
    tapefile = fopen(tape_filename, "r");
    
    // get file length
    fseek(tapefile, 0L, SEEK_END);
    size = ftell(tapefile);
    fseek(tapefile, 0L, SEEK_SET);
    
    fprintf(fp, "read %d bits.\n",size);
    progbytes = 0;
    
    fgets((char *)line, 10, tapefile);
    size--;
    
    while (size >= 8)
    {
        // skip start bit
        fgets((char *)line, 10, tapefile);
        size--;
        
        byte = 0;
        for (i = 0; i < 8; i++)
        {
            // shift byte
            byte = byte >> 1;
            
            // get bit
            fgets((char *)line, 10, tapefile);
            if (line[0] == '1')
            {
                byte = byte | 0x80;
            }
        }
            
        fprintf(fp, "got character: %d (%02x)\n",byte,byte);
        progbuffer[progbytes] = byte;
        progbytes++;
        
        size -= 8;
        
        if (size >= 1)
        {
            // skip stop bit
            fgets((char *)line, 10, tapefile);
        }
    }
    
    fclose(tapefile);
    tapefile = fopen(tape_filename, "wb");
    for (i = 0; i < progbytes; i++)
    {
        fwrite(&progbuffer[i], 1, 1, tapefile);
    }
    fclose(tapefile);
}

int main (int argc, const char * argv[]) {
    int i;
    
    unsigned char inkey;
    int readkeys[3];
    int cycles;
    timeval cpu_start;
    timeval cpu_end;
    double diff_time;
    int diff_usec;
    double cycle_time;
    int wait_usec;
    int test;
    int char_read_counter;
    
    last_key_pressed = 0;
    count = 10;
    last_msb = false;
    // set cpu to 2.5 MHz
    cpu_speed = 2500000;
    num_tape_bytes_remaining = 0;
    tape_writing = false;
    test = 0;
    buffersize = 0;
    char_read_counter = 0;
    
    RAM = (Z80EX_BYTE *)malloc(64000);
    
    for (i = 0; i < 64000; i++)
    {
        RAM[i] = 0;
    }
    
    for (i = 0; i < program_bytes; i++)
    {
        RAM[i] = program[i];
    }
    
    program_index = 0;
    
    open_console();
    
    fp = fopen("z80debug.txt","w");
    
    nrows = 16;
    ncols = 64;
    
    wnd = initscr();
    nodelay(wnd, true);
    keypad(stdscr, true);
    cbreak();
    noecho();
    clear();
    refresh();
     
    //Z80 proc;
    printf("initialize emulator\n");
    init_emulator();
    
    // reset the Z80 processor
    //Z80RESET(&context);
    z80ex_reset(context);
    
    gettimeofday(&cpu_start, NULL);
	cycles = 0;
    while (1)
    {
        gettimeofday(&cpu_end, NULL);
        
        diff_usec = cpu_end.tv_usec - cpu_start.tv_usec;
        cycle_time = (1.0 / cpu_speed) * (1 - cycles);
        wait_usec = (int) (cycle_time * 1000000) - diff_usec;
        if (wait_usec > 0 && wait_usec < 10)
        {
            // wait for appropriate delay
            usleep(wait_usec);
        }
        
        gettimeofday(&cpu_start, NULL);
        //Z80Execute(&context);
        z80ex_step(context);
        cycles++;
        
        if (char_read_counter < 10)
        {
            char_read_counter++;
        }
        else
        {
            char_read_counter = 0;
            numread = read_console(readkeys, 1);
            if (numread > 0)
            {
                if (readkeys[0] == KEY_UP)
                {
                    inkey = 11;
                }
                else if (readkeys[0] == KEY_DOWN) 
                {
                    inkey = 10;
                }
                else if (readkeys[0] == KEY_LEFT)
                {
                    inkey = 8;
                }
                else if (readkeys[0] == KEY_RIGHT)
                {
                    inkey = 12;
                }
                else 
                {
                    inkey = readkeys[0];
                }
                
                if (inkey == 16)
                {
                    if (ncols == 64)
                    {
                        ncols = 32;
                    }
                    else 
                    {
                        ncols = 64;
                    }
                    clearscr();
                    clear();
                    refresh();
                    
                }
                
                keybuffer[buffersize] = inkey;
                buffersize++;
            }
        
        }
        
        // check if tape write should stop
        if (tape_writing)
        {
            if ((cpu_start.tv_sec - last_tape_write.tv_sec) > 5)
            {
                // stop tape
                tape_writing = false;
                write_command_string("Tape write complete.");
                gettimeofday(&message_start, NULL);
                message_display = true;
                
                fclose(tapefile);
                
                // convert bit file to bytes
                convert_bit_file();
            }
        }
        else if (tape_reading)
        {
            if ((cpu_start.tv_sec - last_tape_read.tv_sec) > 1)
            {
                tape_reading = false;
                write_command_string("Tape read complete.");
                gettimeofday(&message_start, NULL);
                message_display = true;
                
                fclose(tapefile);
                num_tape_bytes_remaining = 0;
            }
        }
        else if (message_display)
        {
            if ((cpu_start.tv_sec - message_start.tv_sec) > 5)
            {
                write_command_string("                                 ");
                message_display = false;
            }
        }
    }
}

//static byte InZ80(register word Port)
//static byte InZ80(int param, ushort Port)
Z80EX_BYTE InZ80(Z80EX_CONTEXT *cpu, Z80EX_WORD Port, void *user_data)
{
    unsigned char c, ret, inkey, i;
    c = Port & 0x00ff;
    
    if (c == 1)
    {
        if (num_tape_bytes_remaining == 0 || tape_reading == false)
        {
            get_tape_file(false);
            tape_reading = true;
        }
        
        // output byte from tape file
        fread(&ret, 1, 1, tapefile);
        num_tape_bytes_remaining--;
        
        gettimeofday(&last_tape_read, NULL);
        
        if (num_tape_bytes_remaining == 0)
        {
            tape_reading = false;
            fclose(tapefile);
        }
        return ret;
    }
    else if (c == 0)
    {
        // read from keyboard
        //if (last_key_pressed != 0)
        if (buffersize > 0)
        {
            //ret = last_key_pressed | 0x80;
            if (count > 0)
            {
                //ret = last_key_pressed | 0x80;
                ret = keybuffer[0] | 0x80;
                count--;
            }
            else             
            {
                count = 10;
                //last_key_pressed = 0;
                buffersize--;
                
                for (i = 0; i < buffersize; i++)
                {
                    keybuffer[i] = keybuffer[i+1];
                }
                              
				ret = 0;
            }
        }
        else 
        {
            ret = 0;
        }

        return ret;
        
    }
    else 
    {
        //fprintf(fp, "read from port %d\n",c);
    }

}

//static byte context_mem_read_callback(int param, ushort address)
Z80EX_BYTE context_mem_read_callback(Z80EX_CONTEXT *cpu, Z80EX_WORD addr, int m1_state, void *user_data)
{
    //return memory[address];
    return RAM[addr];
}

//static void context_mem_write_callback(int param, ushort address, byte data)
void context_mem_write_callback(Z80EX_CONTEXT *cpu, Z80EX_WORD addr, Z80EX_BYTE value, void *user_data)
{
    //memory[address] = data;
    RAM[addr] = value;
}

//static void OutZ80(register word Port, register byte Value)
//static void OutZ80(int param, ushort Port, byte Value)
void OutZ80(Z80EX_CONTEXT *cpu, Z80EX_WORD Port, Z80EX_BYTE Value, void *user_data)
{
    unsigned char c,v,lsb;
    c = Port & 0x00FF;
    
    if (c == 0)
    {
        if (Value > 0)
        {
            if (Value == 127 || Value == 255)
            {
                clearscr();
            }
            else if (Value > 127) // MSB high
            {
                v = Value & 0x7f;
                
                if (v == 26 || v == 27)
                {
                    v = ' ';
                }
                
                draw(v);
                cursorforward();
                
                last_msb = true;
            }
            else 
            {
                if (Value == 1)
                {
                    if (last_msb == false)
                    {
                        // advance the cursor
                        cursorforward();
                    }
                    else 
                    {
                        cursorback();
                    }

                }
            }

        }
        else 
        {
            last_msb = false;
        }
    }
    else if (c == 1)
    {
        if (tape_writing == false)
        {
            get_tape_file(true);
            tape_writing = true;
        }
        lsb = Value & 0x01;
        fprintf(tapefile, "%d\n",lsb);
        
        gettimeofday(&last_tape_write, NULL);
    }
}

