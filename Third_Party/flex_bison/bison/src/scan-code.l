/* Bison Action Scanner                             -*- C -*-

   Copyright (C) 2006-2011 Free Software Foundation, Inc.

   This file is part of Bison, the GNU Compiler Compiler.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

%option debug nodefault noinput nounput noyywrap never-interactive
%option prefix="code_" outfile="lex.yy.c"

%{
/* Work around a bug in flex 2.5.31.  See Debian bug 333231
   <http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=333231>.  */
#undef code_wrap
#define code_wrap() 1

#define FLEX_PREFIX(Id) code_ ## Id
#include "flex-scanner.h"

#include "complain.h"
#include "reader.h"
#include "getargs.h"
#include "scan-code.h"
#include "symlist.h"

#include <c-ctype.h>
#include <get-errno.h>
#include <quote.h>

/* The current calling start condition: SC_RULE_ACTION or
   SC_SYMBOL_ACTION. */
# define YY_DECL static char *code_lex (code_props *self, int sc_context)
YY_DECL;

#define YY_USER_ACTION  location_compute (loc, &loc->end, yytext, yyleng);

static void handle_action_dollar (symbol_list *rule, char *cp,
				  location dollar_loc);
static void handle_action_at (symbol_list *rule, char *cp, location at_loc);

/* A string to be pushed to obstack after dollar/at has been handled. */
static char *ref_tail_fields;

static location the_location;
static location *loc = &the_location;

/* A string representing the most recent translation.  */
static char *last_string;

/* True if an untyped $$ or $n was seen.  */
static bool untyped_var_seen;

%}
 /* C and C++ comments in code. */
%x SC_COMMENT SC_LINE_COMMENT
 /* Strings and characters in code. */
%x SC_STRING SC_CHARACTER
 /* Whether in a rule or symbol action.  Specifies the translation
    of $ and @.  */
%x SC_RULE_ACTION SC_SYMBOL_ACTION


/* POSIX says that a tag must be both an id and a C union member, but
   historically almost any character is allowed in a tag.  We disallow
   NUL and newline, as this simplifies our implementation.  */
tag	 [^\0\n>]+

/* Zero or more instances of backslash-newline.  Following GCC, allow
   white space between the backslash and the newline.  */
splice	 (\\[ \f\t\v]*\n)*

/* C style identifier. Must start with letter. Will be used for
   named symbol references. Shall be kept synchronized with
   scan-gram.l "letter" and "id". */
letter	  [.abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_]
id	  {letter}({letter}|[-0-9])*
ref      -?[0-9]+|{id}|"["{id}"]"|"$"

%%

%{
  /* Nesting level of the current code in braces.  */
  int braces_level = 0;

  /* Whether a semicolon is probably needed.
     The heuristic is that a semicolon is not needed after `{', `}', `;',
     or a C preprocessor directive, and that whitespaces and comments
     do not affect this flag.
     Note that `{' does not need a semicolon because of `{}'.
     A semicolon may be needed before a cpp direcive, but don't bother.  */
  bool need_semicolon = false;

  /* Whether in a C preprocessor directive.  Don't use a start condition
     for this because, at the end of strings and comments, we still need
     to know whether we're in a directive.  */
  bool in_cpp = false;

  /* This scanner is special: it is invoked only once, henceforth
     is expected to return only once.  This initialization is
     therefore done once per action to translate. */
  aver (sc_context == SC_SYMBOL_ACTION
	|| sc_context == SC_RULE_ACTION
	|| sc_context == INITIAL);
  BEGIN sc_context;
%}

  /*------------------------------------------------------------.
  | Scanning a C comment.  The initial `/ *' is already eaten.  |
  `------------------------------------------------------------*/

<SC_COMMENT>
{
  "*"{splice}"/"  STRING_GROW; BEGIN sc_context;
}


  /*--------------------------------------------------------------.
  | Scanning a line comment.  The initial `//' is already eaten.  |
  `--------------------------------------------------------------*/

<SC_LINE_COMMENT>
{
  "\n"		 STRING_GROW; BEGIN sc_context;
  {splice}	 STRING_GROW;
}


  /*--------------------------------------------.
  | Scanning user-code characters and strings.  |
  `--------------------------------------------*/

<SC_CHARACTER,SC_STRING>
{
  {splice}|\\{splice}.	STRING_GROW;
}

<SC_CHARACTER>
{
  "'"		STRING_GROW; BEGIN sc_context;
}

<SC_STRING>
{
  "\""		STRING_GROW; BEGIN sc_context;
}


<SC_RULE_ACTION,SC_SYMBOL_ACTION>{
  "'" {
    STRING_GROW;
    BEGIN SC_CHARACTER;
    need_semicolon = true;
  }
  "\"" {
    STRING_GROW;
    BEGIN SC_STRING;
    need_semicolon = true;
  }
  "/"{splice}"*" {
    STRING_GROW;
    BEGIN SC_COMMENT;
  }
  "/"{splice}"/" {
    STRING_GROW;
    BEGIN SC_LINE_COMMENT;
  }
}

<SC_RULE_ACTION>
{
  "$"("<"{tag}">")?{ref}  {
    ref_tail_fields = 0;
    handle_action_dollar (self->rule, yytext, *loc);
    if (ref_tail_fields) {
      obstack_sgrow (&obstack_for_string, ref_tail_fields);
    }
    need_semicolon = true;
  }
  "@"{ref} {
    ref_tail_fields = 0;
    handle_action_at (self->rule, yytext, *loc);
    if (ref_tail_fields) {
      obstack_sgrow (&obstack_for_string, ref_tail_fields);
    }
    need_semicolon = true;
  }
  "$"  {
    warn_at (*loc, _("stray `$'"));
    obstack_sgrow (&obstack_for_string, "$][");
    need_semicolon = true;
  }
  "@"  {
    warn_at (*loc, _("stray `@'"));
    obstack_sgrow (&obstack_for_string, "@@");
    need_semicolon = true;
  }
  "["  {
    obstack_sgrow (&obstack_for_string, "@{");
    need_semicolon = true;
  }
  "]"  {
    obstack_sgrow (&obstack_for_string, "@}");
    need_semicolon = true;
  }

  ";"  STRING_GROW;                 need_semicolon = false;
  "{"  STRING_GROW; ++braces_level; need_semicolon = false;
  "}"  {
    bool outer_brace = --braces_level == 0;

    /* As an undocumented Bison extension, append `;' before the last
       brace in braced code, so that the user code can omit trailing
       `;'.  But do not append `;' if emulating Yacc, since Yacc does
       not append one.  */
    if (outer_brace && !yacc_flag && language_prio == default_prio
        && skeleton_prio == default_prio && need_semicolon && ! in_cpp)
      {
	warn_at (*loc, _("a `;' might be needed at the end of action code"));
	warn_at (*loc, _("future versions of Bison will not add the `;'"));
	obstack_1grow (&obstack_for_string, ';');
      }

    STRING_GROW;
    need_semicolon = false;
  }

  /* Preprocessing directives should only be recognized at the beginning
     of lines, allowing whitespace including comments, but in C/C++,
     `#' can only be the start of preprocessor directives or within
     `#define' directives anyway, so don't bother with begin of line.  */
  "#"       STRING_GROW; in_cpp = true;

  {splice}  STRING_GROW;
  [\n\r]    STRING_GROW; if (in_cpp) in_cpp = need_semicolon = false; 
  [ \t\f]   STRING_GROW;

  /* YYFAIL is undocumented and was formally deprecated in Bison
     2.4.2.  */
  YYFAIL {
    STRING_GROW; need_semicolon = true;
    warn_at (*loc, _("use of YYFAIL, which is deprecated and will be"
                     " removed"));
  }

  /* The sole purpose of this is to make sure identifiers that merely
     contain YYFAIL don't produce the above warning.  */
  [A-Za-z_][0-9A-Za-z_]* STRING_GROW; need_semicolon = true;

  . STRING_GROW; need_semicolon = true;
}

<SC_SYMBOL_ACTION>
{
  "$$" {
    obstack_sgrow (&obstack_for_string, "]b4_dollar_dollar[");
    self->is_value_used = true;
  }
  "@$" {
    obstack_sgrow (&obstack_for_string, "]b4_at_dollar[");
    locations_flag = true;
  }
}


  /*-----------------------------------------.
  | Escape M4 quoting characters in C code.  |
  `-----------------------------------------*/

<*>
{
  \$	obstack_sgrow (&obstack_for_string, "$][");
  \@	obstack_sgrow (&obstack_for_string, "@@");
  \[	obstack_sgrow (&obstack_for_string, "@{");
  \]	obstack_sgrow (&obstack_for_string, "@}");
}

  /*-----------------------------------------------------.
  | By default, grow the string obstack with the input.  |
  `-----------------------------------------------------*/

<*>.|\n	STRING_GROW;

 /* End of processing. */
<*><<EOF>>	 {
                   STRING_FINISH;
                   return last_string;
                 }

%%

static  bool
is_dot_or_dash (char ch)
{
  return ch == '.' || ch == '-';
}

static  bool
contains_dot_or_dash (const char* p)
{
  for (; *p; ++p)
    if (is_dot_or_dash (*p))
      return true;
  return false;
}

/* Defines a variant of a symbolic name resolution. */
typedef struct
{
  /* Index in symbol list. */
  unsigned symbol_index;

  /* Matched symbol id and loc. */
  uniqstr id;
  location loc;

  /* Hiding named reference. */
  named_ref* hidden_by;

  /* Error flags. May contain zero (no errors) or
     a combination of VARIANT_* values. */
  unsigned err;
} variant;

/* Set when the variant refers to a symbol hidden
   by an explicit symbol reference. */
#define VARIANT_HIDDEN (1 << 0)

/* Set when the variant refers to a symbol containing
   dots or dashes. Will require explicit bracketing. */
#define VARIANT_BAD_BRACKETING (1 << 1)

/* Set when the variant refers to a symbol which is
   not visible from current midrule. */
#define VARIANT_NOT_VISIBLE_FROM_MIDRULE (1 << 2)

static variant *variant_table = 0;
static unsigned variant_table_size = 0;
static unsigned variant_count = 0;

static variant *
variant_table_grow (void)
{
  ++variant_count;
  if (variant_count > variant_table_size)
    {
      while (variant_count > variant_table_size)
	variant_table_size = 2 * variant_table_size + 3;
      variant_table = xnrealloc (variant_table, variant_table_size,
				 sizeof *variant_table);
    }
  return &variant_table[variant_count - 1];
}

static void
variant_table_free (void)
{
  free (variant_table);
  variant_table = 0;
  variant_table_size = variant_count = 0;
}

static char *
find_prefix_end (const char *prefix, char *begin, char *end)
{
  char *ptr = begin;

  for (; *prefix && ptr != end; ++prefix, ++ptr)
    if (*prefix != *ptr)
      return 0;

  if (*prefix)
    return 0;

  return ptr;
}

static variant *
variant_add (uniqstr id, location id_loc, unsigned symbol_index,
	     char *cp, char *cp_end, bool explicit_bracketing)
{
  char *prefix_end;

  prefix_end = find_prefix_end (id, cp, cp_end);
  if (prefix_end &&
      (prefix_end == cp_end ||
       (!explicit_bracketing && is_dot_or_dash (*prefix_end))))
    {
      variant *r = variant_table_grow ();
      r->symbol_index = symbol_index;
      r->id = id;
      r->loc = id_loc;
      r->hidden_by = NULL;
      r->err = 0;
      return r;
    }
  else
    return NULL;
}

static const char *
get_at_spec(unsigned symbol_index)
{
  static char at_buf[20];
  if (symbol_index == 0)
    strcpy (at_buf, "$$");
  else
    snprintf (at_buf, sizeof at_buf, "$%u", symbol_index);
  return at_buf;
}

static void
show_sub_messages (const char* cp, bool explicit_bracketing,
                   int midrule_rhs_index, char dollar_or_at,
                   bool is_warning, unsigned indent)
{
  unsigned i;

  for (i = 0; i < variant_count; ++i)
    {
      const variant *var = &variant_table[i];
      const char *at_spec = get_at_spec (var->symbol_index);

      if (var->err == 0)
        {
          if (is_warning)
            warn_at_indent (var->loc, &indent, _("refers to: %c%s at %s"),
                            dollar_or_at, var->id, at_spec);
          else
            complain_at_indent (var->loc, &indent, _("refers to: %c%s at %s"),
                                dollar_or_at, var->id, at_spec);
        }
      else
	{
	  static struct obstack msg_buf;
	  const char *tail = explicit_bracketing ? "" :
	    cp + strlen (var->id);
	  const char *id = var->hidden_by ? var->hidden_by->id :
	    var->id;
	  location id_loc = var->hidden_by ? var->hidden_by->loc :
	    var->loc;

	  /* Create the explanation message. */
	  obstack_init (&msg_buf);

	  obstack_fgrow1 (&msg_buf, _("possibly meant: %c"), dollar_or_at);
	  if (contains_dot_or_dash (id))
	    obstack_fgrow1 (&msg_buf, "[%s]", id);
	  else
	    obstack_sgrow (&msg_buf, id);
	  obstack_sgrow (&msg_buf, tail);

	  if (var->err & VARIANT_HIDDEN)
	    {
	      obstack_fgrow1 (&msg_buf, _(", hiding %c"), dollar_or_at);
	      if (contains_dot_or_dash (var->id))
		obstack_fgrow1 (&msg_buf, "[%s]", var->id);
	      else
		obstack_sgrow (&msg_buf, var->id);
	      obstack_sgrow (&msg_buf, tail);
	    }

	  obstack_fgrow1 (&msg_buf, _(" at %s"), at_spec);

	  if (var->err & VARIANT_NOT_VISIBLE_FROM_MIDRULE)
            {
              const char *format =
                _(", cannot be accessed from mid-rule action at $%d");
              obstack_fgrow1 (&msg_buf, format, midrule_rhs_index);
            }

	  obstack_1grow (&msg_buf, '\0');
          if (is_warning)
            warn_at_indent (id_loc, &indent, "%s",
                            (char *) obstack_finish (&msg_buf));
          else
            complain_at_indent (id_loc, &indent, "%s",
                                (char *) obstack_finish (&msg_buf));
	  obstack_free (&msg_buf, 0);
	}
    }
}

/* Returned from "parse_ref" when the reference
   is inappropriate. */
#define INVALID_REF (INT_MIN)

/* Returned from "parse_ref" when the reference
   points to LHS ($$) of the current rule or midrule. */
#define LHS_REF (INT_MIN + 1)

/* Sub-messages indent. */
#define SUB_INDENT (4)

/* Parse named or positional reference. In case of positional
   references, can return negative values for $-n "deep" stack
   accesses. */
static long int
parse_ref (char *cp, symbol_list *rule, int rule_length,
	   int midrule_rhs_index, char *text, location text_loc,
	   char dollar_or_at)
{
  symbol_list *l;
  char *cp_end;
  bool explicit_bracketing;
  unsigned i;
  unsigned valid_variants = 0;
  unsigned valid_variant_index = 0;

  if ('$' == *cp)
    return LHS_REF;

  if (c_isdigit (*cp) || (*cp == '-' && c_isdigit (* (cp + 1))))
    {
      long int num = strtol (cp, &cp, 10);
      if (1 - INT_MAX + rule_length <= num && num <= rule_length)
	return num;
      else
	{
	  complain_at (text_loc, _("integer out of range: %s"),
                       quote (text));
	  return INVALID_REF;
	}
    }

  if ('[' == *cp)
    {
      /* Ignore the brackets. */
      char *p;
      for (p = ++cp; *p != ']'; ++p)
	continue;
      cp_end = p;

      explicit_bracketing = true;
    }
  else
    {
      /* Take all characters of the name. */
      char* p;
      for (p = cp; *p; ++p)
	if (is_dot_or_dash (*p))
	  {
	    ref_tail_fields = p;
	    break;
	  }
      for (p = cp; *p; ++p)
	continue;
      cp_end = p;

      explicit_bracketing = false;
    }

  /* Add all relevant variants. */
  {
    unsigned symbol_index;
    variant_count = 0;
    for (symbol_index = 0, l = rule; !symbol_list_null (l);
         ++symbol_index, l = l->next)
      {
	variant *var;
	if (l->content_type != SYMLIST_SYMBOL)
	  continue;

	var = variant_add (l->content.sym->tag, l->sym_loc,
                           symbol_index, cp, cp_end, explicit_bracketing);
	if (var && l->named_ref)
	  var->hidden_by = l->named_ref;

	if (l->named_ref)
	  variant_add (l->named_ref->id, l->named_ref->loc,
                       symbol_index, cp, cp_end, explicit_bracketing);
      }
  }

  /* Check errors. */
  for (i = 0; i < variant_count; ++i)
    {
      variant *var = &variant_table[i];
      unsigned symbol_index = var->symbol_index;

      /* Check visibility from mid-rule actions. */
      if (midrule_rhs_index != 0
	  && (symbol_index == 0 || midrule_rhs_index < symbol_index))
        var->err |= VARIANT_NOT_VISIBLE_FROM_MIDRULE;

      /* Check correct bracketing. */
      if (!explicit_bracketing && contains_dot_or_dash (var->id))
        var->err |= VARIANT_BAD_BRACKETING;

      /* Check using of hidden symbols. */
      if (var->hidden_by)
        var->err |= VARIANT_HIDDEN;

      if (!var->err)
        {
          valid_variant_index = i;
          ++valid_variants;
        }
    }

  switch (valid_variants)
    {
    case 0:
      {
        unsigned len = (explicit_bracketing || !ref_tail_fields) ?
          cp_end - cp : ref_tail_fields - cp;
        unsigned indent = 0;

        complain_at_indent (text_loc, &indent, _("invalid reference: %s"),
                            quote (text));
        indent += SUB_INDENT;
        if (len == 0)
          {
            location sym_loc = text_loc;
            sym_loc.start.column += 1;
            sym_loc.end = sym_loc.start;
            const char *format =
              _("syntax error after `%c', expecting integer, letter,"
                " `_', `[', or `$'");
            complain_at_indent (sym_loc, &indent, format, dollar_or_at);
          }
        else if (midrule_rhs_index)
          {
            const char *format =
              _("symbol not found in production before $%d: %.*s");
            complain_at_indent (rule->location, &indent, format,
                                midrule_rhs_index, len, cp);
          }
        else
          {
            const char *format =
              _("symbol not found in production: %.*s");
            complain_at_indent (rule->location, &indent, format,
                                len, cp);
          }

        if (variant_count > 0)
          show_sub_messages (cp, explicit_bracketing, midrule_rhs_index,
                             dollar_or_at, false, indent);
        return INVALID_REF;
      }
    case 1:
      {
        unsigned indent = 0;
        if (variant_count > 1)
          {
            warn_at_indent (text_loc, &indent, _("misleading reference: %s"),
                            quote (text));
            show_sub_messages (cp, explicit_bracketing, midrule_rhs_index,
                               dollar_or_at, true, indent + SUB_INDENT);
          }
        {
          unsigned symbol_index =
            variant_table[valid_variant_index].symbol_index;
          return (symbol_index == midrule_rhs_index) ? LHS_REF : symbol_index;
        }
      }
    case 2:
    default:
      {
        unsigned indent = 0;
        complain_at_indent (text_loc, &indent, _("ambiguous reference: %s"),
                            quote (text));
        show_sub_messages (cp, explicit_bracketing, midrule_rhs_index,
                           dollar_or_at, false, indent + SUB_INDENT);
        return INVALID_REF;
      }
    }

  /* Not reachable. */
  return INVALID_REF;
}

/* Keeps track of the maximum number of semantic values to the left of
   a handle (those referenced by $0, $-1, etc.) are required by the
   semantic actions of this grammar. */
int max_left_semantic_context = 0;


/*------------------------------------------------------------------.
| TEXT is pointing to a wannabee semantic value (i.e., a `$').      |
|                                                                   |
| Possible inputs: $[<TYPENAME>]($|integer)                         |
|                                                                   |
| Output to OBSTACK_FOR_STRING a reference to this semantic value.  |
`------------------------------------------------------------------*/

static void
handle_action_dollar (symbol_list *rule, char *text, location dollar_loc)
{
  char const *type_name = NULL;
  char *cp = text + 1;
  char *gt_ptr = 0;
  symbol_list *effective_rule;
  int effective_rule_length;
  int n;

  if (rule->midrule_parent_rule)
    {
      effective_rule = rule->midrule_parent_rule;
      effective_rule_length = rule->midrule_parent_rhs_index - 1;
    }
  else
    {
      effective_rule = rule;
      effective_rule_length = symbol_list_length (rule->next);
    }

  /* Get the type name if explicit. */
  if (*cp == '<')
    {
      type_name = ++cp;
      while (*cp != '>')
	++cp;

      /* The '>' symbol will be later replaced by '\0'. Original
	 'text' is needed for error messages. */
      gt_ptr = cp;
      ++cp;
      if (untyped_var_seen)
	complain_at (dollar_loc, _("explicit type given in untyped grammar"));
      tag_seen = true;
    }

  n = parse_ref (cp, effective_rule, effective_rule_length,
		 rule->midrule_parent_rhs_index, text, dollar_loc, '$');

  if (gt_ptr)
    *gt_ptr = '\0';

  switch (n)
    {
    case INVALID_REF:
      break;

    case LHS_REF:
      if (!type_name)
	type_name = symbol_list_n_type_name_get (rule, dollar_loc, 0);

      if (!type_name)
	{
	  if (union_seen | tag_seen)
	    {
	      if (rule->midrule_parent_rule)
		complain_at (dollar_loc,
			     _("$$ for the midrule at $%d of `%s'"
			       " has no declared type"),
			     rule->midrule_parent_rhs_index,
			     effective_rule->content.sym->tag);
	      else
		complain_at (dollar_loc, _("$$ of `%s' has no declared type"),
			     rule->content.sym->tag);
	    }
	  else
	    untyped_var_seen = true;
	  type_name = "";
	}

      obstack_fgrow1 (&obstack_for_string,
		      "]b4_lhs_value([%s])[", type_name);
      rule->action_props.is_value_used = true;
      break;

    default:
      if (max_left_semantic_context < 1 - n)
	max_left_semantic_context = 1 - n;
      if (!type_name && 0 < n)
	type_name =
	  symbol_list_n_type_name_get (effective_rule, dollar_loc, n);
      if (!type_name)
	{
	  if (union_seen | tag_seen)
	    complain_at (dollar_loc, _("$%s of `%s' has no declared type"),
			 cp, effective_rule->content.sym->tag);
	  else
	    untyped_var_seen = true;
	  type_name = "";
	}

      obstack_fgrow3 (&obstack_for_string,
		      "]b4_rhs_value(%d, %d, [%s])[",
		      effective_rule_length, n, type_name);
      if (n > 0)
	symbol_list_n_get (effective_rule, n)->action_props.is_value_used =
	  true;
      break;
    }
}


/*------------------------------------------------------.
| TEXT is a location token (i.e., a `@...').  Output to |
| OBSTACK_FOR_STRING a reference to this location.      |
`------------------------------------------------------*/

static void
handle_action_at (symbol_list *rule, char *text, location at_loc)
{
  char *cp = text + 1;
  symbol_list *effective_rule;
  int effective_rule_length;
  int n;

  if (rule->midrule_parent_rule)
    {
      effective_rule = rule->midrule_parent_rule;
      effective_rule_length = rule->midrule_parent_rhs_index - 1;
    }
  else
    {
      effective_rule = rule;
      effective_rule_length = symbol_list_length (rule->next);
    }

  locations_flag = true;

  n = parse_ref (cp, effective_rule, effective_rule_length,
		       rule->midrule_parent_rhs_index, text, at_loc, '@');
  switch (n)
    {
    case INVALID_REF:
      break;

    case LHS_REF:
      obstack_sgrow (&obstack_for_string, "]b4_lhs_location[");
      break;

    default:
      obstack_fgrow2 (&obstack_for_string, "]b4_rhs_location(%d, %d)[",
		      effective_rule_length, n);
      break;
    }
}


/*-------------------------.
| Initialize the scanner.  |
`-------------------------*/

/* Translate the dollars and ats in \a self, in the context \a sc_context
   (SC_RULE_ACTION, SC_SYMBOL_ACTION, INITIAL).  */

static char const *
translate_action (code_props *self, int sc_context)
{
  char *res;
  static bool initialized = false;
  if (!initialized)
    {
      obstack_init (&obstack_for_string);
      yy_flex_debug = 0;
      initialized = true;
    }

  loc->start = loc->end = self->location.start;
  yy_switch_to_buffer (yy_scan_string (self->code));
  res = code_lex (self, sc_context);
  yy_delete_buffer (YY_CURRENT_BUFFER);

  return res;
}

/*------------------------------------------------------------------------.
| Implementation of the public interface as documented in "scan-code.h".  |
`------------------------------------------------------------------------*/

void
code_props_none_init (code_props *self)
{
  *self = code_props_none;
}

code_props const code_props_none = CODE_PROPS_NONE_INIT;

void
code_props_plain_init (code_props *self, char const *code,
		       location code_loc)
{
  self->kind = CODE_PROPS_PLAIN;
  self->code = code;
  self->location = code_loc;
  self->is_value_used = false;
  self->rule = NULL;
  self->named_ref = NULL;
}

void
code_props_symbol_action_init (code_props *self, char const *code,
                               location code_loc)
{
  self->kind = CODE_PROPS_SYMBOL_ACTION;
  self->code = code;
  self->location = code_loc;
  self->is_value_used = false;
  self->rule = NULL;
  self->named_ref = NULL;
}

void
code_props_rule_action_init (code_props *self, char const *code,
                             location code_loc, symbol_list *rule,
			     named_ref *name)
{
  self->kind = CODE_PROPS_RULE_ACTION;
  self->code = code;
  self->location = code_loc;
  self->is_value_used = false;
  self->rule = rule;
  self->named_ref = name;
}

void
code_props_translate_code (code_props *self)
{
  switch (self->kind)
    {
      case CODE_PROPS_NONE:
        break;
      case CODE_PROPS_PLAIN:
        self->code = translate_action (self, INITIAL);
        break;
      case CODE_PROPS_SYMBOL_ACTION:
        self->code = translate_action (self, SC_SYMBOL_ACTION);
        break;
      case CODE_PROPS_RULE_ACTION:
        self->code = translate_action (self, SC_RULE_ACTION);
        break;
    }
}

void
code_scanner_last_string_free (void)
{
  STRING_FREE;
}

void
code_scanner_free (void)
{
  obstack_free (&obstack_for_string, 0);
  variant_table_free ();

  /* Reclaim Flex's buffers.  */
  yylex_destroy ();
}
