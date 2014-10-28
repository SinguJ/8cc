// Copyright 2012 Rui Ueyama <rui314@gmail.com>
// This program is free software licensed under the MIT license.

#include "8cc.h"

static char *maybe_add_bitfield(char *name, Ctype *ctype) {
    if (ctype->bitsize > 0)
        return format("%s:%d:%d", name, ctype->bitoff, ctype->bitoff + ctype->bitsize);
    return name;
}

static char *do_c2s(Dict *dict, Ctype *ctype) {
    if (!ctype)
        return "(nil)";
    switch (ctype->type) {
    case CTYPE_VOID: return "void";
    case CTYPE_BOOL: return "_Bool";
    case CTYPE_CHAR: return maybe_add_bitfield("char", ctype);
    case CTYPE_SHORT: return maybe_add_bitfield("short", ctype);
    case CTYPE_INT:  return maybe_add_bitfield("int", ctype);
    case CTYPE_LONG: return maybe_add_bitfield("long", ctype);
    case CTYPE_LLONG: return maybe_add_bitfield("long long", ctype);
    case CTYPE_FLOAT: return "float";
    case CTYPE_DOUBLE: return "double";
    case CTYPE_LDOUBLE: return "long double";
    case CTYPE_PTR:
        return format("*%s", do_c2s(dict, ctype->ptr));
    case CTYPE_ARRAY:
        return format("[%d]%s", ctype->len, do_c2s(dict, ctype->ptr));
    case CTYPE_STRUCT: {
        char *type = ctype->is_struct ? "struct" : "union";
        if (dict_get(dict, format("%p", ctype)))
            return format("(%s)", type);
        dict_put(dict, format("%p", ctype), (void *)1);
        Buffer *b = make_buffer();
        buf_printf(b, "(%s", type);
        for (Iter *i = vec_iter(dict_keys(ctype->fields)); !iter_end(i);) {
            char *key = iter_next(i);
            Ctype *fieldtype = dict_get(ctype->fields, key);
            buf_printf(b, " (%s)", do_c2s(dict, fieldtype));
        }
        buf_printf(b, ")");
        return buf_body(b);
    }
    case CTYPE_FUNC: {
        Buffer *b = make_buffer();
        buf_printf(b, "(");
        for (Iter *i = vec_iter(ctype->params); !iter_end(i);) {
            Ctype *t = iter_next(i);
            buf_printf(b, "%s", do_c2s(dict, t));
            if (!iter_end(i))
                buf_printf(b, ",");
        }
        buf_printf(b, ")=>%s", do_c2s(dict, ctype->rettype));
        return buf_body(b);
    }
    default:
        return format("(Unknown ctype: %d)", ctype->type);
    }
}

char *c2s(Ctype *ctype) {
    return do_c2s(make_dict(), ctype);
}

static void uop_to_string(Buffer *b, char *op, Node *node) {
    buf_printf(b, "(%s %s)", op, a2s(node->operand));
}

static void binop_to_string(Buffer *b, char *op, Node *node) {
    buf_printf(b, "(%s %s %s)", op, a2s(node->left), a2s(node->right));
}

static void a2s_declinit(Buffer *b, Vector *initlist) {
    for (Iter *i = vec_iter(initlist); !iter_end(i);) {
        Node *init = iter_next(i);
        buf_printf(b, "%s", a2s(init));
        if (!iter_end(i))
            buf_printf(b, " ");
    }
}

static void do_a2s(Buffer *b, Node *node) {
    if (!node) {
        buf_printf(b, "(nil)");
        return;
    }
    switch (node->type) {
    case AST_LITERAL:
        switch (node->ctype->type) {
        case CTYPE_CHAR:
            if (node->ival == '\n')      buf_printf(b, "'\n'");
            else if (node->ival == '\\') buf_printf(b, "'\\\\'");
            else if (node->ival == '\0') buf_printf(b, "'\\0'");
            else buf_printf(b, "'%c'", node->ival);
            break;
        case CTYPE_INT:
            buf_printf(b, "%d", node->ival);
            break;
        case CTYPE_LONG:
            buf_printf(b, "%ldL", node->ival);
            break;
        case CTYPE_FLOAT:
        case CTYPE_DOUBLE:
            buf_printf(b, "%f", node->fval);
            break;
        default:
            error("internal error");
        }
        break;
    case AST_STRING:
        buf_printf(b, "\"%s\"", quote_cstring(node->sval));
        break;
    case AST_LVAR:
        buf_printf(b, "lv=%s", node->varname);
        if (node->lvarinit) {
            buf_printf(b, "(");
            a2s_declinit(b, node->lvarinit);
            buf_printf(b, ")");
        }
        break;
    case AST_GVAR:
        buf_printf(b, "gv=%s", node->varname);
        break;
    case AST_FUNCALL:
    case AST_FUNCPTR_CALL: {
        buf_printf(b, "(%s)%s(", c2s(node->ctype),
                   node->type == AST_FUNCALL ? node->fname : a2s(node));
        for (Iter *i = vec_iter(node->args); !iter_end(i);) {
            buf_printf(b, "%s", a2s(iter_next(i)));
            if (!iter_end(i))
                buf_printf(b, ",");
        }
        buf_printf(b, ")");
        break;
    }
    case AST_FUNCDESG: {
        buf_printf(b, "(funcdesg %s)", a2s(node->fptr));
        break;
    }
    case AST_FUNC: {
        buf_printf(b, "(%s)%s(", c2s(node->ctype), node->fname);
        for (Iter *i = vec_iter(node->params); !iter_end(i);) {
            Node *param = iter_next(i);
            buf_printf(b, "%s %s", c2s(param->ctype), a2s(param));
            if (!iter_end(i))
                buf_printf(b, ",");
        }
        buf_printf(b, ")");
        do_a2s(b, node->body);
        break;
    }
    case AST_DECL:
        buf_printf(b, "(decl %s %s",
                   c2s(node->declvar->ctype),
                   node->declvar->varname);
        if (node->declinit) {
            buf_printf(b, " ");
            a2s_declinit(b, node->declinit);
        }
        buf_printf(b, ")");
        break;
    case AST_INIT:
        buf_printf(b, "%s@%d", a2s(node->initval), node->initoff, c2s(node->totype));
        break;
    case AST_CONV:
        buf_printf(b, "(conv %s=>%s)", a2s(node->operand), c2s(node->ctype));
        break;
    case AST_IF:
        buf_printf(b, "(if %s %s",
                   a2s(node->cond),
                   a2s(node->then));
        if (node->els)
            buf_printf(b, " %s", a2s(node->els));
        buf_printf(b, ")");
        break;
    case AST_TERNARY:
        buf_printf(b, "(? %s %s %s)",
                   a2s(node->cond),
                   a2s(node->then),
                   a2s(node->els));
        break;
    case AST_FOR:
        buf_printf(b, "(for %s %s %s %s)",
                   a2s(node->forinit),
                   a2s(node->forcond),
                   a2s(node->forstep),
                   a2s(node->forbody));
        break;
    case AST_WHILE:
        buf_printf(b, "(while %s %s)",
                   a2s(node->forcond),
                   a2s(node->forbody));
        break;
    case AST_DO:
        buf_printf(b, "(do %s %s)",
                   a2s(node->forcond),
                   a2s(node->forbody));
        break;
    case AST_RETURN:
        buf_printf(b, "(return %s)", a2s(node->retval));
        break;
    case AST_COMPOUND_STMT: {
        buf_printf(b, "{");
        for (Iter *i = vec_iter(node->stmts); !iter_end(i);) {
            do_a2s(b, iter_next(i));
            buf_printf(b, ";");
        }
        buf_printf(b, "}");
        break;
    }
    case AST_STRUCT_REF:
        do_a2s(b, node->struc);
        buf_printf(b, ".");
        buf_printf(b, node->field);
        break;
    case AST_ADDR:  uop_to_string(b, "addr", node); break;
    case AST_DEREF: uop_to_string(b, "deref", node); break;
    case OP_UMINUS: uop_to_string(b, "-", node); break;
    case OP_SAL:  binop_to_string(b, "<<", node); break;
    case OP_SAR:
    case OP_SHR:  binop_to_string(b, ">>", node); break;
    case OP_GE:  binop_to_string(b, ">=", node); break;
    case OP_LE:  binop_to_string(b, "<=", node); break;
    case OP_NE:  binop_to_string(b, "!=", node); break;
    case OP_PRE_INC: uop_to_string(b, "pre++", node); break;
    case OP_PRE_DEC: uop_to_string(b, "pre--", node); break;
    case OP_POST_INC: uop_to_string(b, "post++", node); break;
    case OP_POST_DEC: uop_to_string(b, "post--", node); break;
    case OP_LOGAND: binop_to_string(b, "and", node); break;
    case OP_LOGOR:  binop_to_string(b, "or", node); break;
    case OP_A_ADD:  binop_to_string(b, "+=", node); break;
    case OP_A_SUB:  binop_to_string(b, "-=", node); break;
    case OP_A_MUL:  binop_to_string(b, "*=", node); break;
    case OP_A_DIV:  binop_to_string(b, "/=", node); break;
    case OP_A_MOD:  binop_to_string(b, "%=", node); break;
    case OP_A_AND:  binop_to_string(b, "&=", node); break;
    case OP_A_OR:   binop_to_string(b, "|=", node); break;
    case OP_A_XOR:  binop_to_string(b, "^=", node); break;
    case OP_A_SAL:  binop_to_string(b, "<<=", node); break;
    case OP_A_SAR:
    case OP_A_SHR:  binop_to_string(b, ">>=", node); break;
    case '!': uop_to_string(b, "!", node); break;
    case '&': binop_to_string(b, "&", node); break;
    case '|': binop_to_string(b, "|", node); break;
    case OP_CAST: {
        buf_printf(b, "((%s)=>(%s) %s)",
                   c2s(node->operand->ctype),
                   c2s(node->ctype),
                   a2s(node->operand));
        break;
    }
    case OP_LABEL_ADDR:
        buf_printf(b, "&&%s", node->label);
        break;
    default: {
        char *left = a2s(node->left);
        char *right = a2s(node->right);
        if (node->type == OP_EQ)
            buf_printf(b, "(== ");
        else
            buf_printf(b, "(%c ", node->type);
        buf_printf(b, "%s %s)", left, right);
    }
    }
}

char *a2s(Node *node) {
    Buffer *b = make_buffer();
    do_a2s(b, node);
    return buf_body(b);
}

char *t2s(Token *tok) {
    if (!tok)
        return "(null)";
    switch (tok->type) {
    case TIDENT:
        return tok->sval;
    case TPUNCT:
        switch (tok->punct) {
#define punct(ident, str)                       \
            case ident: return str;
#define keyword(ident, str, _)                  \
            case ident: return str;
#include "keyword.h"
#undef keyword
#undef punct
        default: return format("%c", tok->c);
        }
    case TCHAR:
        return quote_char(tok->c);
    case TNUMBER:
        return tok->sval;
    case TSTRING:
        return format("\"%s\"", quote_cstring(tok->sval));
    case TNEWLINE:
        return "(newline)";
    case TSPACE:
        return "(space)";
    case TMACRO_PARAM:
        return "(macro-param)";
    }
    error("internal error: unknown token type: %d", tok->type);
}
