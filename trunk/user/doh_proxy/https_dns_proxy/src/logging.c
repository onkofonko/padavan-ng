#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>         // NOLINT(llvmlibc-restrict-system-libc-headers)
#include <sys/time.h>      // NOLINT(llvmlibc-restrict-system-libc-headers)
#include <unistd.h>        // NOLINT(llvmlibc-restrict-system-libc-headers)

#include "logging.h"
#include "ring_buffer.h"

// logs of this severity or higher are flushed immediately after write
#define LOG_FLUSH_LEVEL LOG_WARNING
// Log line should be at least 100
enum {
LOG_LINE_SIZE = 2048,
LOG_FLIGHT_RECORDER_LIMIT = 10
};

static FILE *logfile = NULL;                // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static int loglevel = LOG_ERROR;            // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static ev_timer logging_timer;              // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static ev_signal sigusr2;                   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static struct ring_buffer flight_recorder;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static const char * const SeverityStr[] = {
  "[D]",
  "[I]",
  "[W]",
  "[E]",
  "[S]",
  "[F]"
};

void logging_timer_cb(struct ev_loop __attribute__((unused)) *loop,
    ev_timer __attribute__((unused)) *w,
    int __attribute__((unused)) revents) {
  if (logfile) {
    (void)fflush(logfile);
  }
}

void logging_flight_recorder_dump(void) {
  ILOG("Flight recorder dump");  // will be also at the end of the dump :)
  ring_buffer_dump(&flight_recorder, logfile);
}

static void logging_flight_recorder_dump_cb(struct ev_loop __attribute__((unused)) *loop,
    ev_signal __attribute__((__unused__)) *w,
    int __attribute__((__unused__)) revents) {
  logging_flight_recorder_dump();
}

void logging_flush_init(struct ev_loop *loop) {
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
  ev_timer_init(&logging_timer, logging_timer_cb, 0, 10);
  /* don't start timer if we will never write messages that are not flushed */
  if (loglevel >= LOG_FLUSH_LEVEL) {
    return;
  }
  DLOG("starting periodic log flush timer");
  ev_timer_start(loop, &logging_timer);

  DLOG("starting SIGUSR2 handler");
  ev_signal_init(&sigusr2, logging_flight_recorder_dump_cb, SIGUSR2);
  ev_signal_start(loop, &sigusr2);
}

void logging_flush_cleanup(struct ev_loop *loop) {
  ev_timer_stop(loop, &logging_timer);
  ev_signal_stop(loop, &sigusr2);
}

void logging_init(int fd, int level) {
  if (logfile) {
    (void)fclose(logfile);
  }
  logfile = fdopen(fd, "a");
  loglevel = level;

  ring_buffer_init(&flight_recorder, LOG_FLIGHT_RECORDER_LIMIT);
}

void logging_cleanup(void) {
  if (logfile) {
    (void)fclose(logfile);
  }
  logfile = NULL;
  ring_buffer_free(&flight_recorder);
}

int logging_debug_enabled(void) {
  return loglevel <= LOG_DEBUG;
}

// NOLINTNEXTLINE(misc-no-recursion) because of severity check
void _log(const char *file, int line, int severity, const char *fmt, ...) {
  if (severity < loglevel && flight_recorder.size == 0) {
    return;
  }
  if (severity < 0 || severity >= LOG_MAX) {
    FLOG("Unknown log severity: %d", severity);
  }
  if (!logfile) {
    logfile = fdopen(STDOUT_FILENO, "w");
  }

  struct timeval tv;
  gettimeofday(&tv, NULL);

  char buff[LOG_LINE_SIZE];
  uint32_t buff_pos = 0;

  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
  int chars = snprintf(buff, LOG_LINE_SIZE, "%s %8"PRIu64".%06"PRIu64" %s:%d ",
                       SeverityStr[severity], (uint64_t)tv.tv_sec, (uint64_t)tv.tv_usec, file, line);
  if (chars < 0 || chars >= LOG_LINE_SIZE/2) {
    abort();  // must be impossible
  }
  buff_pos += chars;

  va_list args;
  va_start(args, fmt);
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
  chars = vsnprintf(buff + buff_pos, LOG_LINE_SIZE - buff_pos, fmt, args);
  va_end(args);

  if (chars < 0) {
    abort();  // must be impossible
  }
  buff_pos += chars;
  if (buff_pos >= LOG_LINE_SIZE) {
    buff_pos = LOG_LINE_SIZE - 1;
    buff[buff_pos - 1] = '$'; // indicate truncation
  }

  ring_buffer_push_back(&flight_recorder, buff, buff_pos);

  if (severity < loglevel) {
    return;
  }

  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
  (void)fprintf(logfile, "%s\n", buff);

  if (severity >= LOG_FLUSH_LEVEL) {
    (void)fflush(logfile);
  }
  if (severity == LOG_FATAL) {
    ring_buffer_dump(&flight_recorder, logfile);
#ifdef DEBUG
    abort();
#else
    exit(1);
#endif
  }
}
