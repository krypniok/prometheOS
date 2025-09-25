/* Recursive descent parser for integer expressions
   which may include variables and function calls.

   Minor fixes incorporated as of 1/4/96.

 */
//#include <setjmp.h>
#include "../../cpu/jmpbuf.h"

// isalpha: Überprüft, ob ein Zeichen ein Alphabetbuchstabe ist
int isalpha(int c) {
    // Implementieren Sie den Code, um zu überprüfen, ob das Zeichen c ein Alphabetbuchstabe ist.
    // Beachten Sie, dass dies eine einfache Implementierung ist und von der
    // Umgebung abhängen kann, in der Ihr Code läuft.

    // Beispiel (vereinfachte Implementierung):
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

int txtmode(void);

#define NUM_FUNC        100
#define NUM_GLOBAL_VARS 100
#define NUM_LOCAL_VARS  200
#define ID_LEN          31
#define FUNC_CALLS      31
#define PROG_SIZE       10000
#define FOR_NEST        31

enum tok_types {DELIMITER, IDENTIFIER, NUMBER, KEYWORD, TEMP,
                STRING, BLOCK};

enum tokens {ARG, CHAR, INT, IF, ELSE, FOR, DO, WHILE, SWITCH,
             RETURN, EOL, FINISHED, END};

enum double_ops {LT=1, LE, GT, GE, EQ, NE};

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
      WHILE_EXPECTED, QUOTE_EXPECTED, NOT_TEMP,
      TOO_MANY_LVARS};

extern char *prog;  /* current location in source code */
extern char *p_buf;  /* points to start of program buffer */
extern jmp_buf e_buf; /* hold environment for longjmp() */

/* An array of these structures will hold the info
   associated with global variables.
*/
extern struct var_type {
  char var_name[32];
  int var_type;
  int value;
}  global_vars[NUM_GLOBAL_VARS];

/*  This is the function call stack. */
extern struct func_type {
  char func_name[32];
  char *loc;  /* location of function entry point in file */
} func_stack[NUM_FUNC];

/* Keyword table */
extern struct commands {
  char command[20];
  char tok;
} table[];

/* "Standard library" functions are declared here so
   they can be put into the internal function table that
   follows.
 */
int call_puts(void), print(void), getnum(void);
int call_putnl(void);

struct intern_func_type {
  char *f_name; /* function name */
  int (*p)();  /* pointer to the function */
} intern_func[] = {
  "puts", call_puts,
  "print", print,
  "getnum", getnum,
  "", 0  /* null terminate the list */
};

extern char token[80]; /* string representation of token */
extern char token_type; /* contains type of token */
extern char tok; /* internal representation of token */

extern int ret_value; /* function return value */

void eval_exp(int *value), eval_exp1(int *value);
void eval_exp2(int *value);
void eval_exp3(int *value), eval_exp4(int *value);
void eval_exp5(int *value), atom(int *value);
void eval_exp0(int *value);
void sntx_err(int error), putback(void);
void assign_var(char *var_name, int value);
int isdelim(char c), look_up(char *s), iswhite(char c);
int find_var(char *s), get_token(void);
int internal_func(char *s);
int is_var(char *s);
char *find_func(char *name);
void call(void);
// external kernel function dispatcher (generic)
static int ext_call(const char* name, int* out_value);
// kernel functions exposed to Little C
#include "../../kernel/conio.h"

/* Entry point into parser. */
void eval_exp(int *value)
{
  get_token();
  if(!*token) {
    sntx_err(NO_EXP);
    return;
  }
  if(*token==';') {
    *value = 0; /* empty expression */
    return;
  }
  eval_exp0(value);
  putback(); /* return last token read to input stream */
}

/* Process an assignment expression */
void eval_exp0(int *value)
{
  char temp[ID_LEN];  /* holds name of var receiving
                         the assignment */
  register int temp_tok;

  if(token_type==IDENTIFIER) {
    if(is_var(token)) {  /* if a var, see if assignment */
      strcpy(temp, token);
      temp_tok = token_type;
      get_token();
      if(*token=='=') {  /* is an assignment */
        get_token();
        eval_exp0(value);  /* get value to assign */
        assign_var(temp, *value);  /* assign the value */
        return;
      }
      else {  /* not an assignment */
        putback();  /* restore original token */
        strcpy(token, temp);
        token_type = temp_tok;
      }
    }
  }
  eval_exp1(value);
}

/* This array is used by eval_exp1(). Because
   some compilers cannot initialize an array within a
   function it is defined as a global variable.
*/
char relops[7] = {
  LT, LE, GT, GE, EQ, NE, 0
};

/* Process relational operators. */
void eval_exp1(int *value)
{
  int partial_value;
  register char op;

  eval_exp2(value);
  op = *token;
  if(strchr(relops, op)) {
    get_token();
    eval_exp2(&partial_value);
    switch(op) {  /* perform the relational operation */
      case LT:
        *value = *value < partial_value;
        break;
      case LE:
        *value = *value <= partial_value;
        break;
      case GT:
        *value = *value > partial_value;
        break;
      case GE:
        *value = *value >= partial_value;
        break;
      case EQ:
        *value = *value == partial_value;
        break;
      case NE:
        *value = *value != partial_value;
        break;
    }
  }
}

/*  Add or subtract two terms. */
void eval_exp2(int *value)
{
  register char  op;
  int partial_value;

  eval_exp3(value);
  while((op = *token) == '+' || op == '-') {
    get_token();
    eval_exp3(&partial_value);
    switch(op) {  /* add or subtract */
      case '-':
        *value = *value - partial_value;
        break;
      case '+':
        *value = *value + partial_value;
        break;
    }
  }
}

/* Multiply or divide two factors. */
void eval_exp3(int *value)
{
  register char  op;
  int partial_value, t;

  eval_exp4(value);
  while((op = *token) == '*' || op == '/' || op == '%') {
    get_token();
    eval_exp4(&partial_value);
    switch(op) { /* mul, div, or modulus */
      case '*':
        *value = *value * partial_value;
        break;
      case '/':
        *value = (*value) / partial_value;
        break;
      case '%':
        t = (*value) / partial_value;
        *value = *value-(t * partial_value);
        break;
    }
  }
}

/* Is a unary + or -. */
void eval_exp4(int *value)
{
  register char  op;

  op = '\0';
  if(*token=='+' || *token=='-') {
    op = *token;
    get_token();
  }
  eval_exp5(value);
  if(op)
    if(op=='-') *value = -(*value);
}

/* Process parenthesized expression. */
void eval_exp5(int *value)
{
  if((*token == '(')) {
    get_token();
    eval_exp0(value);   /* get subexpression */
    if(*token != ')') sntx_err(PAREN_EXPECTED);
    get_token();
  }
  else
    atom(value);
}

/* Find value of number, variable, or function. */
void atom(int *value)
{
  int i;

  switch(token_type) {
  case IDENTIFIER:
    /* Save identifier token in case ext_call peeks ahead and alters it */
    char saved_token[80]; char saved_token_type = token_type; char saved_tok = tok;
    strcpy(saved_token, token);
    i = internal_func(token);
    if(i != -1) {  /* call "standard library" function */
      *value = (*intern_func[i].p)();
    }
    else
    if(find_func(token)){  /* call user-defined function */
      call();
      *value = ret_value;
    }
    else if (ext_call(token, value)) {
      /* handled by external dispatcher; advance and return like regular call */
      get_token();
      return;
    }
    else {
      /* Restore identifier token before variable lookup */
      strcpy(token, saved_token); token_type = saved_token_type; tok = saved_tok;
      *value = find_var(token);  /* get var's value */
    }
    get_token();
    return;
  case NUMBER: /* is numeric constant */
    *value = atoi(token);
    get_token();
    return;
  case DELIMITER: /* see if character constant */
    if(*token=='\'') {
      *value = *prog;
      prog++;
      if(*prog!='\'') sntx_err(QUOTE_EXPECTED);
      prog++;
      get_token();
      return ;
    }
    if(*token==')') return; /* process empty expression */
    else sntx_err(SYNTAX); /* syntax error */
  default:
    sntx_err(SYNTAX); /* syntax error */
  }
}

/* Generic external dispatcher: parse args list and invoke kernel functions. */
// Supported signatures: args string of 'i' for ints; rettype: 'v' or 'i'
typedef int  (*fn_i0_t)(void);        typedef void (*fn_v0_t)(void);
typedef int  (*fn_i1_t)(int);         typedef void (*fn_v1_t)(int);
typedef int  (*fn_i2_t)(int,int);     typedef void (*fn_v2_t)(int,int);
typedef int  (*fn_i3_t)(int,int,int); typedef void (*fn_v3_t)(int,int,int);
typedef int  (*fn_i4_t)(int,int,int,int);     typedef void (*fn_v4_t)(int,int,int,int);
typedef int  (*fn_i5_t)(int,int,int,int,int); typedef void (*fn_v5_t)(int,int,int,int,int);
typedef int  (*fn_i7_t)(int,int,int,int,int,int,int); typedef void (*fn_v7_t)(int,int,int,int,int,int,int);

#include "../../kernel/bga_video.h"
#include "../../kernel/conio.h"
#include "../../drivers/keyboard.h"
// Unified time API
#include "../../kernel/time.h"

typedef struct { const char* name; const char* args; char ret; void* fn; } kentry_t;
// Small console helper to print a single character (kernel-side)
static int __k_putch(int c){ if (c==13) printf("\n"); else printf("%c", c); return c; }
static void __k_sleep_ms(int ms){ if (ms>0) sleep(ms); }
static void __k_sleep_us(int us){ if (us>0) sleep_us((uint64_t)us); }
static const kentry_t KFN[] = {
  // console/sound
  { "putch",       "i",  'i', (void*)(fn_i1_t)__k_putch },
  { "beep",        "ii", 'v', (void*)(fn_v2_t)beep },
  { "sleep",       "i",  'v', (void*)(fn_v1_t)__k_sleep_ms },
  { "sleep_us",    "i",  'v', (void*)(fn_v1_t)__k_sleep_us },
  // keyboard
  { "getkey",      "",   'i', (void*)(fn_i0_t)getkey },
  { "getkey_async","",   'i', (void*)(fn_i0_t)getkey_async },
  // BGA
  { "bga_init",    "ii", 'i', (void*)(fn_i2_t)bga_init },
  { "bga_close",   "",   'v', (void*)(fn_v0_t)bga_close },
  { "bga_clear",   "i",  'v', (void*)(fn_v1_t)bga_clear },
  { "bga_drawpixel","iii",'v', (void*)(fn_v3_t)bga_drawpixel },
  { "bga_drawline","iiiii",'v',(void*)(fn_v5_t)bga_drawline },
  { "bga_drawtri", "iiiiiii",'v',(void*)(fn_v7_t)bga_drawtri },
  { "bga_is_active","",  'i', (void*)(fn_i0_t)bga_is_active },
  { "bga_width",   "",   'i', (void*)(fn_i0_t)bga_width },
  { "bga_height",  "",   'i', (void*)(fn_i0_t)bga_height },
  // Text mode force (map to robust bga_close)
  { "txtmode",     "",   'v', (void*)(fn_v0_t)txtmode },
  { 0, 0, 0, 0 }
};

static int ext_call(const char* name, int* out_value)
{
  if (!name || !name[0]) return 0;
  // Copy function name locally because get_token() overwrites global 'token'
  char fname[64]; int i=0; while (name[i] && i < (int)sizeof(fname)-1){ fname[i]=name[i]; i++; } fname[i]='\0';
  // Must be followed by '('
  get_token();
  if (*token != '(') { putback(); return 0; }

  // Lookup in registry
  const kentry_t* e = KFN; int found = 0;
  while (e->name) { if (strcmp(e->name, fname)==0){ found=1; break; } e++; }
  if (!found) { // skip args until ')' and report unknown
    // consume until matching ')'
    int depth=1; do { get_token(); if (*token=='(') depth++; else if (*token==')') depth--; } while (depth>0 && tok!=FINISHED);
    return 0;
  }

  // Parse arguments according to signature (supports only 'i')
  int args_i[8]; int argc=0; int val=0;
  // Check empty call first
  get_token();
  if (*token != ')') {
    putback();
    do {
      eval_exp(&val);
      if (argc < (int)(sizeof(args_i)/sizeof(args_i[0]))) args_i[argc++] = val;
      get_token();
    } while (*token == ',');
    if (*token != ')') sntx_err(PAREN_EXPECTED);
  }

  // Invoke based on arg count and return type
  int n = (int)strlen(e->args);
  if (e->ret=='v'){
    switch(n){
      case 0: ((fn_v0_t)e->fn)(); break;
      case 1: ((fn_v1_t)e->fn)(args_i[0]); break;
      case 2: ((fn_v2_t)e->fn)(args_i[0],args_i[1]); break;
      case 3: ((fn_v3_t)e->fn)(args_i[0],args_i[1],args_i[2]); break;
      case 4: ((fn_v4_t)e->fn)(args_i[0],args_i[1],args_i[2],args_i[3]); break;
      case 5: ((fn_v5_t)e->fn)(args_i[0],args_i[1],args_i[2],args_i[3],args_i[4]); break;
      case 7: ((fn_v7_t)e->fn)(args_i[0],args_i[1],args_i[2],args_i[3],args_i[4],args_i[5],args_i[6]); break;
      default: sntx_err(PARAM_ERR);
    }
    if (out_value) *out_value = 0;
    return 1;
  } else { // ret == 'i'
    int r=0;
    switch(n){
      case 0: r=((fn_i0_t)e->fn)(); break;
      case 1: r=((fn_i1_t)e->fn)(args_i[0]); break;
      case 2: r=((fn_i2_t)e->fn)(args_i[0],args_i[1]); break;
      case 3: r=((fn_i3_t)e->fn)(args_i[0],args_i[1],args_i[2]); break;
      case 4: r=((fn_i4_t)e->fn)(args_i[0],args_i[1],args_i[2],args_i[3]); break;
      case 5: r=((fn_i5_t)e->fn)(args_i[0],args_i[1],args_i[2],args_i[3],args_i[4]); break;
      default: sntx_err(PARAM_ERR);
    }
    if (out_value) *out_value = r;
    return 1;
  }

  // Unsupported signature for now
  return 0;
}

/* Display an error message. */
// forward decl for debug console (isa-debugcon) when ENABLE_DEBUG
void debug_puts(const char*);
int sprintf(char*, const char*, ...);

void sntx_err(int error)
{
  char *p, *temp;
  int linecount = 0;
  register int i;

  static char *e[]= {
    "syntax error",
    "unbalanced parentheses",
    "no expression present",
    "equals sign expected",
    "not a variable",
    "parameter error",
    "semicolon expected",
    "unbalanced braces",
    "function undefined",
    "type specifier expected",
    "too many nested function calls",
    "return without call",
    "parentheses expected",
    "while expected",
    "closing quote expected",
    "not a string",
    "too many local variables"
  };
  printf("\n%s\n", e[error]);
  debug_puts("[dobby] syntax error: ");
  debug_puts(e[error]);
  debug_puts("\n");
  p = p_buf;
  while(p != prog) {  /* find line number of error */
    p++;
    if(*p == '\r' || *p == '\n') {
      linecount++;
    }
  }
  printf(" in line %d\n", linecount);
  {
    char buf[48];
    sprintf(buf, "[dobby] in line %d\n", linecount);
    debug_puts(buf);
  }

  temp = p;
  for(i=0; i<20 && p>p_buf && *p!='\n'; i++, p--);
  for(i=0; i<30 && p<=temp; i++, p++) printf("%c", *p);
  // Also mirror the snippet to the debug console
  {
    char snip[64]; int idx=0; p = temp- (i>0?i:0); if (p < p_buf) p = p_buf;
    for (idx=0; idx< (int)sizeof(snip)-2 && p<=temp; idx++, p++) snip[idx]=*p;
    snip[idx]='\n'; snip[idx+1]='\0';
    debug_puts("[dobby] snippet: "); debug_puts(snip);
  }

  longjmp(&e_buf, 1); /* return to safe point */
}

/* Get a token. */
int get_token(void)
{
  register char *temp;

  token_type = 0; tok = 0;

  temp = token;
  *temp = '\0';

 /* skip over white space */
  while(iswhite(*prog) && *prog) ++prog;

  /* handle newlines (CR, LF, or CRLF) as statement separators */
  while(*prog=='\r' || *prog=='\n') {
    if(*prog=='\r' && *(prog+1)=='\n') prog += 2; else ++prog;
    while(iswhite(*prog) && *prog) ++prog;
  }

  if(*prog=='\0') { /* end of file */
    *token = '\0';
    tok = FINISHED;
    return(token_type=DELIMITER);
  }

  if(strchr("{}", *prog)) { /* block delimiters */
    *temp = *prog;
    temp++;
    *temp = '\0';
    prog++;
    return (token_type = BLOCK);
  }

  /* look for comments */
  if(*prog=='/')
    if(*(prog+1)=='*') { /* is a comment */
      prog += 2;
      do { /* find end of comment */
        while(*prog!='*') prog++;
        prog++;
      } while (*prog!='/');
      prog++;
    }

  if(strchr("!<>=", *prog)) { /* is or might be
                              a relation operator */
    switch(*prog) {
      case '=': if(*(prog+1)=='=') {
          prog++; prog++;
          *temp = EQ;
          temp++; *temp = EQ; temp++;
          *temp = '\0';
       }
       break;
      case '!': if(*(prog+1)=='=') {
          prog++; prog++;
          *temp = NE;
          temp++; *temp = NE; temp++;
          *temp = '\0';
       }
       break;
      case '<': if(*(prog+1)=='=') {
          prog++; prog++;
          *temp = LE; temp++; *temp = LE;
       }
       else {
          prog++;
          *temp = LT;
       }
       temp++;
       *temp = '\0';
       break;
      case '>': if(*(prog+1)=='=') {
          prog++; prog++;
          *temp = GE; temp++; *temp = GE;
       }
       else {
         prog++;
         *temp = GT;
       }
       temp++;
       *temp = '\0';
       break;
    }
    if(*token) return(token_type = DELIMITER);
  }

  if(strchr("+-*^/%=;(),'", *prog)){ /* delimiter */
    *temp = *prog;
    prog++; /* advance to next position */
    temp++;
    *temp = '\0';
    return (token_type=DELIMITER);
  }

  if(*prog=='"') { /* quoted string */
    prog++;
    while(*prog!='"'&& *prog!='\r' && *prog!='\n') *temp++ = *prog++;
    if(*prog=='\r' || *prog=='\n') sntx_err(SYNTAX);
    prog++; *temp = '\0';
    return(token_type=STRING);
  }

  if(isdigit(*prog)) { /* number */
    while(!isdelim(*prog)) *temp++ = *prog++;
    *temp = '\0';
    return(token_type = NUMBER);
  }

  if(isalpha(*prog)) { /* var or command */
    while(!isdelim(*prog)) *temp++ = *prog++;
    token_type=TEMP;
  }

  *temp = '\0';

  /* see if a string is a command or a variable */
  if(token_type==TEMP) {
    tok = look_up(token); /* convert to internal rep */
    if(tok) token_type = KEYWORD; /* is a keyword */
    else token_type = IDENTIFIER;
  }
  return token_type;
}

/* Return a token to input stream. */
void putback(void)
{
  char *t;

  t = token;
  for(; *t; t++) prog--;
}

/* Look up a token's internal representation in the
   token table.
*/
int look_up(char *s)
{
  register int i;
  char *p;

/* convert to lowercase */
  p = s;
  while(*p){ *p = tolower(*p); p++; }

  /* see if token is in table */
  for(i=0; *table[i].command; i++)
      if(!strcmp(table[i].command, s)) return table[i].tok;
  return 0; /* unknown command */
}

/* Return index of internal library function or -1 if
   not found.
*/
int internal_func(char *s)
{
  int i;

  for(i=0; intern_func[i].f_name[0]; i++) {
    if(!strcmp(intern_func[i].f_name, s))  return i;
  }
  return -1;
}

/* Return true if c is a delimiter. */
int isdelim(char c)
{
  if(strchr(" !;,+-<>'/*%^=()", c) || c==9 ||
     c=='\r' || c=='\n' || c==0) return 1;
  return 0;
}

/* Return 1 if c is space or tab. */
int iswhite(char c)
{
  if(c==' ' || c=='\t') return 1;
  else return 0;
}
