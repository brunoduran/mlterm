/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <stdio.h>
#include <unistd.h>
#include <sys/select.h> /* select */
#include <sys/stat.h>
#include <string.h>  /* memset, strchr */
#include <stdlib.h>  /* atoi */
#include <termios.h> /* tcgetattr/tcsetattr */
#include "comm.h"

static char internal_buffer[512];
static char mlterm_pass[65];
#define COLORID_DEFAULT 9

static int choosed_color = 2;
static int pointed_color = 1;
static const char *color_name_table[] = {"black",   "red",  "green", "yellow", "blue",
                                         "magenta", "cyan", "white", NULL};

static const char color_name_error[] = "(error)";

static struct termios oldtio;

/*
 * communication functions
 */

static void csi(const char *str) {
  write(STDOUT_FILENO, "\033", 1);
  write(STDOUT_FILENO, str, strlen(str));
}

static void reload_passwd(void) {
  FILE *file;
  char local_buffer[256];
  size_t len;
  struct stat st;
  if (mlterm_pass[0]) return;
  if (!getenv("MLTERM")) return; /* is on remote host. */
  if (!getenv("HOME")) return;
  snprintf(local_buffer, sizeof(local_buffer), "%s%s", getenv("HOME"), "/.config/mlterm");
  if (stat(local_buffer, &st) == 0 && sizeof(local_buffer) - strlen(local_buffer) >= 11)
    strcat(local_buffer, "/challenge");
  else
    snprintf(local_buffer, sizeof(local_buffer), "%s%s", getenv("HOME"), "/.mlterm/challenge");
  file = fopen(local_buffer, "r");
  if (!file) {
    csi("]5379;gen_proto_challange\007");
    sleep(1);
    file = fopen(local_buffer, "r");
  }
  if (!file) {
    mlterm_pass[0] = 0;
    return;
  }
  len = fread(mlterm_pass, 1, sizeof(mlterm_pass) - 2, file);
  mlterm_pass[len] = ';';
  mlterm_pass[len + 1] = '\0';
  /*fprintf( stderr, "%d\n",fread(mlterm_pass, 1, 63, file) );*/
  /*fprintf( stderr, "%s\n", mlterm_pass);*/
  fclose(file);
}

static char *read_param(void) {
  char *result;

  fgets(internal_buffer, sizeof(internal_buffer) - 1, stdin);
  result = strchr(internal_buffer, '\n');
  if (!result) {
    return NULL;
  }
  *result = 0; /* terminate */
  result = strchr(internal_buffer, '=');
  if (result) { /*XXX check key and error!!*/
    if (*(result + 1))
      return result + 1;
    else
      return NULL;
  }
  return NULL;
}

static ssize_t read_stdin_timeout(void *buf, size_t nbytes) {
  fd_set fds;
  struct timeval tval;

  tval.tv_usec = 10000; /* 0.01 sec */
  tval.tv_sec = 0;

  FD_SET(STDIN_FILENO, &fds);
  if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tval) < 0 || !FD_ISSET(STDIN_FILENO, &fds))
    return -1;

  return read(STDIN_FILENO, buf, nbytes);
}

int read_one(void) {
  char buf[4] = {0};
  read(STDIN_FILENO, buf, 4);
  if (buf[0] == 0x08 || buf[0] == 0x7f) return KEY_BS;
  if (buf[0] != 0x1b) /* XXX should check non-printable */
    return buf[0];
  if (buf[1] == 0) read_stdin_timeout(buf + 1, 3);
  if (buf[1] == 0) return KEY_ESC; /* single esc */
  if (buf[1] == 0x4f) {            /*cursor key?*/
    if (buf[2] == 0) read_stdin_timeout(buf + 2, 2);
    switch (buf[2]) {
      case 0x41:
        return KEY_UP;
      case 0x42:
        return KEY_DOWN;
      case 0x43:
        return KEY_RIGHT;
      case 0x44:
        return KEY_LEFT;
      default:
        return -1;
    }
  }
  if (buf[1] == 0x5b) { /*cursor key?*/
    if (buf[2] == 0) read_stdin_timeout(buf + 2, 2);
    if (buf[2] == 0x33) {
      if (buf[3] == 0) read_stdin_timeout(buf + 2, 2);
      if (buf[3] == 0x7e) return KEY_DEL;
    }
  }

  return -1; /* couldn't processed */
}

void flush_stdout(void) { fsync(STDOUT_FILENO); }

void dec_char(void) { csi("(0"); }

void normal_char(void) { csi("(B"); }

void set_cursor_pos(window_t *window, int x, int y) {
  static char local_buffer[12]; /* [xxxx;yyyyH should enough._buffer may be
                                   already used */
  snprintf(local_buffer, sizeof(local_buffer), "[%d;%dH", window->top + y, window->left + x);
  csi(local_buffer);
}

void set_fg_color(int colorid) {
  char cmd[] = "[30m";
  if (colorid < 0) colorid = COLORID_DEFAULT;
  if (colorid > COLORID_DEFAULT) colorid = COLORID_DEFAULT;
  cmd[2] += colorid;
  csi(cmd);
}

void set_fg_color_default(void) { csi("[39m"); }

void set_bg_color(int colorid) {
  char cmd[] = "[40m";
  if (colorid < 0) colorid = COLORID_DEFAULT;
  if (colorid > COLORID_DEFAULT) colorid = COLORID_DEFAULT;
  cmd[2] += colorid;
  csi(cmd);
}

void set_altscr(void) {
  csi("7");
  csi("[?47h");
}

void unset_altscr(void) {
  csi("[?47l");
  csi("8");
}

void set_keypad(void) {
  csi("[?1h");
  csi("=");
}

void unset_keypad(void) {
  csi("[?1l");
  csi(">");
}

void cursor_show(void) { csi("[?25h"); }

void cursor_hide(void) { csi("[?25l"); }

int term_size(int *w, int *h) {
  char *p;

  if (!(p = mlterm_get_param("cols")))
    return -1;
  else
    *w = atoi(p);

  if (!(p = mlterm_get_param("rows")))
    return -1;
  else
    *h = atoi(p);

  return 0;
}

char *mlterm_get_color_param(const char *key) {
  reload_passwd();

  snprintf(internal_buffer, sizeof(internal_buffer) - 1, "]5380;%scolor:%s\007", mlterm_pass, key);
  /*fprintf(stderr, internal_buffer);*/
  csi(internal_buffer);

  return read_param();
}

char *mlterm_get_font_param(const char *file, const char *key) {
  reload_passwd();

  snprintf(internal_buffer, sizeof(internal_buffer) - 1, "]5380;%s%s:%s\007", mlterm_pass, file,
           key);
  /*fprintf(stderr, internal_buffer);*/
  csi(internal_buffer);

  return read_param();
}

char *mlterm_get_param(const char *key) {
  reload_passwd();

  snprintf(internal_buffer, sizeof(internal_buffer) - 1, "]5380;%s%s\007", mlterm_pass, key);
  /*fprintf(stderr, internal_buffer);*/
  csi(internal_buffer);

  return read_param();
}

void mlterm_set_color_param(const char *key, char *value) {
  snprintf(internal_buffer, sizeof(internal_buffer), "]5379;color:%s=%s\007", key, value);
  /*fprintf(stderr, internal_buffer);*/
  csi(internal_buffer);
}

void mlterm_set_font_param(const char *file, const char *key, char *value) {
  snprintf(internal_buffer, sizeof(internal_buffer), "]5379;%s:%s=%s\007", file, key, value);
  /*fprintf(stderr, internal_buffer);*/
  csi(internal_buffer);
}

void mlterm_set_value(const char *key, int value) {
  snprintf(internal_buffer, sizeof(internal_buffer), "]5379;%s=%d\007", key, value);
  csi(internal_buffer);
}

void mlterm_set_param(const char *key, char *value) {
  snprintf(internal_buffer, sizeof(internal_buffer), "]5379;%s=%s\007", key, value);
  csi(internal_buffer);
}

void mlterm_exec(const char *cmd) {
  snprintf(internal_buffer, sizeof(internal_buffer), "]5379;%s\007", cmd);
  csi(internal_buffer);
}

void display_colorcube(window_t *window, int x, int y, int colorid) {
  if ((colorid >= 0) && (colorid < COLORID_DEFAULT)) {
    dec_char();
    set_fg_color(colorid);
    window_addstr(window, x, y, (char *)"a"); /* XXX better char? */
    set_fg_color(COLORID_DEFAULT);            /* return to default*/
    normal_char();
  } else
    window_addstr(window, x, y, (char *)" ");
}

void display_str(window_t *window, int x, int y, const char *src, decor_t flag) {
  int len;

  len = strlen(src);
  if (len > sizeof(internal_buffer) - 3) len = sizeof(internal_buffer) - 3;
  memmove(internal_buffer + 1, src, len + 1);

  switch (flag) {
    case DC_CHOOSED:
      internal_buffer[0] = '<';
      internal_buffer[len + 1] = '>';
      set_fg_color(choosed_color);
      break;
    case DC_POINTED:
      internal_buffer[0] = '<';
      internal_buffer[len + 1] = '>';
      set_fg_color(pointed_color);
      break;
    default:
    case DC_NORMAL:
      internal_buffer[0] = ' ';
      internal_buffer[len + 1] = ' ';
      break;
  }
  internal_buffer[len + 2] = 0;
  window_addstr(window, x, y, internal_buffer);
  switch (flag) {
    case DC_CHOOSED:
    case DC_POINTED:
      set_fg_color_default();
      break;
    default:
      break;
  }
}

void display_numeric(window_t *window, int x, int y, int value, const char *unit, decor_t flag) {
  char *pos;

  snprintf(internal_buffer, sizeof(internal_buffer) - 1, " %4d:(%s)", value, unit);
  pos = strchr(internal_buffer, ':');
  switch (flag) {
    case DC_CHOOSED:
      internal_buffer[0] = '<';
      pos[0] = '>';
      set_fg_color(choosed_color);
      break;
    case DC_POINTED:
      internal_buffer[0] = '<';
      pos[0] = '>';
      set_fg_color(pointed_color);
      break;
    case DC_NORMAL:
      internal_buffer[0] = ' ';
      pos[0] = ' ';
      break;
  }
  window_addstr(window, x, y, internal_buffer);
  switch (flag) {
    case DC_CHOOSED:
    /* pass through */
    case DC_POINTED:
      set_fg_color_default();
      break;
    case DC_NORMAL:
      break;
  }
}

int colorid_from_name(char *name) {
  int i;

  for (i = 0; color_name_table[i]; i++)
    if (strcmp(name, color_name_table[i]) == 0) return i;
  return -1;
}

const char *colorname_from_id(int colorid) {
  if ((colorid >= 0) && (colorid < COLORID_DEFAULT)) return (color_name_table[colorid]);
  return color_name_error;
}

/*
 * window handling functions
 */

window_t *window_new(int left, int top, int right, int bottom, int framed, window_t *parent) {
  window_t *window;
  window = (window_t *)malloc(sizeof(window_t));
  window->framed = framed;
  window->parent = parent;
  if (parent) {
    window->top = top + parent->top;
    window->bottom = bottom + parent->top;
    window->left = left + parent->left;
    window->right = right + parent->left;
  } else {
    window->top = top;
    window->bottom = bottom;
    window->left = left;
    window->right = right;
  }
  return window;
}

void window_free(window_t *window) { free(window); }

void window_addstr(window_t *window, int x, int y, const char *str) {
  if (window->framed) {
    set_cursor_pos(window, x + 1, y + 1);
  } else {
    set_cursor_pos(window, x, y);
  }
  write(STDOUT_FILENO, str, strlen(str));
}

int window_width(window_t *window) { return window->right - window->left + 1; }

void window_clear(window_t *window) {
  int y, width;

  width = window_width(window);
  if (width > sizeof(internal_buffer) - 1) width = sizeof(internal_buffer) - 1;
  memset(internal_buffer, ' ', width - 1);
  internal_buffer[width] = 0;

  if (window->framed) {
    dec_char();
    internal_buffer[0] = 'x'; /* vertical line*/
    internal_buffer[width - 1] = 'x';
  } else
    internal_buffer[width - 1] = ' ';

  for (y = 1; y < window->bottom - window->top; y++) {
    set_cursor_pos(window, 0, y);
    write(STDOUT_FILENO, internal_buffer, strlen(internal_buffer));
  }

  if (window->framed) {
    memset(internal_buffer, 'q', width - 1); /* horiz. line*/
    internal_buffer[0] = 'l';                /* upper left*/
    internal_buffer[width - 1] = 'k';        /* upper right*/
  }
  set_cursor_pos(window, 0, 0);
  write(STDOUT_FILENO, internal_buffer, strlen(internal_buffer));
  if (window->framed) {
    internal_buffer[0] = 'm';         /* lower left*/
    internal_buffer[width - 1] = 'j'; /* lower right*/
  }
  set_cursor_pos(window, 0, window->bottom - window->top);
  write(STDOUT_FILENO, internal_buffer, strlen(internal_buffer));
  if (window->framed) normal_char();

  flush_stdout();
}

int termios_init(void) {
  struct termios newtio;
  tcgetattr(0, &oldtio);
  newtio = oldtio;
  newtio.c_lflag &= ~ICANON;
  newtio.c_lflag &= ~ECHO;
  newtio.c_cc[VMIN] = 1;
  newtio.c_cc[VTIME] = 0; /* have to break with some intervals to distinguish ESC/Right*/
  tcsetattr(0, TCSAFLUSH, &newtio);
  return 0;
}

int termios_final(void) {
  tcsetattr(0, TCSAFLUSH, &oldtio);
  return 0;
}
