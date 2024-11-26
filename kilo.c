/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

// Bitwise-ANDs a character with the value 00011111. 
// This mirrors what the Ctrl key does in the terminal
#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct termios orig_termios;

/*** terminal ***/

void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    // pererror() looks at the global errno variable and prints an error message
    perror(s);
    exit(1);
}

void disableRawMode() {
    // Restore terminal's original attributes
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) 
        die("tcsetattr");
}

void enableRawMode() {
    // Read terminal attributes
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) 
        die("tcgetattr");
    
    // We register disableRawMode() to be called automatically when the program exits
    atexit(disableRawMode);

    struct termios raw = orig_termios;
    // Input flags
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    // BRKINT when turned on, a break condition will cause a SIGINT signal to be sent to the program, 
    // like pressing Ctrl-C
    // INPCK enables parity checking
    // ISTRIP causes the 8th bit of each input byte to be stripped (set to 0)
    // ICRNL is related to Ctrl-M (carriage return)
    // IXON is related to Ctrl-S (stops data from being transmitted to the terminal) and Ctrl-Q (resume)

    // Output flags
    raw.c_oflag &= ~(OPOST);
    // OPOST is related to the output traslation of '\n' to '\r\n'

    //Control flags
    raw.c_cflag |= (CS8);
    // CS8 sets the character size (CS) to 8 bits per byte

    // Local flags 
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN); // Turned off
    // ECHO feature causes each key you type to be printed to the terminal
    // ICANON stands for canonical mode
    // ISIG is related to Ctrl-C (terminate process) and Ctrl-Z (stop process)
    // IEXTEN is related to Ctrl-V (lets the terminal waits for you to type another character and then 
    // sends that character literally)

    // Control characters
    raw.c_cc[VMIN] = 0;
    // VMIN sets the minimun number of bytes of the input needed before read() can return
    raw.c_cc[VTIME] = 1;
    // VTIME sets the maximum amount of time to wait before read() returns

    // Apply updates of terminal attributes
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

// Wait for one keypress and return it
char editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) 
            die("read");
    }
    return c;
}

/*** input ***/

// Wait for a keypress and handles it
void editorProcessKeypress() {
    char c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
    }
}

/*** output ***/

// Clear the screen
void editorRefreshScreen() {
    // Write escape sequence to the terminal
    write(STDOUT_FILENO, "\x1b[2J", 4);
    //J command is related with clearing the screen

    // Reposition cursor at the top-left corner
    write(STDOUT_FILENO, "x\1b[H", 3);
    // H command is related with cursor position
}

/*** init ***/

int main() {
    enableRawMode();
    
    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}