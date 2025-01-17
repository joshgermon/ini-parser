#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_ALLOC_SIZE 4096

/*
* Allocator
* Most basic possible linear allocator to store data while parsing the input
*/
struct Allocator {
  uint8_t* buffer; // points to start of memory block
  size_t cap; // capacity of allocator
  size_t offset; // current index into memory block
};

void allocator_init(struct Allocator *allocator) {
  allocator->buffer = malloc(INITIAL_ALLOC_SIZE);
  allocator->cap = INITIAL_ALLOC_SIZE;
  allocator->offset = 0;
}

void* allocator_alloc(struct Allocator *allocator, size_t size) {
  // Align the current offset to the next multiple of the alignment
  size_t alignment = 8;
  size_t aligned_offset = (allocator->offset + (alignment - 1)) & ~(alignment - 1);

  // Check for capacity
  if (allocator->offset + size > allocator->cap) {
    return NULL; // failed to alloc
  }
  // Start of memory free
  void *ptr = allocator->buffer + aligned_offset;
  // Offset moves to size
  allocator->offset = aligned_offset + size;
  return ptr;
}

// All memory is void and free to be overriden
void allocator_reset(struct Allocator *allocator) {
  allocator->offset = 0;
}

void allocator_free(struct Allocator *allocator) {
  free(allocator->buffer);
  allocator->buffer = NULL;
  allocator->cap = 0;
  allocator->offset = 0;
}

/*
* INI Parser
*/
struct IniEntry {
  const char *key;
  const char *val;
  const char *section;
};

struct Parser {
  // Parser state
  const char *input;
  int input_len;
  int position;      // Position pointing to current char
  int read_position; // Reading position in input
  char ch;           // Current char
  char* section_name; // Current section name

  // Parsed data
  int entry_len;
  struct IniEntry *entries;
};

struct IniEntry new_ini_entry(const char *key, const char *val, char* section) {
  struct IniEntry entry = {
    .key = key,
    .val = val,
    .section = section
  };
  return entry;
}

struct Parser new_parser(const char *input, int input_len, struct Allocator *allocator) {
  struct IniEntry *entries = allocator_alloc(allocator, 1024);
  struct Parser parser = {input, input_len, 0, 1, input[0], 0, 0, entries };
  return parser;
}

int is_digit(char ch) { return '0' <= ch && ch <= '9'; }
int is_valid_char(char ch) {
  return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z') || ch == '_' || is_digit(ch);
}


void read_char(struct Parser *parser) {
  // Check if at end of input
  if (parser->read_position >= parser->input_len) {
    parser->ch = '0'; // EOF
  } else {
    // Read the current char
    parser->ch = parser->input[parser->read_position];
  }
  // Move position to last read position
  parser->position = parser->read_position;
  // Move our read position to the next char
  parser->read_position += 1;
}

char *read_literal(struct Parser *parser, struct Allocator *allocator) {
  int pos = parser->position;

  // TODO: No int type for now
  while (is_valid_char(parser->ch)) {
    read_char(parser);
  }

  int substr_len = parser->position - pos;
  char *literal = allocator_alloc(allocator, substr_len + 1);

  strncpy(literal, parser->input + pos, substr_len);
  literal[substr_len] = '\0';

  return literal;
}

void skip_to_next_line(struct Parser *parser) {
  while (parser->ch !='\n') {
    if (parser->ch == '0') exit(EXIT_FAILURE);
    read_char(parser);
  }
}

void skip_whitespace(struct Parser *parser) {
  while (parser->ch == ' ' || parser->ch == '\t' || parser->ch == '\r' || parser->ch == '\n') {
    read_char(parser);
  }
}

void parse_section_name(struct Parser *parser, struct Allocator *allocator) {
  // Lexer is at LBRACK move to next char, and read literal
  read_char(parser);
  assert(is_valid_char(parser->ch));

  parser->section_name = read_literal(parser, allocator);

  assert(parser->ch == ']');
  // Move past closing bracket
  read_char(parser);
}

void parse_key_value(struct Parser *parser, struct Allocator *allocator) {
  const char *key = read_literal(parser, allocator);
  skip_whitespace(parser);
  // Move past assignment oper
  assert(parser->ch == '=');
  read_char(parser);
  skip_whitespace(parser);

  const char *val = read_literal(parser, allocator);

  struct IniEntry entry = new_ini_entry(key, val, parser->section_name);

  parser->entries[parser->entry_len] = entry;
  parser->entry_len += 1;
}

int parse_next(struct Parser *parser, struct Allocator *allocator) {
  skip_whitespace(parser);

  switch (parser->ch) {
  case '[':
    parse_section_name(parser, allocator);
    break;
  case ';':
    // Consume semi colon and skip to next non-whitespace/new line
    skip_to_next_line(parser);
    break;
  case '0':
    return 0; // No next values to parse
    break;
  default:
    if (is_valid_char(parser->ch)) {
        parse_key_value(parser, allocator);
    } else {
      printf("Illegal token");
      exit(EXIT_FAILURE);
    }
  }

  read_char(parser);
  return 1;
}

char *read_file(const char* path, struct Allocator* allocator) {
  FILE *file = fopen(path, "rb");
  if(file == NULL) {
    perror("File is null\n");
    exit(1);
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    perror("Error seeking to end of file");
    fclose(file);
    return NULL;
  }

  long filesize = ftell(file);
  if(filesize == -1) {
    perror("Error getting filesize");
    fclose(file);
    return NULL;
  }

  rewind(file);


  char *buf = allocator_alloc(allocator, filesize + 1);
  if(!buf) {
    perror("No size left in arena");
    fclose(file);
    return NULL;
  }

  size_t bytes_read = fread(buf, 1, filesize, file);
  if(bytes_read <= 0) {
    printf("Error: did not read bytes");
    exit(1);
  }

  buf[filesize] = '\0';
  fclose(file);

  return buf;
}

int main(int argc, char *argv[]) {
  if(argc != 2) {
    printf("Usage: inip <path to ini file>\n");
    exit(1);
  }

  const char *file_path = argv[1];

  struct Allocator allocator = { 0, 0, 0 };
  allocator_init(&allocator);
  const char *ini_input = read_file(file_path, &allocator);

  struct Parser parser = new_parser(ini_input, strlen(ini_input), &allocator);

  while(parse_next(&parser, &allocator)) { }

  for(int i = 0; i < parser.entry_len; i++) {
    struct IniEntry *ent = &parser.entries[i];
    printf("Key: %s, Value: %s, Section: %s\n", ent->key, ent->val, ent->section);
  }

  return 0;
}
