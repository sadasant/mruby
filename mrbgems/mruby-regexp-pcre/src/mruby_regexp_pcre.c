#include <mruby.h>
#include <mruby/class.h>
#include <mruby/array.h>
#include <mruby/string.h>
#include <mruby/data.h>
#include <mruby/variable.h>

#include <stdio.h>
#include <string.h>
#include <pcre.h>

struct mrb_regexp_pcre {
  pcre *re;
};

void
mrb_regexp_free(mrb_state *mrb, void *ptr)
{
  struct mrb_regexp_pcre *mrb_re = ptr;

  if (mrb_re->re)
    pcre_free(mrb_re->re);
  mrb_free(mrb, mrb_re);
}
static struct mrb_data_type mrb_regexp_type = { "Regexp", mrb_regexp_free };

static int
mrb_mruby_to_pcre_options(mrb_value options)
{
  int coptions = PCRE_DOTALL;

  if (mrb_nil_p(options)) {
    coptions = 0;
  } else if (mrb_fixnum_p(options)) {
    int nopt;
    nopt = mrb_fixnum(options);
    if (nopt & 1) coptions |= PCRE_CASELESS;
    if (nopt & 2) coptions |= PCRE_EXTENDED;
    if (nopt & 4) coptions |= PCRE_MULTILINE;
  } else if (mrb_string_p(options)) {
      if (strchr(RSTRING_PTR(options), 'i')) coptions |= PCRE_CASELESS;
      if (strchr(RSTRING_PTR(options), 'x')) coptions |= PCRE_EXTENDED;
      if (strchr(RSTRING_PTR(options), 'm')) coptions |= PCRE_MULTILINE;
  } else if (mrb_type(options) == MRB_TT_TRUE) {
    coptions |= PCRE_CASELESS;
  }

  return coptions;
}

mrb_value
regexp_pcre_initialize(mrb_state *mrb, mrb_value self)
{
  int erroff = 0, coptions;
  const char *errstr = NULL;
  struct mrb_regexp_pcre *reg = NULL;
  mrb_value source, opt = mrb_nil_value();

  mrb_get_args(mrb, "S|o", &source, &opt);

  reg = mrb_malloc(mrb, sizeof(struct mrb_regexp_pcre));
  memset(reg, 0, sizeof(struct mrb_regexp_pcre));
  DATA_PTR(self) = reg;
  DATA_TYPE(self) = &mrb_regexp_type;
  coptions = mrb_mruby_to_pcre_options(opt);

  source = mrb_str_new(mrb, RSTRING_PTR(source), RSTRING_LEN(source));
  reg->re = pcre_compile(RSTRING_PTR(source), coptions, &errstr, &erroff, NULL);
  if (reg->re == NULL) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "invalid regular expression");
  }
  mrb_iv_set(mrb, self, mrb_intern(mrb, "@source"), source);

  return self;
}

#define OVECCOUNT 30
mrb_value
regexp_pcre_match(mrb_state *mrb, mrb_value self)
{
  int i, rc;
  int nmatch;
  int *match;
  struct RClass *m;
  mrb_value md, str;
  mrb_int pos;
  struct mrb_regexp_pcre *reg;
  mrb_value args[2];

  reg = (struct mrb_regexp_pcre *)mrb_get_datatype(mrb, self, &mrb_regexp_type);
  if (!reg)
    return mrb_nil_value();

  mrb_get_args(mrb, "S|i", &str, &pos);

  rc = pcre_fullinfo(reg->re, NULL, PCRE_INFO_CAPTURECOUNT, &nmatch);
  if (rc < 0) {
    /* fullinfo error */
    return mrb_nil_value();
  }

  nmatch = (nmatch + 1) * 3; /* capture(nmatch) + substring(1) * multiple 3 */
  match = mrb_malloc(mrb, sizeof(int) * nmatch);
  rc = pcre_exec(reg->re, NULL, RSTRING_PTR(str), RSTRING_LEN(str), 0, 0, match, nmatch);
  if (rc < 0) {
    mrb_free(mrb, match);
    return mrb_nil_value();
  }

  m = mrb_class_get(mrb, "MatchData");
  md = mrb_class_new_instance(mrb, 0, NULL, m);
  for (i = 0; i < nmatch; i++) {
    args[0] = mrb_fixnum_value(match[2 * i]);
    args[1] = mrb_fixnum_value(match[2 * i + 1]);
    mrb_funcall_argv(mrb, md, mrb_intern(mrb, "push"), sizeof(args) / sizeof(args[0]), &args[0]);
  }

  mrb_iv_set(mrb, md, mrb_intern(mrb, "@regexp"), self);
  // XXX: length
  mrb_iv_set(mrb, md, mrb_intern(mrb, "@length"), mrb_fixnum_value(rc));
  mrb_iv_set(mrb, md, mrb_intern(mrb, "@string"), mrb_str_dup(mrb, str));

  free(match);
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
