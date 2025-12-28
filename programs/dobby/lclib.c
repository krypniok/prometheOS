/****** Internal Library Functions *******/

/* Add more of your own, here. */

#include <stddef.h>

#include "../../kernel/conio.h"
#include "../../kernel/bga_video.h"
#define getchar getkey

extern char *prog; /* points to current location in program */
extern char token[80]; /* holds string representation of token */
extern char token_type; /* contains type of token */
extern char tok; /* holds the internal representation of token */

enum tok_types {DELIMITER, IDENTIFIER, NUMBER, KEYWORD,
                TEMP, STRING, BLOCK};

/* These are the constants used to call sntx_err() when
   a syntax error occurs. Add more if you like.
   NOTE: SYNTAX is a generic error message used when
   nothing else seems appropriate.
*/
enum error_msg
     {SYNTAX, UNBAL_PARENS, NO_EXP, EQUALS_EXPECTED,
      NOT_VAR, PARAM_ERR, SEMI_EXPECTED,
      UNBAL_BRACES, FUNC_UNDEF, TYPE_EXPECTED,
      NEST_FUNC, RET_NOCALL, PAREN_EXPECTED,
      WHILE_EXPECTED, QUOTE_EXPECTED, NOT_STRING,
      TOO_MANY_LVARS};

int get_token(void);
void sntx_err(int error), eval_exp(int *result);
void putback(void);

/* Get a character from console. (Use getchar() if
   your compiler does not support getche().) */
int call_getche()
{
  // Parse empty arg list: getche()
  get_token(); if(*token!='(') sntx_err(PAREN_EXPECTED);
  get_token(); if(*token!=')') sntx_err(PAREN_EXPECTED);
  // Drain any buffered keys (e.g., leftover ENTER from command)
  // by polling async until empty once
  while (getkey_async() != 0) { /* drain */ }
  // Wait for next key press
  unsigned int sc = getkey();
  return (int)(sc & 0xFF);
}

/* Put a character to the display. */
int call_putch()
{
  int value;
  eval_exp(&value);
  if(value == 13) {
    printf("\n");
  } else {
    printf("%c", value);
  }
  return value;
}

/* Call puts(). */
// Build the output in a small buffer and print once to avoid per-char flushes
static void print_escaped(const char* s)
{
  // token size in the interpreter is typically <= 80 chars; 512 is safe
  char out[512];
  int  n = 0;
  while (*s && n < (int)sizeof(out)-1) {
    if (*s == '\\' && *(s+1)) {
      s++;
      char c = *s++;
      switch (c) {
        case 'n': out[n++]='\n'; break;
        case 'r': out[n++]='\r'; break;
        case 't': out[n++]='\t'; break;
        case '\\': out[n++]='\\'; break;
        case '"': out[n++]='"'; break;
        case '\'': out[n++]='\''; break;
        default: out[n++]=c; break;
      }
      continue;
    }
    out[n++] = *s++;
  }
  out[n] = '\0';
  printf("%s", out);
}

int call_puts(void)
{
  get_token();
  if(*token!='(') sntx_err(PAREN_EXPECTED);
  get_token();
  if(token_type!=STRING) sntx_err(QUOTE_EXPECTED);
  // Print with escape sequence handling (\n, \t, etc.)
  print_escaped(token);
  get_token();
  if(*token!=')') sntx_err(PAREN_EXPECTED);

  get_token();
  if(*token!=';') sntx_err(SEMI_EXPECTED);
  putback();
  return 0;
}

// --- BGA wrappers for Little C -------------------------------------------------
// bga_drawpixel(x,y,color)
int call_bga_drawpixel(void)
{
  int x=0,y=0,c=0;
  get_token(); if(*token!='(') sntx_err(PAREN_EXPECTED);
  eval_exp(&x); get_token(); if(*token!=',') sntx_err(PARAM_ERR);
  eval_exp(&y); get_token(); if(*token!=',') sntx_err(PARAM_ERR);
  eval_exp(&c); get_token(); if(*token!=')') sntx_err(PAREN_EXPECTED);
  bga_drawpixel(x,y,(uint32_t)c);
  return 0;
}
// bga_init(width, height) -> int (0 on success)
int call_bga_init(void)
{
  int w=640, h=480;
  get_token(); if(*token!='(') sntx_err(PAREN_EXPECTED);
  eval_exp(&w);
  get_token(); if(*token!=',') sntx_err(PARAM_ERR);
  eval_exp(&h);
  get_token(); if(*token!=')') sntx_err(PAREN_EXPECTED);
  return bga_init(w,h);
}

// bga_close()
int call_bga_close(void)
{
  get_token(); if(*token!='(') sntx_err(PAREN_EXPECTED);
  get_token(); if(*token!=')') sntx_err(PAREN_EXPECTED);
  bga_close();
  return 0;
}

// bga_clear(color)
int call_bga_clear(void)
{
  int c=0;
  get_token(); if(*token!='(') sntx_err(PAREN_EXPECTED);
  eval_exp(&c);
  get_token(); if(*token!=')') sntx_err(PAREN_EXPECTED);
  bga_clear((uint32_t)c);
  return 0;
}

// bga_drawline(x0,y0,x1,y1,color)
int call_bga_drawline(void)
{
  int x0=0,y0=0,x1=0,y1=0,c=0;
  get_token(); if(*token!='(') sntx_err(PAREN_EXPECTED);
  eval_exp(&x0); get_token(); if(*token!=',') sntx_err(PARAM_ERR);
  eval_exp(&y0); get_token(); if(*token!=',') sntx_err(PARAM_ERR);
  eval_exp(&x1); get_token(); if(*token!=',') sntx_err(PARAM_ERR);
  eval_exp(&y1); get_token(); if(*token!=',') sntx_err(PARAM_ERR);
  eval_exp(&c);
  get_token(); if(*token!=')') sntx_err(PAREN_EXPECTED);
  bga_drawline(x0,y0,x1,y1,(uint32_t)c);
  return 0;
}

// bga_drawtri(x0,y0,x1,y1,x2,y2,color) (filled)
int call_bga_drawtri(void)
{
  int x0=0,y0=0,x1=0,y1=0,x2=0,y2=0,c=0;
  get_token(); if(*token!='(') sntx_err(PAREN_EXPECTED);
  eval_exp(&x0); get_token(); if(*token!=',') sntx_err(PARAM_ERR);
  eval_exp(&y0); get_token(); if(*token!=',') sntx_err(PARAM_ERR);
  eval_exp(&x1); get_token(); if(*token!=',') sntx_err(PARAM_ERR);
  eval_exp(&y1); get_token(); if(*token!=',') sntx_err(PARAM_ERR);
  eval_exp(&x2); get_token(); if(*token!=',') sntx_err(PARAM_ERR);
  eval_exp(&y2); get_token(); if(*token!=',') sntx_err(PARAM_ERR);
  eval_exp(&c);
  get_token(); if(*token!=')') sntx_err(PAREN_EXPECTED);
  bga_drawtri(x0,y0,x1,y1,x2,y2,(uint32_t)c);
  return 0;
}

// bga_is_active() -> int
int call_bga_is_active(void)
{
  get_token(); if(*token!='(') sntx_err(PAREN_EXPECTED);
  get_token(); if(*token!=')') sntx_err(PAREN_EXPECTED);
  return bga_is_active();
}

// bga_width() -> int
int call_bga_width(void)
{
  get_token(); if(*token!='(') sntx_err(PAREN_EXPECTED);
  get_token(); if(*token!=')') sntx_err(PAREN_EXPECTED);
  return bga_width();
}

// bga_height() -> int
int call_bga_height(void)
{
  get_token(); if(*token!='(') sntx_err(PAREN_EXPECTED);
  get_token(); if(*token!=')') sntx_err(PAREN_EXPECTED);
  return bga_height();
}

/* Call beep(freq, ms) */
int call_beep(void)
{
  // Parse: beep( expr, expr );
  int f=440, ms=100;
  get_token(); if(*token!='(') sntx_err(PAREN_EXPECTED);
  eval_exp(&f);
  get_token(); if(*token!=',') sntx_err(PARAM_ERR);
  eval_exp(&ms);
  get_token(); if(*token!=')') sntx_err(PAREN_EXPECTED);
  get_token(); if(*token!=';') sntx_err(SEMI_EXPECTED); putback();
  beep(f, ms);
  return 0;
}

/* A built-in console output function. */
int print(void)
{
  int i;

  get_token();
  if(*token!='(')  sntx_err(PAREN_EXPECTED);

  get_token();
  if(token_type==STRING) { /* output a string */
    printf("%s ", token);
  }
  else {  /* output a number */
   putback();
   eval_exp(&i);
   printf("%d ", i);
  }

  get_token();

  if(*token!=')') sntx_err(PAREN_EXPECTED);

  get_token();
  if(*token!=';') sntx_err(SEMI_EXPECTED);
  putback();
  return 0;
}

/* Read an integer from the keyboard. */
// Out od order
int getnum(void)
{
	char s[80];
	gets(s, 80);
	//if( fgets(s, sizeof(s), stdin) ) {
		while(*prog!=')')
			prog++;
		prog++;  // advance to end of line
		return strtoul(s, NULL, 0);
//	}
	return 0;
}
