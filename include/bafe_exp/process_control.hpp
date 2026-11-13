#pragma once
// Measurement control for the timing programs (revision 2026-11).
//
// Windows 11 may place a process whose console window is not in the foreground
// in its background class ("power throttling", EcoQoS). On a hybrid processor
// such a process runs on the efficiency cores at reduced speed. In the first
// run of the revision measurements this changed the CPU times of long
// configurations by factors of 1.5 (one thread) to 4 (twenty threads) in some
// sessions, while the GPU times were unchanged.
//
// stabilize_process() asks the operating system not to apply power throttling
// to this process and raises its priority class. The call is made once, before
// any implementation is constructed, so it applies identically to the serial
// CPU, multithreaded CPU and GPU implementations that run in the process.
// The returned text records what was set and is written to the result rows.
#include <string>

namespace bafe_exp {
std::string stabilize_process();
}
