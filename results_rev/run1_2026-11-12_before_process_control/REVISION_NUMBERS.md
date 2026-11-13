# Revision numbers (i7-12700K + RTX 4070, run 1 (before process control))

## A. Session 1: wall clock around the complete call, 81 configurations (i7-12700K + RTX 4070, run 1 (before process control))

### Delivery: fresh  (GPU faster in 49 of 81; median ratio 2.62; range 0.10 to 5.03)
| output cells | conf. | serial flat | threads flat | GPU | ratio median (range) | GPU faster |
|---|---|---|---|---|---|---|
| up to 3,000 | 33 | 0.0609 | 0.178 | 0.163 | 0.36 (0.10 to 1.01) | 1 of 33 |
| 3,000 to 40,000 | 12 | 0.718 | 0.578 | 0.204 | 2.70 (2.12 to 4.10) | 12 of 12 |
| 40,000 to 120,000 | 8 | 3.03 | 1.42 | 0.391 | 3.75 (2.57 to 5.03) | 8 of 8 |
| 120,000 to 400,000 | 10 | 9.97 | 1.98 | 0.706 | 2.82 (2.48 to 3.41) | 10 of 10 |
| 0.4 to 1.2 million | 9 | 59.7 | 8.08 | 2.10 | 3.88 (3.39 to 4.41) | 9 of 9 |
| 1.2 to 4 million | 5 | 158 | 22.7 | 6.45 | 3.61 (3.48 to 4.14) | 5 of 5 |
| 4 to 11 million | 4 | 673 | 95.9 | 23.5 | 4.06 (3.99 to 4.15) | 4 of 4 |
by level: 12: 0.32, 20: 3.20, 28: 3.68
by hops: 1: 0.49, 4: 2.76, 8: 3.78
boundary: median 2.69, range 0.10 to 5.03, GPU faster 16 of 27
buildings: median 2.60, range 0.15 to 4.17, GPU faster 17 of 27
streets: median 2.62, range 0.15 to 4.10, GPU faster 16 of 27
CPU faster: 32 configurations, largest output among them 3000 cells; smallest output with GPU faster 2107
robustness, ratio of the fastest calls (minimum / minimum): median 2.44, range 0.10 to 4.11, GPU faster 50 of 81; for outputs above 4 million cells: 4.06, 3.88, 3.98, 3.96
p95 / median of the calls, median over configurations: serial 1.11, threads 1.53, GPU 1.07
configurations above 3,000 returned cells: 48, smallest output 29977, ratio median 3.53, range 2.12 to 5.03
configurations with max/min call spread above 3 in any implementation: 43 (kept)
routing rule (level >= 20 and budget >= T): best T in [4000, 5000, 6000, 8000, 10000, 15000, 20000, 25000, 30000] with 80 of 81 correct; T=5000 gives 80
held out dataset check: boundary: T=4000, 27 of 27; buildings: T=4000, 26 of 27; streets: T=4000, 27 of 27
point mapping: median 6.611 ms, max 82.516 ms; with mapping added to both sides the ratio has median 1.04, max 2.11

### Delivery: reused  (GPU faster in 48 of 81; median ratio 2.90; range 0.09 to 9.91)
| output cells | conf. | serial flat | threads flat | GPU | ratio median (range) | GPU faster |
|---|---|---|---|---|---|---|
| up to 3,000 | 33 | 0.0573 | 0.179 | 0.158 | 0.35 (0.09 to 0.99) | 0 of 33 |
| 3,000 to 40,000 | 12 | 0.693 | 0.518 | 0.187 | 2.83 (2.15 to 3.92) | 12 of 12 |
| 40,000 to 120,000 | 8 | 2.98 | 1.45 | 0.331 | 4.47 (2.53 to 5.92) | 8 of 8 |
| 120,000 to 400,000 | 10 | 9.05 | 1.45 | 0.345 | 4.27 (3.00 to 6.61) | 10 of 10 |
| 0.4 to 1.2 million | 9 | 57.2 | 6.64 | 0.979 | 6.73 (4.62 to 9.91) | 9 of 9 |
| 1.2 to 4 million | 5 | 152 | 18.3 | 3.14 | 5.83 (5.71 to 6.47) | 5 of 5 |
| 4 to 11 million | 4 | 664 | 81.9 | 12.2 | 6.67 (6.48 to 6.96) | 4 of 4 |
by level: 12: 0.31, 20: 4.21, 28: 4.43
by hops: 1: 0.46, 4: 3.79, 8: 4.84
boundary: median 2.53, range 0.09 to 7.01, GPU faster 16 of 27
buildings: median 3.12, range 0.15 to 9.91, GPU faster 16 of 27
streets: median 3.00, range 0.15 to 7.66, GPU faster 16 of 27
CPU faster: 33 configurations, largest output among them 3000 cells; smallest output with GPU faster 29977
robustness, ratio of the fastest calls (minimum / minimum): median 2.68, range 0.10 to 6.98, GPU faster 50 of 81; for outputs above 4 million cells: 6.39, 6.21, 6.46, 6.20
p95 / median of the calls, median over configurations: serial 1.12, threads 1.43, GPU 1.06
configurations above 3,000 returned cells: 48, smallest output 29977, ratio median 4.47, range 2.15 to 9.91
configurations with max/min call spread above 3 in any implementation: 34 (kept)
routing rule (level >= 20 and budget >= T): best T in [4000, 5000, 6000, 8000, 10000, 15000, 20000, 25000, 30000] with 81 of 81 correct; T=5000 gives 81
held out dataset check: boundary: T=4000, 27 of 27; buildings: T=4000, 27 of 27; streets: T=4000, 27 of 27
point mapping: median 6.611 ms, max 82.516 ms; with mapping added to both sides the ratio has median 1.03, max 2.29

## A. Session 2: wall clock around the complete call, 81 configurations (i7-12700K + RTX 4070, run 1 (before process control))

### Delivery: fresh  (GPU faster in 49 of 81; median ratio 2.45; range 0.10 to 16.51)
| output cells | conf. | serial flat | threads flat | GPU | ratio median (range) | GPU faster |
|---|---|---|---|---|---|---|
| up to 3,000 | 33 | 0.0597 | 0.181 | 0.161 | 0.35 (0.10 to 1.02) | 1 of 33 |
| 3,000 to 40,000 | 12 | 0.741 | 0.529 | 0.193 | 2.58 (1.96 to 3.44) | 12 of 12 |
| 40,000 to 120,000 | 8 | 3.11 | 1.46 | 0.375 | 3.91 (2.33 to 6.27) | 8 of 8 |
| 120,000 to 400,000 | 10 | 9.68 | 1.89 | 0.724 | 2.62 (2.36 to 3.18) | 10 of 10 |
| 0.4 to 1.2 million | 9 | 58.4 | 9.03 | 2.12 | 4.16 (2.86 to 16.51) | 9 of 9 |
| 1.2 to 4 million | 5 | 169 | 24.7 | 6.34 | 3.68 (3.44 to 11.47) | 5 of 5 |
| 4 to 11 million | 4 | 681 | 205 | 25.6 | 7.84 (4.07 to 14.94) | 4 of 4 |
by level: 12: 0.32, 20: 2.96, 28: 2.93
by hops: 1: 0.49, 4: 2.72, 8: 4.07
boundary: median 2.45, range 0.10 to 16.51, GPU faster 16 of 27
buildings: median 2.58, range 0.13 to 4.27, GPU faster 17 of 27
streets: median 2.39, range 0.16 to 14.94, GPU faster 16 of 27
CPU faster: 32 configurations, largest output among them 3000 cells; smallest output with GPU faster 2107
robustness, ratio of the fastest calls (minimum / minimum): median 2.36, range 0.11 to 13.99, GPU faster 50 of 81; for outputs above 4 million cells: 4.00, 4.05, 11.83, 13.99
p95 / median of the calls, median over configurations: serial 1.14, threads 1.44, GPU 1.07
configurations above 3,000 returned cells: 48, smallest output 29977, ratio median 3.21, range 1.96 to 16.51
configurations with max/min call spread above 3 in any implementation: 28 (kept)
routing rule (level >= 20 and budget >= T): best T in [4000, 5000, 6000, 8000, 10000, 15000, 20000, 25000, 30000] with 80 of 81 correct; T=5000 gives 80
held out dataset check: boundary: T=4000, 27 of 27; buildings: T=4000, 26 of 27; streets: T=4000, 27 of 27
point mapping: median 6.416 ms, max 81.618 ms; with mapping added to both sides the ratio has median 1.03, max 4.49

### Delivery: reused  (GPU faster in 49 of 81; median ratio 2.76; range 0.11 to 43.78)
| output cells | conf. | serial flat | threads flat | GPU | ratio median (range) | GPU faster |
|---|---|---|---|---|---|---|
| up to 3,000 | 33 | 0.0565 | 0.214 | 0.151 | 0.35 (0.11 to 1.03) | 1 of 33 |
| 3,000 to 40,000 | 12 | 0.693 | 0.454 | 0.179 | 2.56 (2.08 to 3.73) | 12 of 12 |
| 40,000 to 120,000 | 8 | 2.99 | 1.37 | 0.323 | 4.34 (2.60 to 4.47) | 8 of 8 |
| 120,000 to 400,000 | 10 | 8.81 | 1.44 | 0.336 | 4.39 (3.46 to 7.43) | 10 of 10 |
| 0.4 to 1.2 million | 9 | 56.5 | 7.53 | 0.914 | 8.00 (4.67 to 43.78) | 9 of 9 |
| 1.2 to 4 million | 5 | 164 | 21.5 | 3.11 | 6.47 (5.52 to 24.97) | 5 of 5 |
| 4 to 11 million | 4 | 673 | 198 | 12.3 | 17.26 (6.66 to 27.78) | 4 of 4 |
by level: 12: 0.29, 20: 4.34, 28: 4.39
by hops: 1: 0.47, 4: 3.38, 8: 4.46
boundary: median 2.60, range 0.11 to 43.78, GPU faster 16 of 27
buildings: median 3.38, range 0.14 to 8.66, GPU faster 17 of 27
streets: median 2.83, range 0.17 to 28.30, GPU faster 16 of 27
CPU faster: 32 configurations, largest output among them 3000 cells; smallest output with GPU faster 1707
robustness, ratio of the fastest calls (minimum / minimum): median 2.55, range 0.11 to 36.95, GPU faster 49 of 81; for outputs above 4 million cells: 6.36, 6.38, 26.51, 25.51
p95 / median of the calls, median over configurations: serial 1.12, threads 1.40, GPU 1.05
configurations above 3,000 returned cells: 48, smallest output 29977, ratio median 4.43, range 2.08 to 43.78
configurations with max/min call spread above 3 in any implementation: 33 (kept)
routing rule (level >= 20 and budget >= T): best T in [4000, 5000, 6000, 8000, 10000, 15000, 20000, 25000, 30000] with 80 of 81 correct; T=5000 gives 80
held out dataset check: boundary: T=4000, 27 of 27; buildings: T=4000, 26 of 27; streets: T=4000, 27 of 27
point mapping: median 6.416 ms, max 81.618 ms; with mapping added to both sides the ratio has median 1.03, max 5.02

## A. Session 3: wall clock around the complete call, 81 configurations (i7-12700K + RTX 4070, run 1 (before process control))

### Delivery: fresh  (GPU faster in 49 of 81; median ratio 2.43; range 0.10 to 16.46)
| output cells | conf. | serial flat | threads flat | GPU | ratio median (range) | GPU faster |
|---|---|---|---|---|---|---|
| up to 3,000 | 33 | 0.0597 | 0.249 | 0.159 | 0.37 (0.10 to 1.11) | 1 of 33 |
| 3,000 to 40,000 | 12 | 0.711 | 0.522 | 0.193 | 2.70 (1.88 to 3.58) | 12 of 12 |
| 40,000 to 120,000 | 8 | 3.03 | 1.50 | 0.384 | 3.88 (2.31 to 8.52) | 8 of 8 |
| 120,000 to 400,000 | 10 | 9.81 | 1.88 | 0.697 | 2.76 (2.39 to 3.19) | 10 of 10 |
| 0.4 to 1.2 million | 9 | 57.4 | 8.03 | 2.10 | 4.25 (2.91 to 14.41) | 9 of 9 |
| 1.2 to 4 million | 5 | 253 | 23.4 | 6.63 | 3.96 (3.49 to 10.64) | 5 of 5 |
| 4 to 11 million | 4 | 1075 | 340 | 25.5 | 13.81 (12.04 to 16.46) | 4 of 4 |
by level: 12: 0.34, 20: 2.92, 28: 3.00
by hops: 1: 0.48, 4: 2.77, 8: 3.89
boundary: median 2.50, range 0.10 to 5.21, GPU faster 16 of 27
buildings: median 2.43, range 0.12 to 16.46, GPU faster 17 of 27
streets: median 2.39, range 0.12 to 15.47, GPU faster 16 of 27
CPU faster: 32 configurations, largest output among them 3000 cells; smallest output with GPU faster 2107
robustness, ratio of the fastest calls (minimum / minimum): median 2.34, range 0.11 to 15.59, GPU faster 50 of 81; for outputs above 4 million cells: 12.33, 15.59, 15.03, 12.16
p95 / median of the calls, median over configurations: serial 1.10, threads 1.42, GPU 1.07
configurations above 3,000 returned cells: 48, smallest output 29977, ratio median 3.22, range 1.88 to 16.46
configurations with max/min call spread above 3 in any implementation: 35 (kept)
routing rule (level >= 20 and budget >= T): best T in [4000, 5000, 6000, 8000, 10000, 15000, 20000, 25000, 30000] with 80 of 81 correct; T=5000 gives 80
held out dataset check: boundary: T=4000, 27 of 27; buildings: T=4000, 26 of 27; streets: T=4000, 27 of 27
point mapping: median 6.408 ms, max 76.518 ms; with mapping added to both sides the ratio has median 1.03, max 4.86

### Delivery: reused  (GPU faster in 49 of 81; median ratio 2.97; range 0.10 to 37.09)
| output cells | conf. | serial flat | threads flat | GPU | ratio median (range) | GPU faster |
|---|---|---|---|---|---|---|
| up to 3,000 | 33 | 0.0597 | 0.284 | 0.149 | 0.37 (0.10 to 1.00) | 1 of 33 |
| 3,000 to 40,000 | 12 | 0.68 | 0.586 | 0.175 | 3.03 (2.14 to 3.88) | 12 of 12 |
| 40,000 to 120,000 | 8 | 2.93 | 1.31 | 0.332 | 4.04 (2.72 to 9.61) | 8 of 8 |
| 120,000 to 400,000 | 10 | 8.69 | 1.44 | 0.335 | 4.27 (3.52 to 10.28) | 10 of 10 |
| 0.4 to 1.2 million | 9 | 58.3 | 6.32 | 0.939 | 6.83 (4.57 to 37.09) | 9 of 9 |
| 1.2 to 4 million | 5 | 238 | 18.6 | 3.11 | 6.30 (5.62 to 23.34) | 5 of 5 |
| 4 to 11 million | 4 | 1073 | 334 | 12.2 | 28.41 (25.38 to 28.94) | 4 of 4 |
by level: 12: 0.32, 20: 3.95, 28: 4.19
by hops: 1: 0.48, 4: 3.64, 8: 4.28
boundary: median 2.79, range 0.10 to 10.28, GPU faster 16 of 27
buildings: median 3.08, range 0.11 to 37.09, GPU faster 17 of 27
streets: median 3.57, range 0.18 to 36.04, GPU faster 16 of 27
CPU faster: 32 configurations, largest output among them 3000 cells; smallest output with GPU faster 2107
robustness, ratio of the fastest calls (minimum / minimum): median 2.55, range 0.11 to 31.69, GPU faster 50 of 81; for outputs above 4 million cells: 28.34, 27.70, 26.93, 25.41
p95 / median of the calls, median over configurations: serial 1.10, threads 1.39, GPU 1.07
configurations above 3,000 returned cells: 48, smallest output 29977, ratio median 4.29, range 2.14 to 37.09
configurations with max/min call spread above 3 in any implementation: 34 (kept)
routing rule (level >= 20 and budget >= T): best T in [4000, 5000, 6000, 8000, 10000, 15000, 20000, 25000, 30000] with 80 of 81 correct; T=5000 gives 80
held out dataset check: boundary: T=4000, 27 of 27; buildings: T=4000, 26 of 27; streets: T=4000, 27 of 27
point mapping: median 6.408 ms, max 76.518 ms; with mapping added to both sides the ratio has median 1.03, max 5.78

### Session 1: representative queries (median of 30 calls, ms; ratio = best CPU / GPU with 95% bootstrap interval)
| query | delivery | output | serial flat | threads flat | GPU | ratio [95% interval] |
|---|---|---|---|---|---|---|
| buildings L28 100000 seeds 8 hops | fresh | 10800000 | 733 | 102 | 25.7 | 3.99 [3.89, 4.07] |
| buildings L28 100000 seeds 8 hops | reused | 10800000 | 712 | 86.1 | 13.3 | 6.48 [6.42, 6.56] |
| streets L20 99657 seeds 8 hops | fresh | 9010953 | 609 | 87.0 | 21.2 | 4.10 [3.99, 4.17] |
| streets L20 99657 seeds 8 hops | reused | 9010953 | 595 | 74.5 | 10.9 | 6.86 [6.70, 7.10] |
| buildings L28 10000 seeds 8 hops | fresh | 1080000 | 63.5 | 8.03 | 2.16 | 3.71 [3.63, 3.83] |
| buildings L28 10000 seeds 8 hops | reused | 1080000 | 60.3 | 6.64 | 0.988 | 6.73 [6.44, 6.87] |
| boundary L28 19717 seeds 8 hops | fresh | 2129436 | 128 | 19.8 | 4.79 | 4.14 [4.00, 4.40] |
| boundary L28 19717 seeds 8 hops | reused | 2129436 | 123 | 16.0 | 2.48 | 6.47 [6.37, 6.51] |
| buildings L12 1914 seeds 4 hops | fresh | 1046 | 0.0714 | 1.72 | 0.173 | 0.41 [0.40, 0.43] |
| buildings L12 1914 seeds 4 hops | reused | 1046 | 0.0719 | 0.769 | 0.163 | 0.44 [0.44, 0.45] |
| boundary L12 287 seeds 1 hops | fresh | 312 | 0.0169 | 0.0972 | 0.146 | 0.12 [0.10, 0.12] |
| boundary L12 287 seeds 1 hops | reused | 312 | 0.0131 | 0.0915 | 0.142 | 0.09 [0.09, 0.09] |

### Session 2: representative queries (median of 30 calls, ms; ratio = best CPU / GPU with 95% bootstrap interval)
| query | delivery | output | serial flat | threads flat | GPU | ratio [95% interval] |
|---|---|---|---|---|---|---|
| buildings L28 100000 seeds 8 hops | fresh | 10800000 | 740 | 111 | 26.1 | 4.27 [4.17, 4.39] |
| buildings L28 100000 seeds 8 hops | reused | 10800000 | 725 | 90.6 | 13.4 | 6.75 [6.56, 6.90] |
| streets L20 99657 seeds 8 hops | fresh | 9010953 | 607 | 298 | 26.1 | 11.41 [11.15, 11.64] |
| streets L20 99657 seeds 8 hops | reused | 9010953 | 595 | 305 | 11.0 | 27.78 [26.43, 28.20] |
| buildings L28 10000 seeds 8 hops | fresh | 1080000 | 63.6 | 9.04 | 2.21 | 4.10 [3.99, 4.24] |
| buildings L28 10000 seeds 8 hops | reused | 1080000 | 60.7 | 6.59 | 1.03 | 6.43 [6.31, 6.67] |
| boundary L28 19717 seeds 8 hops | fresh | 2129436 | 197 | 64.4 | 5.62 | 11.47 [11.32, 11.69] |
| boundary L28 19717 seeds 8 hops | reused | 2129436 | 193 | 61.9 | 2.48 | 24.97 [24.71, 25.21] |
| buildings L12 1914 seeds 4 hops | fresh | 1046 | 0.0694 | 0.272 | 0.162 | 0.43 [0.42, 0.45] |
| buildings L12 1914 seeds 4 hops | reused | 1046 | 0.0688 | 0.274 | 0.145 | 0.48 [0.44, 0.49] |
| boundary L12 287 seeds 1 hops | fresh | 312 | 0.0136 | 0.0707 | 0.132 | 0.10 [0.10, 0.11] |
| boundary L12 287 seeds 1 hops | reused | 312 | 0.0134 | 0.056 | 0.126 | 0.11 [0.10, 0.11] |

### Session 3: representative queries (median of 30 calls, ms; ratio = best CPU / GPU with 95% bootstrap interval)
| query | delivery | output | serial flat | threads flat | GPU | ratio [95% interval] |
|---|---|---|---|---|---|---|
| buildings L28 100000 seeds 8 hops | fresh | 10800000 | 1275 | 413 | 25.1 | 16.46 [16.07, 16.81] |
| buildings L28 100000 seeds 8 hops | reused | 10800000 | 1191 | 378 | 13.2 | 28.64 [28.42, 29.00] |
| streets L20 99657 seeds 8 hops | fresh | 9010953 | 969 | 325 | 21.0 | 15.47 [15.32, 15.64] |
| streets L20 99657 seeds 8 hops | reused | 9010953 | 960 | 305 | 10.8 | 28.17 [27.65, 28.74] |
| buildings L28 10000 seeds 8 hops | fresh | 1080000 | 65.1 | 39.1 | 2.71 | 14.41 [14.13, 14.69] |
| buildings L28 10000 seeds 8 hops | reused | 1080000 | 101 | 38.1 | 1.03 | 37.09 [36.20, 37.72] |
| boundary L28 19717 seeds 8 hops | fresh | 2129436 | 127 | 19.0 | 4.81 | 3.96 [3.91, 4.08] |
| boundary L28 19717 seeds 8 hops | reused | 2129436 | 122 | 15.7 | 2.49 | 6.30 [6.19, 6.41] |
| buildings L12 1914 seeds 4 hops | fresh | 1046 | 0.0736 | 1.27 | 0.159 | 0.46 [0.46, 0.47] |
| buildings L12 1914 seeds 4 hops | reused | 1046 | 0.0734 | 0.778 | 0.162 | 0.45 [0.45, 0.47] |
| boundary L12 287 seeds 1 hops | fresh | 312 | 0.014 | 0.051 | 0.135 | 0.10 [0.10, 0.11] |
| boundary L12 287 seeds 1 hops | reused | 312 | 0.0137 | 0.0517 | 0.134 | 0.10 [0.10, 0.11] |

## D. Controlled GPU variants (ratio of the variant median to the reference median of the same session, query and delivery)

### Delivery: fresh
| variant | buildings L28 100000 seeds 8 hops | streets L20 99657 seeds 8 hops | buildings L28 10000 seeds 8 hops | buildings L12 1914 seeds 4 hops | boundary L12 287 seeds 1 hops |
|---|---|---|---|---|---|
| reference median (ms) | 25.0 | 20.4 | 2.26 | 0.142 | 0.124 |
| kernel_per_ring | 1.03 | 1.03 | 1.15 | 2.33 | 1.54 |
| load_0.25 | 1.03 | 1.04 | 1.10 | 0.95 | 1.07 |
| load_0.7 | 0.99 | 1.00 | 0.99 | 1.61 | 1.03 |
| masked_hash | -- | -- | -- | 47.58 | 4.45 |
| morton_hash | 1.45 | 2.33 | 1.75 | 0.97 | 1.02 |
| no_arrival_skip | 1.05 | 1.05 | 0.96 | 0.94 | 1.01 |
| no_mechanisms | 1.50 | 1.51 | 1.27 | 1.08 | 1.21 |
| no_read_before_cas | 1.12 | 1.15 | 1.01 | 0.93 | 1.70 |
| pageable_transfer | 1.20 | 1.22 | 1.20 | 1.58 | 0.96 |
| preflight_every_call | 1.00 | 1.01 | 0.96 | 1.02 | 1.87 |
| realized_capacity | 1.03 | 1.02 | 1.00 | 1.10 | 0.99 |
| scatter_clear | 1.00 | 1.01 | 0.99 | 1.72 | 1.01 |
| separate_control_clears | 1.00 | 1.01 | 0.97 | 1.03 | 1.14 |
| thread_append | 1.04 | 1.04 | 1.03 | 0.99 | 1.02 |

### Delivery: reused
| variant | buildings L28 100000 seeds 8 hops | streets L20 99657 seeds 8 hops | buildings L28 10000 seeds 8 hops | buildings L12 1914 seeds 4 hops | boundary L12 287 seeds 1 hops |
|---|---|---|---|---|---|
| reference median (ms) | 13.3 | 10.8 | 0.986 | 0.136 | 0.125 |
| kernel_per_ring | 1.06 | 1.05 | 1.37 | 2.44 | 1.52 |
| load_0.25 | 1.05 | 1.07 | 1.33 | 0.98 | 1.00 |
| load_0.7 | 0.99 | 0.96 | 0.97 | 1.63 | 1.01 |
| masked_hash | -- | -- | -- | 49.18 | 4.37 |
| morton_hash | 1.86 | 3.49 | 2.60 | 1.02 | 1.01 |
| no_arrival_skip | 1.10 | 1.10 | 0.99 | 0.98 | 1.00 |
| no_mechanisms | -- | -- | -- | -- | -- |
| no_read_before_cas | 1.25 | 1.28 | 1.16 | 0.97 | 1.66 |
| pageable_transfer | -- | -- | -- | -- | -- |
| preflight_every_call | 1.01 | 1.00 | 1.01 | 1.03 | 1.78 |
| realized_capacity | 1.00 | 1.02 | 1.00 | 1.15 | 0.99 |
| scatter_clear | 1.00 | 1.00 | 1.05 | 1.78 | 1.00 |
| separate_control_clears | 1.01 | 1.00 | 1.01 | 1.08 | 1.13 |
| thread_append | 1.08 | 1.07 | 1.16 | 1.02 | 0.97 |

## B. Representation on one CPU thread (median of 30 calls, ms; ratio to the compact index)

calgary_points level 20, discovered set auto, seeds 99999, output 2914499:
  compact          147.382 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical     308.119 ms  ratio  2.09  adjacency storage 720 bytes
  bim_tuple        196.149 ms  ratio  1.33  adjacency storage 720 bytes
  bim_packed       159.059 ms  ratio  1.08  adjacency storage 720 bytes

calgary_points level 28, discovered set auto, seeds 100000, output 3000000:
  compact          158.248 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical     402.367 ms  ratio  2.54  adjacency storage 720 bytes
  bim_tuple        205.518 ms  ratio  1.30  adjacency storage 720 bytes
  bim_packed       165.344 ms  ratio  1.04  adjacency storage 720 bytes

compact level 10, discovered set auto, seeds 100000, output 3124:
  compact            8.304 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical       9.397 ms  ratio  1.13  adjacency storage 720 bytes
  bim_tuple         16.598 ms  ratio  2.00  adjacency storage 720 bytes
  bim_packed        10.950 ms  ratio  1.32  adjacency storage 720 bytes
  stored             9.488 ms  ratio  1.14  adjacency storage 1,509,949,440 bytes

compact level 16, discovered set auto, seeds 100000, output 3124:
  compact            8.155 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical       9.540 ms  ratio  1.17  adjacency storage 720 bytes
  bim_tuple         16.666 ms  ratio  2.04  adjacency storage 720 bytes
  bim_packed         9.600 ms  ratio  1.18  adjacency storage 720 bytes

compact level 28, discovered set auto, seeds 100000, output 3124:
  compact           10.478 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical      13.288 ms  ratio  1.27  adjacency storage 720 bytes
  bim_tuple         18.467 ms  ratio  1.76  adjacency storage 720 bytes
  bim_packed        10.559 ms  ratio  1.01  adjacency storage 720 bytes

scattered level 10, discovered set auto, seeds 100000, output 2928633:
  compact          145.516 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical     302.534 ms  ratio  2.08  adjacency storage 720 bytes
  bim_tuple        189.613 ms  ratio  1.30  adjacency storage 720 bytes
  bim_packed       149.810 ms  ratio  1.03  adjacency storage 720 bytes
  stored           161.359 ms  ratio  1.11  adjacency storage 1,509,949,440 bytes

scattered level 28, discovered set auto, seeds 100000, output 3000000:
  compact          158.727 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical     398.925 ms  ratio  2.51  adjacency storage 720 bytes
  bim_tuple        205.071 ms  ratio  1.29  adjacency storage 720 bytes
  bim_packed       168.138 ms  ratio  1.06  adjacency storage 720 bytes

calgary_points level 20, discovered set tight, seeds 99999, output 2914499:
  compact          147.574 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical     312.655 ms  ratio  2.12  adjacency storage 720 bytes
  bim_tuple        200.590 ms  ratio  1.36  adjacency storage 720 bytes
  bim_packed       156.524 ms  ratio  1.06  adjacency storage 720 bytes

calgary_points level 28, discovered set tight, seeds 100000, output 3000000:
  compact          159.921 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical     403.812 ms  ratio  2.53  adjacency storage 720 bytes
  bim_tuple        205.131 ms  ratio  1.28  adjacency storage 720 bytes
  bim_packed       166.360 ms  ratio  1.04  adjacency storage 720 bytes

compact level 10, discovered set tight, seeds 100000, output 3124:
  compact            3.337 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical       4.896 ms  ratio  1.47  adjacency storage 720 bytes
  bim_tuple          4.470 ms  ratio  1.34  adjacency storage 720 bytes
  bim_packed         3.393 ms  ratio  1.02  adjacency storage 720 bytes
  stored             2.973 ms  ratio  0.89  adjacency storage 1,509,949,440 bytes

compact level 16, discovered set tight, seeds 100000, output 3124:
  compact            3.391 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical       5.162 ms  ratio  1.52  adjacency storage 720 bytes
  bim_tuple          4.568 ms  ratio  1.35  adjacency storage 720 bytes
  bim_packed         3.516 ms  ratio  1.04  adjacency storage 720 bytes

compact level 28, discovered set tight, seeds 100000, output 3124:
  compact            3.831 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical       7.451 ms  ratio  1.94  adjacency storage 720 bytes
  bim_tuple          5.218 ms  ratio  1.36  adjacency storage 720 bytes
  bim_packed         4.009 ms  ratio  1.05  adjacency storage 720 bytes

scattered level 10, discovered set tight, seeds 100000, output 2928633:
  compact          146.459 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical     302.489 ms  ratio  2.07  adjacency storage 720 bytes
  bim_tuple        192.473 ms  ratio  1.31  adjacency storage 720 bytes
  bim_packed       150.659 ms  ratio  1.03  adjacency storage 720 bytes
  stored           162.777 ms  ratio  1.11  adjacency storage 1,509,949,440 bytes

scattered level 28, discovered set tight, seeds 100000, output 3000000:
  compact          160.301 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical     400.975 ms  ratio  2.50  adjacency storage 720 bytes
  bim_tuple        205.658 ms  ratio  1.28  adjacency storage 720 bytes
  bim_packed       167.251 ms  ratio  1.04  adjacency storage 720 bytes

## C. H3 context (one thread)
  cpu_serial_flat_compact      level/res 12 depth 8 seeds   1914 output      1707 median     0.090 ms      52.8 ns per cell
  h3_grid_disk_union_serial    level/res  8 depth 5 seeds   1424 output      1223 median     1.974 ms    1614.4 ns per cell
  h3_grid_disk_union_serial    level/res  8 depth 6 seeds   1424 output      1424 median     2.721 ms    1911.0 ns per cell
  cpu_serial_flat_compact      level/res 20 depth 8 seeds  99999 output   9380470 median   617.216 ms      65.8 ns per cell
  h3_grid_disk_union_serial    level/res 14 depth 5 seeds  99999 output   8223527 median   335.634 ms      40.8 ns per cell
  h3_grid_disk_union_serial    level/res 14 depth 6 seeds  99999 output  11078224 median   456.614 ms      41.2 ns per cell
  cpu_serial_flat_compact      level/res 28 depth 8 seeds 100000 output  10800000 median   714.113 ms      66.1 ns per cell
  h3_grid_disk_union_serial    level/res 15 depth 6 seeds 100000 output  12525699 median   472.779 ms      37.7 ns per cell

## E. Buffer decision margins
  name=nose_hill_L12, level=12, distance_m=1000, segments=1482, seed_cells=33, tested_cells=136, returned_cells=78, excluded_cells=58, unresolved_cells=0, separating_ring=4, last_ring_cells=28, stopping_margin_m=139.3568691, smallest_margin_excluded_m=3.223374922, smallest_margin_confirmed_m=0.7305198144, below_10um=0, below_1mm=0, below_1cm=0, below_1m=1, delta_m=0.001, returned_plus=78, changed_plus=0, returned_minus=78, changed_minus=0
  name=nose_hill_L14, level=14, distance_m=1000, segments=1482, seed_cells=147, tested_cells=1768, returned_cells=941, excluded_cells=827, unresolved_cells=0, separating_ring=16, last_ring_cells=106, stopping_margin_m=608.0538428, smallest_margin_excluded_m=0.04796947165, smallest_margin_confirmed_m=0.02379725572, below_10um=0, below_1mm=0, below_1cm=0, below_1m=18, delta_m=0.001, returned_plus=941, changed_plus=0, returned_minus=941, changed_minus=0
  name=nose_hill_L16, level=16, distance_m=1000, segments=1482, seed_cells=611, tested_cells=26455, returned_cells=13874, excluded_cells=12581, unresolved_cells=0, separating_ring=64, last_ring_cells=413, stopping_margin_m=707.2112256, smallest_margin_excluded_m=0.004872844639, smallest_margin_confirmed_m=0.002646633246, below_10um=0, below_1mm=0, below_1cm=2, below_1m=144, delta_m=0.001, returned_plus=13874, changed_plus=0, returned_minus=13874, changed_minus=0
  name=point_L20, level=20, distance_m=10, segments=1, seed_cells=3, tested_cells=131, returned_cells=60, excluded_cells=71, unresolved_cells=0, separating_ring=8, last_ring_cells=27, stopping_margin_m=2.180043318, smallest_margin_excluded_m=0.1472476161, smallest_margin_confirmed_m=0.0005096335206, below_10um=0, below_1mm=1, below_1cm=1, below_1m=39, delta_m=0.001, returned_plus=60, changed_plus=0, returned_minus=60, changed_minus=0
  name=point_L28, level=28, distance_m=0.1, segments=1, seed_cells=3, tested_cells=1667, returned_cells=316, excluded_cells=1351, unresolved_cells=0, separating_ring=32, last_ring_cells=99, stopping_margin_m=0.1040175078, smallest_margin_excluded_m=2.109091262e-06, smallest_margin_confirmed_m=2.326822102e-05, below_10um=2, below_1mm=53, below_1cm=175, below_1m=1667, delta_m=0.001, returned_plus=325, changed_plus=9, returned_minus=310, changed_minus=6
  name=dateline_L12, level=12, distance_m=1000, segments=1, seed_cells=4, tested_cells=48, returned_cells=14, excluded_cells=34, unresolved_cells=0, separating_ring=4, last_ring_cells=16, stopping_margin_m=649.1515906, smallest_margin_excluded_m=0.6433060786, smallest_margin_confirmed_m=0.01688962218, below_10um=0, below_1mm=0, below_1cm=0, below_1m=6, delta_m=0.001, returned_plus=14, changed_plus=0, returned_minus=14, changed_minus=0
  name=polar_L12, level=12, distance_m=1000, segments=1, seed_cells=4, tested_cells=48, returned_cells=10, excluded_cells=38, unresolved_cells=0, separating_ring=4, last_ring_cells=16, stopping_margin_m=672.6496251, smallest_margin_excluded_m=0.2481591864, smallest_margin_confirmed_m=21.48463025, below_10um=0, below_1mm=0, below_1cm=0, below_1m=4, delta_m=0.001, returned_plus=10, changed_plus=0, returned_minus=10, changed_minus=0

## F. Global ring validation: 40 cases x 8 implementations = 320 comparisons, all PASS; 1200 ring comparisons; 56160 base mesh edge crossings in the reference; capacity error then retry checks: 4
implementations: cpu_flat_fresh, cpu_flat_reused, cpu_threads_fresh, cpu_threads_reused, gpu_fresh, gpu_kernel_per_ring, gpu_mechanisms_off, gpu_reused
