#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * ------------------------------------
 * Allocator
 * ------------------------------------
 * Most basic possible linear allocator to store data while parsing the input
 * ------------------------------------
 */

#define INITIAL_ALLOC_SIZE 2048

typedef struct {
  uint8_t *buffer; // points to start of memory block
  size_t cap;      // capacity of allocator
  size_t offset;   // current index into memory block
} LinearAllocator;

void allocator_init(LinearAllocator *allocator) {
  allocator->buffer = malloc(INITIAL_ALLOC_SIZE);
  allocator->cap = INITIAL_ALLOC_SIZE;
  allocator->offset = 0;
}

void *allocator_alloc(LinearAllocator *allocator, size_t size) {
  // Align the current offset to the next multiple of the alignment
  size_t alignment = 8;
  size_t aligned_offset =
      (allocator->offset + (alignment - 1)) & ~(alignment - 1);

  // Check for capacity
  if (aligned_offset + size > allocator->cap) {
    return NULL; // failed to alloaligned_offset
  }
  // Start of memory free
  void *ptr = allocator->buffer + aligned_offset;
  memset(ptr, 0, size); // Zero out memory
  // Offset moves to size
  allocator->offset = aligned_offset + size;
  return ptr;
}

// All memory is void and free to be overriden
void allocator_reset(LinearAllocator *allocator) { allocator->offset = 0; }

void allocator_free(LinearAllocator *allocator) {
  free(allocator->buffer);
  allocator->buffer = NULL;
  allocator->cap = 0;
  allocator->offset = 0;
}

/*
 * ------------------------------------
 * Hash Table
 * ------------------------------------
 * Simple hash table to store ini entries
 * ------------------------------------
 */

#define MAX_TABLE_SIZE 64
#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

typedef struct {
  const char *key;
  void *val;
} HTEntry;

typedef struct {
  HTEntry *entries;
  size_t len;
  size_t cap;
} SHashTable;

SHashTable *shasht_init(LinearAllocator *allocator) {
  SHashTable *table = allocator_alloc(allocator, sizeof(SHashTable));
  if (table == NULL)
    exit(1);

  table->cap = MAX_TABLE_SIZE;
  table->len = 0;
  table->entries = allocator_alloc(allocator, table->cap * sizeof(HTEntry));
  if (table->entries == NULL)
    exit(1);

  return table;
}

void shasht_destroy(SHashTable *table) {
  for (size_t i = 0; i < table->cap; i++) {
    free((void *)table->entries[i].key);
  }

  free(table->entries);
  free(table);
}

// FNV-1a hash function
// https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
static uint64_t hash_key(const char *key) {
  uint64_t hash = FNV_OFFSET;
  for (const char *p = key; *p; p++) {
    hash ^= (uint64_t)(unsigned char)(*p);
    hash *= FNV_PRIME;
  }

  return hash;
}

char *str_dup(const char *c, LinearAllocator *allocator) {
  char *dup = allocator_alloc(allocator, strlen(c) + 1);
  if (!dup) {
    fprintf(stderr, "Failed to allocate memory for string duplication\n");
    exit(EXIT_FAILURE);
  }
  strcpy(dup, c);
  return dup;
}
static const char *shasht_set(SHashTable *table, const char *key, void *value,
                              LinearAllocator *allocator) {
  assert(value != NULL);

  if (table->len >= table->cap / 2) {
    fprintf(stderr, "Need to implement resizing as Hash table has filled up\n");
    exit(EXIT_FAILURE);
  }

  uint64_t hash = hash_key(key);
  // Normalise hash to capacity of table
  size_t index = (hash & (table->cap - 1));


  // Look for the key in the array, loop until
  // we find an empty slot, in which case - we
  // could not find the key requested.
  while (table->entries[index].key != NULL) {
    if (strcmp(key, table->entries[index].key) == 0) {
      // TODO: Found existing key, update value
      table->entries[index].val = value;
      return table->entries[index].key;
    }

    index++;

    // Wrap around array
    if (index >= table->cap) {
      index = 0;
    }
  }
  // Insert new key value pair
  key = str_dup(key, allocator);
  table->len += 1;

  table->entries[index].key = key;
  table->entries[index].val = value;

  return key;
}

void *shasht_get(SHashTable *table, char *key) {
  uint64_t hash = hash_key(key);
  // Normalise hash to capacity of table
  size_t index = (hash & (table->cap - 1));

  // Look for the key in the array, loop until
  // we find an empty slot, in which case - we
  // could not find the key requested.
  while (table->entries[index].key != NULL) {
    if (strcmp(key, table->entries->key) == 0) {
      return table->entries[index].val;
    }

    index++;

    // Wrap around array
    if (index >= table->cap) {
      index = 0;
    }
  }

  return NULL;
}

size_t shasht_len(SHashTable *table) { return table->len; }

void shasht_print_debug(SHashTable *table) {
  printf("=== Hash Table Debug Info ===\n");
  printf("Capacity: %zu\n", table->cap);
  printf("Entries: %zu\n", table->len);
  printf("Load Factor: %.2f\n", (float)table->len / table->cap);
  printf("=============================\n");

  printf("Filled Slots:\n");
  for (size_t i = 0; i < table->cap; i++) {
    HTEntry *entry = &table->entries[i];
    if (entry->key != NULL) { // Check if the slot is filled
      printf("Slot %zu:\n", i);
      printf("\tKey: %s\n", entry->key);
      printf("\tValue: %s\n",
             (char *)entry->val); // Assuming values are strings for debug
    }
  }
  printf("=============================\n");
}
// TODO: Delete from hash table function
// int shasht_delete(SHashTable *table) { }

/*
 * ------------------------------------
 * INI IniParser
 * ------------------------------------
 * Reading text file and parsing sections, keys &
 * values while ignoring comments
 * ------------------------------------
 */
typedef struct {
  const char *key;
  const char *val;
  const char *section;
} IniEntry;

typedef struct {
  // IniParser state
  const char *input;
  int input_len;
  int position;       // Position pointing to current char
  int read_position;  // Reading position in input
  char ch;            // Current char
  char *section_name; // Current section name
} IniParser;

IniEntry new_ini_entry(const char *key, const char *val, char *section) {
  IniEntry entry = {.key = key, .val = val, .section = section};
  return entry;
}

IniParser new_parser(const char *input, int input_len) {
  IniParser parser = {input, input_len, 0, 1, input[0], 0};
  return parser;
}

int is_digit(char ch) { return '0' <= ch && ch <= '9'; }
int is_valid_char(char ch) {
  return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z') || ch == '_' ||
         is_digit(ch);
}

void read_char(IniParser *parser) {
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

char *read_literal(IniParser *parser, LinearAllocator *allocator) {
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

void skip_to_next_line(IniParser *parser) {
  while (parser->ch != '\n') {
    if (parser->ch == '0')
      exit(EXIT_FAILURE);
    read_char(parser);
  }
}

void skip_whitespace(IniParser *parser) {
  while (parser->ch == ' ' || parser->ch == '\t' || parser->ch == '\r' ||
         parser->ch == '\n') {
    read_char(parser);
  }
}

void parse_section_name(IniParser *parser, LinearAllocator *allocator) {
  // Lexer is at LBRACK move to next char, and read literal
  read_char(parser);
  assert(is_valid_char(parser->ch));

  parser->section_name = read_literal(parser, allocator);

  assert(parser->ch == ']');
  // Move past closing bracket
  read_char(parser);
}

void parse_key_value(IniParser *parser, SHashTable *table,
                     LinearAllocator *allocator) {
  const char *key = read_literal(parser, allocator);
  skip_whitespace(parser);
  // Move past assignment oper
  assert(parser->ch == '=');
  read_char(parser);
  skip_whitespace(parser);

  const char *val = read_literal(parser, allocator);
  shasht_set(table, key, (void *)val, allocator);
}

int parse_next(IniParser *parser, SHashTable *table, LinearAllocator *allocator) {
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
      parse_key_value(parser, table, allocator);
    } else {
      printf("Illegal token");
      exit(EXIT_FAILURE);
    }
  }

  read_char(parser);
  return 1;
}

SHashTable *parse_ini(IniParser *parser, LinearAllocator *allocator) {
  SHashTable *ini_table = shasht_init(allocator);
  while (parse_next(parser, ini_table, allocator)) { }
  return ini_table;
}

char *read_file(const char *path, LinearAllocator *allocator) {
  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    perror("File is null\n");
    exit(1);
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    perror("Error seeking to end of file");
    fclose(file);
    return NULL;
  }

  long filesize = ftell(file);
  if (filesize == -1) {
    perror("Error getting filesize");
    fclose(file);
    return NULL;
  }

  rewind(file);

  char *buf = allocator_alloc(allocator, filesize + 1);
  if (!buf) {
    perror("No size left in allocator");
    fclose(file);
    return NULL;
  }

  size_t bytes_read = fread(buf, 1, filesize, file);
  if (bytes_read <= 0) {
    printf("Error: did not read bytes");
    exit(1);
  }

  buf[filesize] = '\0';
  fclose(file);

  return buf;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: ini_parser <path to ini file>\n");
    exit(EXIT_FAILURE);
  }

  const char *file_path = argv[1];

  LinearAllocator allocator = {0, 0, 0};
  allocator_init(&allocator);

  const char *ini_input = read_file(file_path, &allocator);
  IniParser parser = new_parser(ini_input, strlen(ini_input));

  SHashTable *ini_data = parse_ini(&parser, &allocator);
  shasht_print_debug(ini_data);

  allocator_free(&allocator);

  return 0;
}
