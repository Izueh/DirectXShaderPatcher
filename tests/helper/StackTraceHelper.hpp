#pragma once

// Debug-only stacktrace capture using C++23 <stacktrace>.
// Only included in Debug builds (#ifndef NDEBUG).

#include <windows.h>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stacktrace>

#ifndef STATUS_STACK_BUFFER_OVERRUN
#define STATUS_STACK_BUFFER_OVERRUN 0xC0000409
#endif

namespace detail {

// Global flag to prevent recursive crashes during stack trace printing.
// Mutable by design: set/cleared via interlocked ops from the crash handler and
// normal code; the non-const-global check is a documented residual.
inline volatile LONG g_in_crash_handler = 0;

// Write to file to ensure output survives crash
inline void WriteToFile(const std::string& msg) {
  static FILE* f = nullptr;
  if (f == nullptr) {
    f = fopen("C:/Users/izueh/source/repos/DirectXShaderPatcher/crash_dump.txt", "w");
  }
  if (f != nullptr) {
    fputs(msg.c_str(), f);
    fflush(f);
  }
}

// Prints the current call stack trace to standard error and file
inline void PrintStackTrace() {
  if (InterlockedCompareExchange(&g_in_crash_handler, 1, 0) != 0) {
    return;
  }

  std::ostringstream oss;
  oss << std::stacktrace::current();
  const std::string trace_str = oss.str();

  const std::string msg = "\n--- Stack Trace ---\n" + trace_str + "\n-------------------\n";

  std::cerr << msg;
  WriteToFile(msg);
  std::cerr.flush();

  InterlockedExchange(&g_in_crash_handler, 0);
}

// Windows SEH exception filter for unhandled exceptions
inline LONG WINAPI CrashExceptionFilter(EXCEPTION_POINTERS* exp) {
  if (InterlockedCompareExchange(&g_in_crash_handler, 1, 0) != 0) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  auto code = exp->ExceptionRecord->ExceptionCode;
  std::string msg = "\nFatal Error: Windows exception 0x" + std::to_string(code) + "\n";

  if (code == STATUS_STACK_BUFFER_OVERRUN) {
    msg += "  -> STATUS_STACK_BUFFER_OVERRUN (debug heap detected buffer overflow)\n";
  } else if (code == EXCEPTION_ACCESS_VIOLATION) {
    msg += "  -> EXCEPTION_ACCESS_VIOLATION\n";
  } else if (code == EXCEPTION_ILLEGAL_INSTRUCTION) {
    msg += "  -> EXCEPTION_ILLEGAL_INSTRUCTION\n";
  } else if (code == EXCEPTION_STACK_OVERFLOW) {
    msg += "  -> EXCEPTION_STACK_OVERFLOW\n";
  }

  std::cerr << msg;
  WriteToFile(msg);

  PrintStackTrace();
  std::cerr.flush();
  WriteToFile("\n");
  if (FILE* f = fopen("C:/Users/izueh/source/repos/DirectXShaderPatcher/crash_dump.txt", "w")) {
    fclose(f);
  }

  return EXCEPTION_CONTINUE_SEARCH;
}

// Handler for unhandled C++ exceptions
inline void HandleUnhandledException() {
  const std::string msg = "Fatal Error: Unhandled C++ exception.\n";
  std::cerr << msg;
  WriteToFile(msg);
  PrintStackTrace();
  std::abort();
}

// Handler for system signals / hardware crashes
inline void HandleSignal(int signal_number) {
  std::string msg = "Fatal Error: Received signal " + std::to_string(signal_number);
  switch (signal_number) {
    case SIGSEGV: msg += " (Segmentation Fault)"; break;
    case SIGFPE:  msg += " (Floating Point Exception)"; break;
    case SIGILL:  msg += " (Illegal Instruction)"; break;
    case SIGABRT: msg += " (Abort)"; break;
    default:      break;
  }
  msg += ".\n";

  std::cerr << msg;
  WriteToFile(msg);

  PrintStackTrace();
  std::_Exit(EXIT_FAILURE);
}

}  // namespace detail

// Install crash handlers (call once at program start)
inline void InstallCrashHandler() {
  std::set_terminate(detail::HandleUnhandledException);
  std::signal(SIGSEGV, detail::HandleSignal);
  std::signal(SIGFPE, detail::HandleSignal);
  std::signal(SIGILL, detail::HandleSignal);
  std::signal(SIGABRT, detail::HandleSignal);

  // Catch Windows SEH exceptions (STATUS_STACK_BUFFER_OVERRUN, etc.)
  SetUnhandledExceptionFilter(detail::CrashExceptionFilter);
}
