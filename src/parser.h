#ifndef __AN_PARSER_H__
#define __AN_PARSER_H__

#include "asheutils.h"
#include "cmdline.h"
#include "vec.h"
#include <stddef.h>

int parse_commandline(const byte *line, commandline_t *out);

typedef struct {
  vec_t *pipelines;   /* collection of pipelines */
  bool is_background; /* proccess is run in foreground otherwise background */
} conditional_t;

void conditional_drop(conditional_t *cond);

typedef struct {
  vec_t *args; /* Command name and arguments */
  vec_t *env;  /* Program environmental variables */
} command_t;

typedef struct {
  vec_t *commands;
  bool is_and; /* Pipeline is connected with '&&' */
} pipeline_t;

#endif
