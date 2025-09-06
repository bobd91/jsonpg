#pragma once

#include "jsonpg.h"

Parser jsonpg_parser_new(Bytes, size_t, void *);
void jsonpg_parser_free(Parser);

bool jsonpg_parser_in_object(Parser);
bool jsonpg_parser_in_array(Parser);
bool jsonpg_parser_in_any(Parser);

Byte jsonpg_parser_peek(Parser);
bool jsonpg_parser_consume(Parser, Byte);
bool jsonpg_parser_eof(Parser);

void jsonpg_consume_whitespace_only(Parser);
void jsonpg_consume_whitespace_comments(Parser);
        
void jsonpg_parse_true(Parser);
void jsonpg_parse_false(Parser);
void jsonpg_parse_null(Parser);
JsonpgType jsonpg_parse_number(Parser, *double, *long);
size_t jsonpg_parse_string(Parser, Bytes *);
size_t jsonpg_parse_sqstring(Parser, Bytes *);
size_t jsonpg_parse_nqstring(Parser, Bytes *);
void jsonpg_parse_start_object(Parser);
void jsonpg_parse_end_object(Parser);
void jsonpg_parse_start_array(Parser);
void jsonpg_parse_end_array(Parser);

void jsonpg_parse_error(Parser, unsigned);
