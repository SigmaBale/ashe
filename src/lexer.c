#include "lexer.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define is_reserved_symbol(ch)                                                      \
    ((ch) == '&' || (ch) == '|' || (ch) == '>' || (ch) == '<' || (ch) == ';')

static token_t token_without_quotes(tokentype_t ttype, byte *word);
static void lexer_get_string(lexer_t *lexer);
static void lexer_skip_ws(lexer_t *lexer);

lexer_t lexer_new(const byte *start, size_t len)
{
    return (lexer_t){.token = {0}, .iter = chariter_new((byte *) start, len)};
}

static void lexer_get_string(lexer_t *lexer)
{
    static byte word[ARG_MAX];

    int c, i;
    byte *ptr;
    byte old;
    bool dquote = false;
    bool escape = false;
    chariter_t *iter = &lexer->iter;
    tokentype_t ttype;

    for(i = 0; ((c = chariter_peek(iter)) != EOL); i++) {
        if(!dquote && ((isspace(c)) || is_reserved_symbol(c))) {
            break;
        }

        if(!escape && c == '"') {
            dquote ^= true;
        } else if(c == '\\' || escape) {
            escape ^= true;
        }

        word[i] = chariter_next(iter);
    }

    word[i] = '\0';
    ttype = WORD_TOKEN;

    if((ptr = strstr(word, "=")) != NULL && word != ptr) {
        old = *ptr;
        *ptr = NULL_TERM;
        if(strspn(word, PORTABLE_CHARACTER_SET) == strlen(word))
            ttype = KVPAIR_TOKEN;
        *ptr = old;
    }

    printf("Processed so far -> '%s'\n", word);
    lexer->token = token_without_quotes(ttype, word);
}

static token_t token_without_quotes(tokentype_t ttype, byte *word)
{
    token_t token;
    string_t *string = string_from(word);
    byte *ptr = string_slice(string, 0);

    if(is_null(string)) {
        return token_new(OOM_TOKEN, NULL);
    }

    token.type = ttype;
    token.contents = string;

    while(is_some(ptr = strchr(ptr, '"'))) {
        if(char_before_ptr(ptr) != '\\') {
            string_remove_at_ptr(string, ptr);
        } else {
            ptr++;
        }
    }

    return token;
}

static void lexer_skip_ws(lexer_t *lexer)
{
    int c;
    chariter_t *iter = &lexer->iter;
    while(isspace((c = chariter_peek(iter)))) chariter_next(iter);
}

token_t lexer_next(lexer_t *lexer)
{
    int c;
    chariter_t *iter = &lexer->iter;

    printf("\n\nSCANNING FOR NEXT TOKEN\n");
    lexer_skip_ws(lexer);

    printf("FIRST CHAR IS '%d'|'%c'\n\n", chariter_peek(iter), chariter_peek(iter));
    switch((c = chariter_peek(iter))) {
        case '2':
            chariter_next(iter);
            if(chariter_peek(iter) == '>') {
                chariter_next(iter);
                if(chariter_peek(iter) == '>') {
                    lexer->token = token_new(REDIROP_TOKEN, "2>>");
                } else {
                    chariter_goback_unsafe(iter, 1);
                    lexer->token = token_new(REDIROP_TOKEN, "2>");
                }
            } else {
                lexer->token = token_new(NAT_TOKEN, NULL);
            }
            break;
        case '>':
            chariter_next(iter);
            if(chariter_peek(iter) == '>') {
                lexer->token = token_new(REDIROP_TOKEN, ">>");
            } else {
                chariter_goback_unsafe(iter, 1);
                lexer->token = token_new(REDIROP_TOKEN, ">");
            }
            break;
        case '<':
            lexer->token = token_new(REDIROP_TOKEN, "<");
            break;
        case '&':
            chariter_next(iter);
            if(chariter_peek(iter) == '&') {
                lexer->token = token_new(AND_TOKEN, "&&");
            } else {
                chariter_goback_unsafe(iter, 1);
                lexer->token = token_new(BG_TOKEN, "&");
            }
            break;
        case '|':
            chariter_next(iter);
            if(chariter_peek(iter) == '|') {
                lexer->token = token_new(OR_TOKEN, "||");
            } else {
                chariter_goback_unsafe(iter, 1);
                lexer->token = token_new(PIPE_TOKEN, "|");
            }
            break;
        case ';':
            lexer->token = token_new(FG_TOKEN, ";");
            break;
        case '\0':
            lexer->token = token_new(EOL_TOKEN, NULL);
            break;
        default:
            lexer_get_string(lexer);
            break;
    }

    chariter_next(iter);
    return lexer->token;
}

token_t lexer_peek(lexer_t *lexer)
{
    return lexer->token;
}
