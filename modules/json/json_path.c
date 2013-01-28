/*
 * Copyright (c) 2009-2012 Petri Lehtinen <petri@digip.org>
 * Copyright (c) 2011-2012 Basile Starynkevitch <basile@starynkevitch.net>
 * Copyright (c) 2012 Rogerz Zhang <rogerz.zhang@gmail.com>
 *
 * Jansson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. 
 *
 * Pulled from https://github.com/rogerz/jansson/blob/json_path/src/path.c
 */

#include <string.h>
#include <assert.h>

#include <jansson.h>

static json_malloc_t do_malloc = malloc;
static json_free_t do_free = free;

void *jsonp_malloc(size_t size) {
    if(!size)
        return NULL;

    return (*do_malloc)(size);
}

void jsonp_free(void *ptr) {
    if(!ptr)
        return;

    (*do_free)(ptr);
}

char *jsonp_strdup(const char *str) {
    char *new_str;

    new_str = jsonp_malloc(strlen(str) + 1); 
    if(!new_str)
        return NULL;

    strcpy(new_str, str);
    return new_str;
}

json_t *json_path_get(const json_t *json, const char *path)
{
    static const char array_open = '[';
    static const char *path_delims = ".[", *array_close = "]";
    const json_t *cursor;
    char *token, *buf, *peek, *endptr, delim = '\0';
    const char *expect;

    if (!json || !path)
        return NULL;
    else
        buf = jsonp_strdup(path);

    peek = buf;
    token = buf;
    cursor = json;
    expect = path_delims;

    if (*token == array_open) {
        expect = array_close;
        token++;
    }

    while (peek && *peek && cursor)
    {
        char *last_peek = peek;
        peek = strpbrk(peek, expect);
        if (peek) {
            if (!token && peek != last_peek)
                goto fail;
            delim = *peek;
            *peek++ = '\0';
        } else if (expect != path_delims || !token) {
            goto fail;
        }

        if (expect == path_delims) {
            if (token) {
                cursor = json_object_get(cursor, token);
            }
            expect = (delim == array_open ? array_close : path_delims);
            token = peek;
        } else if (expect == array_close) {
            size_t index = strtol(token, &endptr, 0);
            if (*endptr)
                goto fail;
            cursor = json_array_get(cursor, index);
            token = NULL;
            expect = path_delims;
        } else {
            goto fail;
        }
    }

    jsonp_free(buf);
    return (json_t *)cursor;
fail:
    jsonp_free(buf);
    return NULL;
}

