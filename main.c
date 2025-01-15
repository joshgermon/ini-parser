#include <stdlib.h>
#include <string.h>

enum TokenType {
  IDENT,
  LITERAL,
  ASSIGN,
  SEMICOLON,
  LBRACK,
  RBRACK,
  ILLEGAL,
  EOF
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
  struct Lexer lexer = { input, input_len, 0, 0, 0 };
  return lexer;
}

int is_letter(char ch) {
 return 'a' <= ch && ch <= 'z' || 'A' <= ch && ch <= 'Z' || ch == '_';
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
  while(is_letter(lexer->ch)) {
    read_char(lexer);
  }
  // TODO: Allocation
  char *literal = malloc(1024);

  // Copy string literal slice from input
  int substr_len = pos - lexer->position;
  memcpy(literal, lexer->input + lexer->position, substr_len);

  // NUL termination of str
  literal[substr_len + 1] = '\0';

  return literal;
}

struct Token next_token(struct Lexer* lexer) {
  struct Token tok = {};

  switch (lexer->ch) {
    case '[':
      tok = new_token(LBRACK, "[");
    case ']':
      tok = new_token(RBRACK, "]");
    case '=':
      tok = new_token(ASSIGN, "=");
    case ';':
      tok = new_token(SEMICOLON, ";");
    case '0':
      tok = new_token(EOF, "");
    default:
      if(is_letter(lexer->ch)) {
        tok.literal = read_literal(lexer);
        tok.type = LITERAL;
        return tok;
      } else {
        tok = new_token(ILLEGAL, &lexer->ch);
      }
  }

  read_char(lexer);
  return tok;
}


// next ...
// expect ...

void parse_tokens(const char* input) {
  int input_len = strlen(input);
  struct Lexer lexer = new_lexer(input, input_len);

}

int main(int argv, char* argc[]) {
  char *some_input = "[section]\nkey=value\n";
  parse_tokens(some_input);
}
