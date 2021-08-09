// adc_log Logging library by Anthony Del Ciotto.
// Implements a simple logger which can output to multiple streams.
//
// Credit to rxi's log library which was used as a reference.
// https://github.com/rxi/log.c
//
// MIT License

// Copyright (c) 2021 Anthony Del Ciotto

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// Begin interface

#ifndef _ADC_LOG_H_
#define _ADC_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h> // For va_start, va_end
#include <stdio.h>  // For vfprintf, fprintf, fflush
#include <time.h>   // For time, localtime

// 0.1.1
#define ADC_LOG_VERSION_MAJOR 0
#define ADC_LOG_VERSION_MINOR 1
#define ADC_LOG_VERSION_PATCH 1

// Allow overriding of MAX_CALLBACKS.
#ifndef ADC_LOG_MAX_CALLBACKS
#define ADC_LOG_MAX_CALLBACKS 32
#endif

// Allow overriding of USE_COLOR.
#ifndef ADC_LOG_USE_COLOR
#define ADC_LOG_USE_COLOR 1
#endif

enum { ADC_LOG_DEBUG, ADC_LOG_INFO, ADC_LOG_WARN, ADC_LOG_ERROR };

// A struct which represents a log message. Passed to all callbacks.
typedef struct {
  const char *fmt;
  const char *file;
  int line;
  int level;
  va_list args;
  struct tm *time;
  void *userdata;
} adc_log_msg;

typedef void (*adc_log_lock_handler)(int lock, void *userdata);
typedef void (*adc_log_log_handler)(adc_log_msg *msg);

// Log macros
#define adc_log_debug(...)                                                     \
  adc__log(ADC_LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define adc_log_info(...)                                                      \
  adc__log(ADC_LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define adc_log_warn(...)                                                      \
  adc__log(ADC_LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define adc_log_error(...)                                                     \
  adc__log(ADC_LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)

// adc_log_set_lock_handler() - Set the logger lock handler.
//
// Useful when the log will be written to from multiple threads. The handler
// function is passed a boolean true if the lock should be aquired or false
// if the lock should be released.
void adc_log_set_lock_handler(adc_log_lock_handler handler, void *userdata);

// adc_log_set_level() - Set the current logging level.
//
// All logs >= the given level will be be written to stderr.
void adc_log_set_level(int level);

// adc_log_add_callback() - Add a callback handler.
//
// One or more callbacks can be added which are called whenever a message is
// logged. The callback will be passed an adc_log_msg struct.
int adc_log_add_callback(adc_log_log_handler handler, void *userdata,
                         int level);

// adc_log_add_fp() - Add an output stream (file pointer).
//
// One or more file pointers can be added which will all be written to whenever
// a message is logged.
//
// All logs >= the given level will be written to the given file pointer.
int adc_log_add_fp(FILE *fp, int level);

// adc_log_level_string() - Returns string representation of the given level.
const char *adc_log_level_string(int level);

void adc__log(int level, const char *file, int line, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif // End interface

// Begin Implementation

#ifdef ADC_LOG_IMPLEMENTATION

typedef struct {
  adc_log_log_handler handler;
  void *userdata;
  int level;
} adc_log_callback;

static struct {
  void *userdata;
  adc_log_lock_handler lock_handler;
  int level;
  int num_callbacks;
  adc_log_callback callbacks[ADC_LOG_MAX_CALLBACKS];
} s_logger = {NULL, NULL, ADC_LOG_DEBUG, 0};

static const char *s_level_strings[] = {"DEBUG", "INFO", "WARN", "ERROR"};

#ifdef ADC_LOG_USE_COLOR
#define ADC_LOG_COLOR_RESET "\x1b[0m"
#define ADC_LOG_COLOR_RED "\x1b[31m"
#define ADC_LOG_COLOR_GREEN "\x1b[32m"
#define ADC_LOG_COLOR_YELLOW "\x1b[33m"
#define ADC_LOG_COLOR_CYAN "\x1b[36m"
#define ADC_LOG_COLOR_GRAY "\x1b[90m"

static const char *s_level_colors[] = {ADC_LOG_COLOR_CYAN, ADC_LOG_COLOR_GREEN,
                                       ADC_LOG_COLOR_YELLOW, ADC_LOG_COLOR_RED};
#endif

static void file_callback(adc_log_msg *msg);
static void stderr_callback(adc_log_msg *msg);

void adc_log_set_lock_handler(adc_log_lock_handler handler, void *userdata) {
  s_logger.lock_handler = handler;
  s_logger.userdata = userdata;
}

void adc_log_set_level(int level) { s_logger.level = level; }

int adc_log_add_callback(adc_log_log_handler handler, void *userdata,
                         int level) {
  if (s_logger.num_callbacks >= ADC_LOG_MAX_CALLBACKS)
    return -1;

  adc_log_callback cb;
  cb.handler = handler;
  cb.userdata = userdata;
  cb.level = level;
  s_logger.callbacks[s_logger.num_callbacks++] = cb;
  return 0;
}

int adc_log_add_fp(FILE *fp, int level) {
  return adc_log_add_callback(file_callback, fp, level);
}

const char *adc_log_level_string(int level) {
  if (level >= ADC_LOG_DEBUG && level <= ADC_LOG_ERROR)
    return s_level_strings[level];

  return "";
}

static void init_msg(adc_log_msg *msg, void *userdata) {
  if (!msg->time) {
    time_t t = time(NULL);
    msg->time = localtime(&t);
  }
  msg->userdata = userdata;
}

static void lock() {
  if (s_logger.lock_handler)
    s_logger.lock_handler(1, s_logger.userdata);
}

static void unlock() {
  if (s_logger.lock_handler)
    s_logger.lock_handler(0, s_logger.userdata);
}

void adc__log(int level, const char *file, int line, const char *fmt, ...) {
  adc_log_msg msg;
  msg.fmt = fmt;
  msg.file = file;
  msg.line = line;
  msg.level = level;
  msg.time = NULL;
  msg.userdata = NULL;

  lock();

  if (level >= s_logger.level) {
    init_msg(&msg, stderr);
    va_start(msg.args, fmt);
    stderr_callback(&msg);
    va_end(msg.args);
  }

  for (int i = 0; i < s_logger.num_callbacks; i++) {
    adc_log_callback *cb = &s_logger.callbacks[i];
    if (cb->handler && level >= cb->level) {
      init_msg(&msg, cb->userdata);
      va_start(msg.args, fmt);
      cb->handler(&msg);
      va_end(msg.args);
    }
  }

  unlock();
}

static void file_callback(adc_log_msg *msg) {
  char buf[64];
  buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", msg->time)] = '\0';
  FILE *fp = (FILE *)msg->userdata;

  fprintf(fp, "%s %-5s %s:%d: ", buf, s_level_strings[msg->level], msg->file,
          msg->line);
  vfprintf(fp, msg->fmt, msg->args);
  fprintf(fp, "\n");
  fflush(fp);
}

static void stderr_callback(adc_log_msg *msg) {
  char buf[16];
  buf[strftime(buf, sizeof(buf), "%H:%M:%S", msg->time)] = '\0';
  FILE *fp = (FILE *)msg->userdata;

#ifdef ADC_LOG_USE_COLOR
  fprintf(fp,
          "%s %s%-5s " ADC_LOG_COLOR_RESET ADC_LOG_COLOR_GRAY
          "%s:%d:" ADC_LOG_COLOR_RESET " ",
          buf, s_level_colors[msg->level], s_level_strings[msg->level],
          msg->file, msg->line);
#else
  fprintf(fp, "%s %-5s %s:%d: ", buf, s_level_strings[msg->level], msg->file,
          msg->line);
#endif
  vfprintf(fp, msg->fmt, msg->args);
  fprintf(fp, "\n");
  fflush(fp);
}

#endif // End implementation
