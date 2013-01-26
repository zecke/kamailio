/*
 * Copyright (c) 2012 Rogerz Zhang <rogerz.zhang@gmail.com>
 *
 * Jansson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. 
 */


#ifndef _JSON_FUNCS_H_
#define _JSON_FUNCS_H_
#include <jansson.h>

json_t *json_path_get(const json_t *json, const char *path);

#endif

