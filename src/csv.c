/* copied and modified from https://github.com/semitrivial/csv_parser */
/*                                                        MIT license */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "strm.h"

static int
count_fields(strm_string line, char sep)
{
  const char *ptr = strm_str_ptr(line);
  const char *pend = ptr + strm_str_len(line);
  int cnt, quoted = 0;

  for (cnt = 1; ptr<pend; ptr++) {
    if (quoted) {
      if (ptr[0] == '\"') {
        if (ptr[1] == '\"') {
          ptr++;
          continue;
        }
        quoted = 0;
      }
      continue;
    }

    switch(*ptr) {
    case '\"':
      quoted = 1;
      continue;
    case ',':
    case '\t':
      if (*ptr == sep)
        cnt++;
      continue;
    default:
      continue;
    }
  }

  if (quoted)
    return -1;

  return cnt;
}

enum csv_type {
  TYPE_UNSPC,                  /* unspecified */
  TYPE_STR,                    /* string */
  TYPE_NUM,                    /* number */
  TYPE_TIME,                   /* time */
  TYPE_ESC,                    /* escaped_string */
  TYPE_INT,                    /* integer */
  TYPE_FLOAT,                  /* float */
};

static strm_value
csv_string(const char* p, strm_int len, enum csv_type ftype)
{
  strm_string str;

  switch (ftype) {
  case TYPE_ESC:                /* escaped_string */
    {
      const char *pend = p + len;
      char *t, *s;
      int in_quote = 0;

      t = s = malloc(len+1);
      while (p<pend) {
        if (in_quote) {
          if (*p == '\"') {
            if (p[1] == '\"') {
              p++;
              *t++ = '"';
              continue;
            }
            else {
              in_quote = 0;
            }
          }
          else {
            *t++ = *p;
          }
        }
        else if (*p == '"') {
          in_quote = 1;
        }
        else {
          *t++ = *p;
        }
        p++;
      }
      str = strm_str_new(s, t - s);
      free(s);
    }
    break;
  default:
    if (isdigit((int)*p)) {
      long sec, usec;
      int offset;
      if (strm_time_parse_time(p, len, &sec, &usec, &offset) == 0) {
        return strm_time_new(sec, usec, offset);
      }
    }
    str = strm_str_new(p, len);
    break;
  }
  return strm_str_value(str);
}

static strm_value
csv_value(const char* p, strm_int len, enum csv_type ftype)
{
  const char *s = p;
  const char *send = s+len;
  long i=0;
  double f, pow = 1;
  enum csv_type type = TYPE_STR;

  switch (ftype) {
  case TYPE_UNSPC:
  case TYPE_NUM:
    /* skip preceding white spaces */
    while (isspace((int)*s)) s++;

    /* check if numbers */
    while (s<send) {
      switch (*s) {
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        if (type == TYPE_STR) type = TYPE_INT;
        i = i*10 + (*s - '0');
        pow *= 10;
        break;
      case '.':
        if (type == TYPE_FLOAT) { /* second dot */
          type = TYPE_TIME;
          break;
        }
        type = TYPE_FLOAT;
        f = i;
        i = 0;
        pow = 1;
        break;
      default:
        type = TYPE_UNSPC;
        break;
      }
      s++;
    }
  default:
    break;
  }

  switch (type) {
  case TYPE_INT:
    return strm_int_value(i);
  case TYPE_FLOAT:
    f += i / pow;
    return strm_float_value(f);
  default:
    return csv_string(p, len, ftype);
  }
  /* not reached */
}

struct csv_data {
  strm_array headers;
  enum csv_type *types;
  strm_string prev;
  char sep;
  int n;
};

enum csv_type
csv_type(strm_value v)
{
  if (strm_number_p(v)) return TYPE_NUM;
  if (strm_time_p(v)) return TYPE_TIME;
  else return TYPE_STR;
}

static int
sv_accept(strm_stream* strm, strm_value data)
{
  strm_array ary;
  strm_string line = strm_value_str(data);
  strm_value *bp;
  const char *fbeg;
  const char *ptr;
  const char *pend;
  int fieldcnt;
  int in_quote = 0, all_str = 1;
  int i = 0;
  enum csv_type ftype;
  enum csv_type* types;
  struct csv_data *cd = strm->data;
  char sep = cd->sep;

  if (cd->prev) {
    strm_int len = strm_str_len(cd->prev)+strm_str_len(line)+1;
    char* tmp = malloc(len);

    memcpy(tmp, strm_str_ptr(cd->prev), strm_str_len(cd->prev));
    *(tmp+strm_str_len(cd->prev)) = '\n';
    memcpy(tmp+strm_str_len(cd->prev)+1, strm_str_ptr(line), strm_str_len(line));
    line = strm_str_new(tmp, len);
    free(tmp);
    cd->prev = strm_str_null;
  }
  fieldcnt = count_fields(line, cd->sep);
  if (fieldcnt == -1) {
    cd->prev = line;
    return STRM_NG;
  }
  if (cd->n > 0 && fieldcnt != cd->n)
    return STRM_NG;

  ptr = strm_str_ptr(line);
  pend = ptr + strm_str_len(line);
  ary = strm_ary_new(NULL, fieldcnt);
  if (!ary) return STRM_NG;
  bp = (strm_value*)strm_ary_ptr(ary);
  types = cd->types;
  ftype = types ? types[0] : TYPE_UNSPC;

  for (fbeg=ptr; ptr<pend; ptr++) {
    if (in_quote) {
      if (*ptr == '\"') {
        if (ptr[1] == '\"') {
          ptr++;
          ftype = TYPE_ESC;
          continue;
        }
        in_quote = 0;
      }
      continue;
    }

    switch(*ptr) {
    case '\"':
      in_quote = 1;
      if (ptr == fbeg) {
        ftype = TYPE_STR;
        fbeg = ptr+1;
      }
      else {
        ftype = TYPE_ESC;
      }
      continue;
    case ',':
    case '\t':
      if (*ptr != sep) continue;
      *bp = csv_value(fbeg, ptr-fbeg, ftype);
      if (!strm_string_p(*bp)) all_str = 0;
      bp++;
      fbeg = ptr+1;
      i++;
      ftype = types ? types[i] : TYPE_UNSPC;
      break;

    default:
      continue;
    }
  }
  /* trim newline at the end */
  if (ptr[-1] == '\n') {
    ptr--;
  }
  /* trim carriage return at the end */
  if (ptr[-1] == '\r') {
    ptr--;
  }
  *bp = csv_value(fbeg, ptr-fbeg, ftype);
  if (!strm_string_p(*bp)) all_str = 0;

  /* check headers */
  if (!cd->headers && !cd->types) {
    if (all_str) {
      cd->headers = ary;
      ary = strm_ary_null;
    }
    cd->n = fieldcnt;
  }
  if (ary) {
    /* set headers if any */
    if (cd->headers)
      strm_ary_headers(ary) = cd->headers;
    if (!cd->types) {
      /* first data line (after optional header line) */
      if (cd->headers) {
        if (all_str) {          /* data line is all string; emit header line */
          strm_emit(strm, strm_ary_value(cd->headers), NULL);
          cd->headers = strm_ary_null;
        }
        else {                  /* intern header strings */
          strm_array h = cd->headers;
          strm_value *p = strm_ary_ptr(h);
          int i;

          for (i=0; i<strm_ary_len(h); i++) {
            strm_string str = strm_value_str(p[i]);

            p[i] = strm_str_value(strm_str_intern_str(str));
          }
        }
      }
      /* initialize types (determined by first data line) */
      cd->types = malloc(sizeof(enum csv_type)*fieldcnt);
      if (!cd->types) return STRM_NG;
      for (i=0; i<fieldcnt; i++) {
        cd->types[i] = csv_type(strm_ary_ptr(ary)[i]);
      }
    }
    else {
      /* type check */
      for (i=0; i<fieldcnt; i++) {
        if (cd->types[i] != csv_type(strm_ary_ptr(ary)[i])) {
          /* type mismatch (error); skip this line */
          strm_raise(strm, "csv type mismatch");
          return STRM_NG;
        }
      }
    }
    strm_emit(strm, strm_ary_value(ary), NULL);
  }
  return STRM_OK;
}

static int
sv_finish(strm_stream* strm, strm_value data)
{
  struct csv_data *cd = strm->data;

  if (cd->headers && cd->types == NULL) {
    strm_emit(strm, strm_ary_value(cd->headers), NULL);
    cd->headers = strm_ary_null;
  }
  /* memory deallocation */
  if (cd->types) {
    free(cd->types);
    cd->types = NULL;
  }
  free(cd);
  return STRM_OK;
}

static int
sv(strm_stream* strm, int argc, strm_value* args, strm_value* ret, char sep)
{
  strm_stream *t;
  struct csv_data *cd;

  strm_get_args(strm, argc, args, "");
  cd = malloc(sizeof(struct csv_data));
  if (!cd) return STRM_NG;
  cd->headers = strm_ary_null;
  cd->types = NULL;
  cd->prev = strm_str_null;
  cd->sep = sep;
  cd->n = 0;

  t = strm_stream_new(strm_filter, sv_accept, sv_finish, (void*)cd);
  *ret = strm_stream_value(t);
  return STRM_OK;
}

/* CSV: comma separated values */
static int
csv(strm_stream* strm, int argc, strm_value* args, strm_value* ret)
{
  return sv(strm, argc, args, ret, ',');
}

/* TSV: tab separated values */
static int
tsv(strm_stream* strm, int argc, strm_value* args, strm_value* ret)
{
  return sv(strm, argc, args, ret, '\t');
}

static int
ltsv_accept(strm_stream* strm, strm_value data)
{
  if (!strm_string_p(data)) {
    strm_raise(strm, "ltsv: string required");
    return STRM_NG;
  }

  const char* p = strm_str_ptr(data);
  strm_int len = strm_str_len(data);
  const char* pend = p+len;
  const char* s = p;
  int nval = 0;

  /* count number of values in line */
  while (p<pend) {
    s = memchr(p, '\t', pend-p);
    nval++;
    if (s == NULL) break;
    p = s+1;
  }

  strm_array ary = strm_ary_new(NULL, nval);
  strm_value* v = strm_ary_ptr(ary);
  strm_array hdr = strm_ary_new(NULL, nval);
  strm_value* h = strm_ary_ptr(hdr);

  p = strm_str_ptr(data);
  for (int i=0; i<nval; i++) {
    strm_string str;
    const char* rend = memchr(p, '\t', pend-p);;

    if (rend == NULL) rend = pend;
    /* retrieve label */
    s = memchr(p, ':', rend-p);
    if (s == NULL) {
      h[i] = strm_nil_value();
    }
    else {
      str = strm_str_intern(p, s-p);
      h[i] = strm_str_value(str);
      p = s+1;
    }
    /* retrieve value */
    if (p<rend) {
      v[i] = csv_value(p, rend-p, TYPE_UNSPC);
    }
    else {
      str = strm_str_new(NULL, 0);
      v[i] = strm_str_value(str);
    }
    p = rend+1;
  }

  strm_ary_headers(ary) = hdr;
  strm_emit(strm, strm_ary_value(ary), NULL);
  return STRM_OK;
}

/* LTSV: labeled tab separated values (see ltsv.org) */
static int
ltsv(strm_stream* strm, int argc, strm_value* args, strm_value* ret)
{
  strm_stream *t;

  strm_get_args(strm, argc, args, "");

  t = strm_stream_new(strm_filter, ltsv_accept, NULL, NULL);
  *ret = strm_stream_value(t);
  return STRM_OK;
}

static int
str_number(strm_stream* strm, int argc, strm_value* args, strm_value* ret)
{
  strm_value s;

  strm_get_args(strm, argc, args, "S", &s);
  s = csv_value(strm_str_ptr(s), strm_str_len(s), TYPE_NUM);
  if (!strm_number_p(s)) {
    strm_raise(strm, "invalid string for number");
    return STRM_NG;
  }
  *ret = s;
  return STRM_OK;
}

void
strm_csv_init(strm_state* state)
{
  strm_var_def(state, "csv", strm_cfunc_value(csv));
  strm_var_def(state, "tsv", strm_cfunc_value(tsv));
  strm_var_def(state, "ltsv", strm_cfunc_value(ltsv));

  strm_var_def(strm_ns_string, "number", strm_cfunc_value(str_number));
}
