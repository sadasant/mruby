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

struct mrb_matchdata {
  mrb_int length;
  int *ovector;
};

void
mrb_regexp_free(mrb_state *mrb, void *ptr)
{
  struct mrb_regexp_pcre *mrb_re = ptr;

  if (mrb_re->re)
    pcre_free(mrb_re->re);
  mrb_free(mrb, mrb_re);
}

static void
mrb_matchdata_free(mrb_state *mrb, void *ptr)
{
  struct mrb_matchdata *mrb_md = ptr;

  if (mrb_md->ovector != NULL)
    mrb_free(mrb, mrb_md->ovector);
  mrb_free(mrb, mrb_md);
}

static struct mrb_data_type mrb_regexp_type = { "Regexp", mrb_regexp_free };
static struct mrb_data_type mrb_matchdata_type = { "MatchData", mrb_matchdata_free };

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
  mrb_iv_set(mrb, self, mrb_intern(mrb, "@options"), mrb_fixnum_value(coptions));

  unsigned char *name_table;
  int i, namecount, name_entry_size;

  pcre_fullinfo(reg->re, NULL, PCRE_INFO_NAMECOUNT, &namecount);
  if (namecount > 0) {
    pcre_fullinfo(reg->re, NULL, PCRE_INFO_NAMETABLE, &name_table);
    pcre_fullinfo(reg->re, NULL, PCRE_INFO_NAMEENTRYSIZE, &name_entry_size);
    unsigned char *tabptr = name_table;
    for (i = 0; i < namecount; i++) {
      int n = (tabptr[0] << 8) | tabptr[1];
      mrb_funcall(mrb, self, "name_push", 2, mrb_str_new(mrb, tabptr + 2, strlen(tabptr + 2)), mrb_fixnum_value(n));
      tabptr += name_entry_size;
    }
  } 

  return self;
}

mrb_value
regexp_pcre_match(mrb_state *mrb, mrb_value self)
{
  struct mrb_matchdata *mrb_md;
  int i, rc;
  int ccount, matchlen, nmatch;
  int *match;
  struct RClass *c;
  mrb_value md, str;
  mrb_int pos;
  struct mrb_regexp_pcre *reg;
  mrb_value args[2];

  reg = (struct mrb_regexp_pcre *)mrb_get_datatype(mrb, self, &mrb_regexp_type);
  if (!reg)
    return mrb_nil_value();

  pos = 0;
  mrb_get_args(mrb, "S|i", &str, &pos);

  // XXX: RSTRING_LEN(str) >= pos ...

  rc = pcre_fullinfo(reg->re, NULL, PCRE_INFO_CAPTURECOUNT, &ccount);
  if (rc < 0) {
    /* fullinfo error */
    return mrb_nil_value();
  }
  matchlen = ccount + 1;
  match = mrb_malloc(mrb, sizeof(int) * matchlen * 3);
  rc = pcre_exec(reg->re, NULL, RSTRING_PTR(str) + pos, RSTRING_LEN(str) - pos, 0, 0, match, matchlen * 3);
  if (rc < 0) {
    mrb_free(mrb, match);
    return mrb_nil_value();
  }

  /* XXX: need current scope */
  mrb_obj_iv_set(mrb, (struct RObject *)mrb_class_real(RDATA(self)->c), mrb_intern(mrb, "@last_match"), mrb_nil_value());

  c = mrb_class_get(mrb, "MatchData");
  md = mrb_funcall(mrb, mrb_obj_value(c), "new", 0);

  mrb_md = (struct mrb_matchdata *)mrb_get_datatype(mrb, md, &mrb_matchdata_type);
  mrb_md->ovector = match;
  mrb_md->length = matchlen;

  mrb_iv_set(mrb, md, mrb_intern(mrb, "@regexp"), self);
  mrb_iv_set(mrb, md, mrb_intern(mrb, "@string"), mrb_str_dup(mrb, str));
  /* XXX: need current scope */
  mrb_obj_iv_set(mrb, (struct RObject *)mrb_class_real(RDATA(self)->c), mrb_intern(mrb, "@last_match"), md);

  return md;
}

static mrb_value
regexp_equal(mrb_state *mrb, mrb_value self)
{
  mrb_value other;
  struct mrb_regexp_pcre *self_reg, *other_reg;

  mrb_get_args(mrb, "o", &other);
  if (mrb_obj_equal(mrb, self, other)) {
    return mrb_true_value();
  }

  if (mrb_type(other) != MRB_TT_DATA || DATA_TYPE(other) != &mrb_regexp_type) {
    return mrb_false_value();
  }

  self_reg = (struct mrb_regexp_pcre *)DATA_PTR(self);
  other_reg = (struct mrb_regexp_pcre *)DATA_PTR(other);
  if (!self_reg || !other_reg) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Invalid Regexp");
  }

  if (mrb_str_equal(mrb, mrb_iv_get(mrb, self, mrb_intern(mrb, "@source")),
      mrb_iv_get(mrb, other, mrb_intern(mrb, "@source")))) {
    return mrb_true_value();
  }

  return mrb_false_value();
}

mrb_value
mrb_matchdata_init(mrb_state *mrb, mrb_value self)
{
  struct mrb_matchdata *mrb_md;

  mrb_md = (struct mrb_matchdata *)mrb_get_datatype(mrb, self, &mrb_matchdata_type);
  if (mrb_md) {
    mrb_matchdata_free(mrb, mrb_md);
  }

  mrb_md = (struct mrb_matchdata *)mrb_malloc(mrb, sizeof(*mrb_md));
  mrb_md->ovector = NULL;
  mrb_md->length = -1;

  DATA_PTR(self) = mrb_md;
  DATA_TYPE(self) = &mrb_matchdata_type;

  return self;
}

mrb_value
mrb_matchdata_init_copy(mrb_state *mrb, mrb_value copy)
{
  mrb_sym sym;
  mrb_value src;
  struct mrb_matchdata *mrb_md_copy, *mrb_md_src;
  int vecsize;

  mrb_get_args(mrb, "o", &src);

  if (mrb_obj_equal(mrb, copy, src)) return copy;
  if (!mrb_obj_is_instance_of(mrb, src, mrb_obj_class(mrb, copy))) {
    mrb_raise(mrb, E_TYPE_ERROR, "wrong argument class");
  }

  mrb_md_copy = (struct mrb_matchdata *)mrb_malloc(mrb, sizeof(*mrb_md_copy));
  mrb_md_src  = DATA_PTR(src);

  if (mrb_md_src->ovector == NULL) {
    mrb_md_copy->ovector = NULL;
    mrb_md_copy->length = -1;
  } else {
    vecsize = sizeof(int) * mrb_md_src->length * 3;
    mrb_md_copy->ovector = mrb_malloc(mrb, vecsize);
    memcpy(mrb_md_copy->ovector, mrb_md_src->ovector, vecsize);
    mrb_md_copy->length = mrb_md_src->length;
  }

  if (DATA_PTR(copy) != NULL) {
    mrb_matchdata_free(mrb, DATA_PTR(copy));
  }
  DATA_PTR(copy) = mrb_md_copy;

  mrb_iv_set(mrb, copy, mrb_intern(mrb, "@regexp"), mrb_iv_get(mrb, src, mrb_intern(mrb, "@regexp")));
  mrb_iv_set(mrb, copy, mrb_intern(mrb, "@string"), mrb_iv_get(mrb, src, mrb_intern(mrb, "@string")));

  return copy;
}

static mrb_value
matchdata_beginend(mrb_state *mrb, mrb_value self, int beginend)
{
  struct mrb_matchdata *mrb_md;
  mrb_int i, offs;

  mrb_md = (struct mrb_matchdata *)mrb_get_datatype(mrb, self, &mrb_matchdata_type);
  if (!mrb_md) return mrb_nil_value();

  mrb_get_args(mrb, "i", &i);
  if (i < 0 || i >= mrb_md->length)
    mrb_raisef(mrb, E_INDEX_ERROR, "index %d out of matches", i);

  offs = mrb_md->ovector[i*2 + beginend];
  if (offs != -1)
    return mrb_fixnum_value(offs);
  else
    return mrb_nil_value();
}

mrb_value
mrb_matchdata_begin(mrb_state *mrb, mrb_value self)
{
  return matchdata_beginend(mrb, self, 0);
}

mrb_value
mrb_matchdata_end(mrb_state *mrb, mrb_value self)
{
  return matchdata_beginend(mrb, self, 1);
}

mrb_value
mrb_matchdata_length(mrb_state *mrb, mrb_value self)
{
  struct mrb_matchdata *mrb_md;

  mrb_md = (struct mrb_matchdata *)mrb_get_datatype(mrb, self, &mrb_matchdata_type);
  if (!mrb_md) return mrb_nil_value();

  return mrb_fixnum_value(mrb_md->length);
}

void
mrb_mruby_regexp_pcre_gem_init(mrb_state *mrb)
{
  struct RClass *re, *md;

  re = mrb_define_class(mrb, "Regexp", mrb->object_class);
  MRB_SET_INSTANCE_TT(re, MRB_TT_DATA);

  mrb_define_method(mrb, re, "initialize", regexp_pcre_initialize, ARGS_REQ(1) | ARGS_OPT(2));
  mrb_define_method(mrb, re, "match", regexp_pcre_match, ARGS_REQ(1));
  mrb_define_method(mrb, re, "==", regexp_equal, ARGS_REQ(1));

  mrb_define_const(mrb, re, "IGNORECASE", mrb_fixnum_value(1));
  mrb_define_const(mrb, re, "EXTENDED", mrb_fixnum_value(2));
  mrb_define_const(mrb, re, "MULTILINE", mrb_fixnum_value(4));

  md = mrb_define_class(mrb, "MatchData", mrb->object_class);
  MRB_SET_INSTANCE_TT(md, MRB_TT_DATA);

  mrb_define_method(mrb, md, "initialize", mrb_matchdata_init, ARGS_REQ(1));
  mrb_define_method(mrb, md, "initialize_copy", mrb_matchdata_init_copy, ARGS_REQ(1));
  mrb_define_method(mrb, md, "begin", mrb_matchdata_begin, ARGS_REQ(1));
  mrb_define_method(mrb, md, "end", mrb_matchdata_end, ARGS_REQ(1));
  mrb_define_method(mrb, md, "length", mrb_matchdata_length, ARGS_NONE());
}

void
mrb_mruby_regexp_pcre_gem_final(mrb_state *mrb)
{
}
