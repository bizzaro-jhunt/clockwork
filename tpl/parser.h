#ifndef _TPL_PARSER_H
#define _TPL_PARSER_H

/* Parse contents of a file and return a template object
   that can be used to render the contents */
struct template* parse_template(const char *path);

#endif
