// Crash diagnostics: logs access violations and other fatal exceptions raised inside ReFix to ReFix.log,
// with the faulting address and the stack's return addresses as module+offset (resolve them with the
// build's .map file). No debugger or dump file is needed on the user's PC.
#pragma once

// Installs the vectored handler once per process. It only logs and always continues the search, so the
// game's own handlers (and Windows Error Reporting) still see the exception.
void ReFix_InstallCrashLog();
