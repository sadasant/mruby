#include <mruby.h>
#include <mruby/class.h>
#include <mruby/array.h>
#include <mruby/string.h>
#include <mruby/data.h>
#include <mruby/variable.h>

#include <stdio.h>
#include <string.h>
#include <pcre.h>

typedef struct mrb_regexp_pcre {
  pcre *re;
  int flags;
} mrb_regexp_pcre;

void
mrb_regexp_free(mrb_state *mrb, void *ptr)
{
  mrb_regexp_pcre *mrb_re = ptr;

  pcre_free(mrb_re->re);
  mrb_free(mrb, mrb_re);
}
static struct mrb_data_type mrb_regexp_type = { "Regexp", mrb_regexp_free };

static int
mrb_pcre_convert_flags(mrb_value flags)
{
  int cflags = PCRE_DOTALL;

  if (mrb_nil_p(flags)) {
    cflags = 0;
  } else if (mrb_fixnum_p(flags)) {
    int nflag = mrb_fixnum(flags);
    if (nflag & 1) cflags |= PCRE_CASELESS;
    if (nflag & 2) cflags |= PCRE_EXTENDED;
    if (nflag & 4) cflags |= PCRE_MULTILINE;
  } else if (mrb_string_p(flags)) {
      if (strchr(RSTRING_PTR(flags), 'i')) cflags |= PCRE_CASELESS;
      if (strchr(RSTRING_PTR(flags), 'x')) cflags |= PCRE_EXTENDED;
      if (strchr(RSTRING_PTR(flags), 'm')) cflags |= PCRE_MULTILINE;
  } else if (mrb_type(flags) == MRB_TT_TRUE) {
    cflags |= PCRE_CASELESS;
  }

  return cflags;
}

mrb_value
regexp_pcre_initialize(mrb_state *mrb, mrb_value self)
{
  int erroff = 0;
  const char *errstr = NULL;
  mrb_regexp_pcre *reg = NULL;
  mrb_value source, flag = mrb_nil_value();

  mrb_get_args(mrb, "S|o", &source, &flag);

  reg = mrb_malloc(mrb, sizeof(mrb_regexp_pcre));
  memset(reg, 0, sizeof(mrb_regexp_pcre));
  DATA_PTR(self) = reg;
  DATA_TYPE(self) = &mrb_regexp_type;

  reg->flags = mrb_pcre_convert_flags(flag);

  reg->re = pcre_compile(RSTRING_PTR(source), reg->flags, &errstr, &erroff, NULL);
  if (reg->re == NULL) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "invalid regular expression");
  }

  mrb_iv_set(mrb, self, mrb_intern(mrb, "@source"), source);
  mrb_iv_set(mrb, self, mrb_intern(mrb, "@options"), mrb_fixnum_value(reg->flags));

  return self;
}

#define OVECCOUNT 30
mrb_value
regexp_pcre_match(mrb_state *mrb, mrb_value self)
{
  int i, nmatch = OVECCOUNT, match[OVECCOUNT], rc;
  struct RClass *m;
  mrb_value md, str;
  mrb_int pos;
  mrb_regexp_pcre *reg;
  mrb_value args[2];

  reg = (mrb_regexp_pcre *)mrb_get_datatype(mrb, self, &mrb_regexp_type);
  if (!reg)
    return mrb_nil_value();

  mrb_get_args(mrb, "S|i", &str, &pos);

  m = mrb_class_get(mrb, "MatchData");
  md = mrb_class_new_instance(mrb, 0, NULL, m);

  rc = pcre_exec(reg->re, NULL, RSTRING_PTR(str), strlen(RSTRING_PTR(str)), 0, 0, match, nmatch);
  if (rc < 0) {
    return mrb_nil_value();
  }

  for (i = 0; i < rc; i++) {
    args[0] = mrb_fixnum_value(match[2 * i]);
    args[1] = mrb_fixnum_value(match[2 * i + 1]);
    mrb_funcall_argv(mrb, md, mrb_intern(mrb, "push"), sizeof(args) / sizeof(args[0]), &args[0]);
  }
  mrb_iv_set(mrb, md, mrb_intern(mrb, "@length"), mrb_fixnum_value(rc));
  mrb_iv_set(mrb, md, mrb_intern(mrb, "@string"), mrb_str_dup(mrb, str));

  return md;
}

void
mrb_mruby_regexp_pcre_gem_init(mrb_state *mrb)
{
  struct RClass *re;

  re = mrb_define_class(mrb, "Regexp", mrb->object_class);
  MRB_SET_INSTANCE_TT(re, MRB_TT_DATA);

  mrb_define_method(mrb, re, "initialize", regexp_pcre_initialize, ARGS_REQ(1) | ARGS_OPT(2));
  mrb_define_method(mrb, re, "match", regexp_pcre_match, ARGS_REQ(1));
}

void
mrb_mruby_regexp_pcre_gem_final(mrb_state *mrb)
{
}
