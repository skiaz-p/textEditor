
// Current progess : Table 3 -> Global State
// https://viewsourcecode.org/snaptoken/kilo/03.rawInputAndOutput.html



/*** includes ***/
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>

/*** Defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)

/*** Data ***/
struct editorConfig{
  int screenRows;
  int screenCols;
  struct termios native_termios;
};
struct editorConfig E;


/*** Terminal ***/

void die(const char *s) {
  perror(s);
  exit(1);
}


void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.native_termios) == -1)
    die("tcsetattr");
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.native_termios) == -1) die("tcgetattr");
  atexit(disableRawMode);

  struct termios raw = E.native_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

char editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  return c;
}
/*** input ***/
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

void editorDrawRows() {
  int y;
  for (y = 0; y < E.screenRows; y++) {
    write(STDOUT_FILENO, "~", 1);

    if(y < E.screenRows -1);{
      write(STDOUT_FILENO, "\r\n", 2);
    }
  }
}


void editorRefreshScreen() {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  editorDrawRows();
  write(STDOUT_FILENO, "\x1b[H", 3);
}

int getCursorPosition(int *rows, int *cols){
  char buf[32];
  unsigned int i = 0;
  if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

  while (i<sizeof(buf) - 1){
    if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if(buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';
  if(buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d%d", rows, cols) != 2) return -1;
  return 0;
}

int getWindowSize(int *rows, int *cols){
  struct winsize ws;
  if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1; // Other way of getting the window size
    return getCursorPosition(rows,cols);
  }else{
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}


/*** Append Buffer ***/

struct buffer
{
  char* b;
  int size;
};
#define buffer_INIT {NULL, 0}

void BuffAppend(struct buffer *ab, const char *s, int size){
  char *new = realloc(ab->b, ab->size + size);

  if (new == NULL ) return;
  memcpy(&new[ab->size], s, size);
  ab->b = new;
  ab->size += size;
}

void abFree(struct buffer *ab){
    free(ab->b);
}

/*** output ***/

void BuffeditorDrawRows(struct buffer *ab){
    int y;
    for(y=0; y < E.screenRows; y++){
      BuffAppend(ab, "~", 1);

      if(y < E.screenRows - 1){
        BuffAppend(ab, "\r\n" ,2);
      }
    }
}

void BuffeditorRefreshScreen(){
  struct buffer ab= buffer_INIT;
  BuffAppend(&ab, "\x1b[2J", 4);
  BuffAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);

  BuffAppend(&ab, "\x1b[H", 3);

  write(STDOUT_FILENO, ab.b, ab.size);
  abFree(&ab);
}
/*** Init ***/

void InitEditor(){
  if(getWindowSize(&E.screenRows, &E.screenCols) == -1) die("getWindowSize");
}

int main(){
  enableRawMode();
  InitEditor();
    while (1) {
      editorRefreshScreen();
      editorProcessKeypress();
    }
  return 0;
}


