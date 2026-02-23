fn u32 parse_name(PState *s) {
  parse_skip(s);
  char c = parse_peek(s);
  if (!nick_is_init(c)) {
    parse_error(s, "name", c);
  }
  u32 start = s->pos;
  while (nick_is_char(parse_peek(s))) {
    parse_advance(s);
  }
  u32 len = s->pos - start;
  u32 id  = table_find(s->src + start, len);
  parse_skip(s);
  return id;
}

// Parses a name as a 18-bit base64 number.
// Used for NAM (stuck names) and static labels that must stay numeric.
fn u32 parse_name_num(PState *s) {
  parse_skip(s);
  char c = parse_peek(s);
  if (!nick_is_init(c)) {
    parse_error(s, "name", c);
  }
  u64 k = 0;
  while (nick_is_char(parse_peek(s))) {
    c = parse_peek(s);
    k = (k << 6) | nick_letter_to_b64(c);
    if (k > EXT_MASK) {
      fprintf(stderr, "\033[1;31mPARSE_ERROR\033[0m (%s:%d:%d)\n", s->file, s->line, s->col);
      fprintf(stderr, "- base64 name '");
      print_name(stderr, k);
      fprintf(stderr, "' exceeds 18-bit limit (max 0x%05X)\n", EXT_MASK);
      exit(1);
    }
    parse_advance(s);
  }
  parse_skip(s);
  return (u32)k;
}

// Like parse_name, but returns a unique ID from the global table.
// Used for function definitions and references.
fn u32 parse_name_ref(PState *s) {
  parse_skip(s);
  char c = parse_peek(s);
  if (!nick_is_init(c)) {
    parse_error(s, "name", c);
  }
  u32 start = s->pos;
  while (nick_is_char(parse_peek(s))) {
    parse_advance(s);
  }
  u32 len = s->pos - start;
  u32 id  = table_find(s->src + start, len);
  parse_skip(s);
  return id;
}
