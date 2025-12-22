#pragma once
typedef void (*from_chars_f32_t)(const char*, const char*, float*);
typedef void (*from_chars_f64_t)(const char*, const char*, double*);
typedef char* (*to_chars_f32_t)(char*, char*, float);
typedef char* (*to_chars_f64_t)(char*, char*, double);
