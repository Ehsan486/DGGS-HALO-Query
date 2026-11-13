First complete run of the revision measurements (2026-11-12, 15:17 to 15:56 local time), kept for the record.

Protocol: identical to the reported run, except that the timing programs did not yet call
bafe_exp::stabilize_process() (include/bafe_exp/process_control.hpp).

Observation: session 1 is undisturbed. In sessions 2 and 3, several of the long configurations (mostly
those with more than one million returned cells; three configurations in session 2 and eight in session 3
for the one thread implementation) show CPU latencies of about 1.5 to 1.7 times (one thread) and 3.3 to
4.9 times (twenty threads) the session 1 values, while the GPU latencies are unchanged.
Constant factors of this size on a processor with eight performance and four efficiency cores are what is
expected when the operating system schedules the process on the efficiency cores (Windows 11 power
throttling of a process whose console window is not in the foreground).

Consequence: these sessions would overstate the GPU speedup for large halos (up to 28 times instead of
4 to 7 times). They are therefore not reported in the paper. The measurement was repeated with the
process control call; the reported sessions are in the parent folder.
REVISION_NUMBERS.md in this folder is the analysis of this first run.
