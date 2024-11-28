/*** includes ***/

#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>

/*** defines ***/

// Bitwise-ANDs a character with the value 00011111. 
// This mirrors what the Ctrl key does in the terminal
#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct editorConfig {
    int screenrows;
    int screencols;
    struct termios orig_termios;
};

struct editorConfig E;

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
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) 
        die("tcsetattr");
}

void enableRawMode() {
    // Read terminal attributes
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) 
        die("tcgetattr");
    
    // We register disableRawMode() to be called automatically when the program exits
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;
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

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    // n command queries the terminal for status information 
    // with argument of 6 it asks for the cursor position
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    //printf("\r\n&buf[1]: '%s'\r\n", &buf[1]);

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
    
    return 0;
}


// Get the size of the terminal
int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    // If ioctl() returns an error, we get the terminal size in another way
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        // \x1b[999C\x1b[999B moves the cursor to the bottom-right
        // C moves curose to the right, while B to the bottom
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
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

// Print a column of ~ on the left hand side
void editorDrawRows() {
    int y;
    for (y = 0; y < E.screenrows; y++) {
        write(STDOUT_FILENO, "~", 1);

        if (y < E.screenrows - 1) {
            write(STDOUT_FILENO, "\r\n", 2);
        }
    }
}

// Clear the screen
void editorRefreshScreen() {
    // Write escape sequence to the terminal
    write(STDOUT_FILENO, "\x1b[2J", 4);
    //J command is related with clearing the screen

    // Reposition cursor at the top-left corner
    write(STDOUT_FILENO, "\x1b[H", 3);
    // H command is related with cursor position

    editorDrawRows();

    write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** init ***/

void initEditor() {
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main() {
    enableRawMode();
    initEditor();
    
    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}