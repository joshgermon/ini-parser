#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum TokenType {
  IDENT,
  LITERAL,
  ASSIGN,
  SEMICOLON,
  NEWLINE,
  LBRACK,
  RBRACK,
  ILLEGAL,
  FILEEND
};

struct Token {
  enum TokenType type;
  const char* literal;
};

struct Token new_token(enum TokenType type, const char* literal) {
  struct Token tok = { type, literal };
  return tok;
}

struct Lexer {
  const char* input;
  int input_len;
  int position; // Position pointing to current char
  int read_position; // Reading position in input
  char ch; // Current char
};

struct Lexer new_lexer(const char* input, int input_len) {
  struct Lexer lexer = { input, input_len, 0, 0, input[0] };
  return lexer;
}

int is_letter(char ch) {
 return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z') || ch == '_';
}

int is_digit(char ch) {
  return '0' <= ch && ch <= '9';
}


void read_char(struct Lexer* lexer) {
  // Check if at end of input
  if(lexer->read_position >= lexer->input_len) {
    lexer->ch = 0; // EOF
  } else {
    // Read the current char
    lexer->ch = lexer->input[lexer->read_position];
  }
  // Move position to last read position
  lexer->position = lexer->read_position;
  // Move our read position to the next char
  lexer->read_position += 1;
}

char* read_literal(struct Lexer* lexer) {
  int pos = lexer->position;

  // TODO: No int type for now
  while(is_letter(lexer->ch) || is_digit(lexer->ch)) {
    read_char(lexer);
  }

  // TODO: Allocation
  int substr_len = lexer->position - pos;
  char *literal = malloc(substr_len + 1);

  // Copy string literal slice from input
  memcpy(literal, lexer->input + lexer->position, substr_len);

  // NUL termination of str
  literal[substr_len] = '\0';

  printf("Literal: %s\n", literal);

  return literal;
}

void skip_whitespace(struct Lexer *l) {
  while (l->ch == ' ' || l->ch == '\t' || l->ch == '\r') {
    read_char(l);
  }
}


struct Token next_token(struct Lexer* lexer) {
  struct Token tok = { 0, 0};

  skip_whitespace(lexer);

  printf("Current char: %c\n", lexer->ch);

  switch (lexer->ch) {
    case '[':
      tok = new_token(LBRACK, "[");
      break;
    case ']':
      tok = new_token(RBRACK, "]");
      break;
    case '=':
      tok = new_token(ASSIGN, "=");
      break;
    case ';':
      tok = new_token(SEMICOLON, ";");
      break;
    case '\n':
      tok = new_token(NEWLINE, "");
      break;
    case '0':
      tok = new_token(FILEEND, "");
      break;
    default:
      if(is_letter(lexer->ch)) {
        tok.literal = read_literal(lexer);
        tok.type = LITERAL;
        return tok;
      } else {
        printf("Illegal token, %s", &lexer->ch);
        tok = new_token(ILLEGAL, &lexer->ch);
      }
  }

  read_char(lexer);
  return tok;
}


// next ...
// expect ...

// void parse_tokens(const char* input) {
//   int input_len = strlen(input);
//   struct Lexer lexer = new_lexer(input, input_len);
//
// }

void test_next_token(void) {
    const char* input = "[section]\nkey=value\n";
    struct Lexer lexer = new_lexer(input, strlen(input));

  printf("Lexer current char: %s\n", &lexer.ch);

    struct Token tests[] = {
        {LBRACK, "["},
        {LITERAL, "section"},
        {RBRACK, "]"},
        {NEWLINE, ""},
        {LITERAL, "key"},
        {ASSIGN, "="},
        {LITERAL, "value"},
        {NEWLINE, "\n"},
        {FILEEND, ""}
    };

    for (int i = 0; i < (int)sizeof(tests) / (int)sizeof(tests[0]); i++) {
        struct Token tok = next_token(&lexer);
        if (tok.type != tests[i].type) {
            fprintf(stderr, "Test %d failed: Expected type=%d, got type=%d\n", i, tests[i].type, tok.type);
            assert(0);
        }
        if (strcmp(tok.literal, tests[i].literal) != 0) {
            fprintf(stderr, "Test %d failed: Expected literal='%s', got literal='%s'\n", i, tests[i].literal, tok.literal);
            assert(0);
        }
        printf("Test %d passed: type=%d, literal=%s\n", i, tok.type, tok.literal);
    }
}

int main(void) {
    test_next_token();
    printf("All tests passed!\n");
    return 0;
}
