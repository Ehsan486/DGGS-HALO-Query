# Revision numbers (Intel Core i7-12700K, NVIDIA GeForce RTX 4070, Windows 11)

## A. Session 1: wall clock around the complete call, 81 configurations (Intel Core i7-12700K, NVIDIA GeForce RTX 4070, Windows 11)

### Delivery: fresh  (GPU faster in 52 of 81; median ratio 2.35; range 0.11 to 4.34)
| output cells | conf. | serial flat | threads flat | GPU | ratio median (range) | GPU faster |
|---|---|---|---|---|---|---|
| up to 3,000 | 33 | 0.0665 | 0.154 | 0.16 | 0.42 (0.11 to 1.08) | 4 of 33 |
| 3,000 to 40,000 | 12 | 0.721 | 0.493 | 0.213 | 2.20 (1.39 to 3.06) | 12 of 12 |
| 40,000 to 120,000 | 8 | 3.06 | 1.41 | 0.378 | 3.64 (2.49 to 4.06) | 8 of 8 |
| 120,000 to 400,000 | 10 | 10.6 | 1.87 | 0.705 | 2.69 (2.35 to 2.87) | 10 of 10 |
| 0.4 to 1.2 million | 9 | 57.2 | 7.66 | 2.08 | 3.56 (2.95 to 4.34) | 9 of 9 |
| 1.2 to 4 million | 5 | 156 | 22.4 | 6.31 | 3.43 (3.37 to 3.95) | 5 of 5 |
| 4 to 11 million | 4 | 663 | 91.9 | 23.2 | 4.03 (3.90 to 4.12) | 4 of 4 |
by level: 12: 0.38, 20: 2.87, 28: 2.88
by hops: 1: 0.52, 4: 2.74, 8: 3.60
boundary: median 2.49, range 0.11 to 4.34, GPU faster 16 of 27
buildings: median 2.35, range 0.16 to 4.06, GPU faster 18 of 27
streets: median 2.21, range 0.17 to 4.12, GPU faster 18 of 27
CPU faster: 29 configurations, largest output among them 3000 cells; smallest output with GPU faster 1364
robustness, ratio of the fastest calls (minimum / minimum): median 2.18, range 0.11 to 4.14, GPU faster 52 of 81; for outputs above 4 million cells: 3.98, 3.95, 4.04, 3.89
p95 / median of the calls, median over configurations: serial 1.09, threads 1.16, GPU 1.07
configurations above 3,000 returned cells: 48, smallest output 29977, ratio median 2.96, range 1.39 to 4.34
configurations with max/min call spread above 3 in any implementation: 8 (kept)
routing rule (level >= 20 and budget >= T): best T in [4000, 5000, 6000, 8000, 10000, 15000, 20000, 25000, 30000] with 77 of 81 correct; T=5000 gives 77
held out dataset check: boundary: T=4000, 27 of 27; buildings: T=4000, 25 of 27; streets: T=4000, 25 of 27
point mapping: median 6.381 ms, max 80.904 ms; with mapping added to both sides the ratio has median 1.03, max 2.06

### Delivery: reused  (GPU faster in 51 of 81; median ratio 2.72; range 0.11 to 7.32)
| output cells | conf. | serial flat | threads flat | GPU | ratio median (range) | GPU faster |
|---|---|---|---|---|---|---|
| up to 3,000 | 33 | 0.0633 | 0.164 | 0.149 | 0.41 (0.11 to 1.04) | 3 of 33 |
| 3,000 to 40,000 | 12 | 0.689 | 0.459 | 0.196 | 2.40 (1.61 to 3.19) | 12 of 12 |
| 40,000 to 120,000 | 8 | 2.93 | 1.34 | 0.328 | 4.05 (2.72 to 4.25) | 8 of 8 |
| 120,000 to 400,000 | 10 | 9.09 | 1.39 | 0.336 | 4.28 (3.43 to 4.53) | 10 of 10 |
| 0.4 to 1.2 million | 9 | 54.9 | 5.88 | 0.907 | 6.34 (4.80 to 7.32) | 9 of 9 |
| 1.2 to 4 million | 5 | 153 | 17.6 | 3.12 | 5.61 (5.29 to 6.37) | 5 of 5 |
| 4 to 11 million | 4 | 644 | 76.6 | 12.2 | 6.38 (6.17 to 6.43) | 4 of 4 |
by level: 12: 0.37, 20: 4.02, 28: 4.24
by hops: 1: 0.52, 4: 3.17, 8: 4.24
boundary: median 2.72, range 0.11 to 7.29, GPU faster 16 of 27
buildings: median 3.06, range 0.15 to 7.32, GPU faster 18 of 27
streets: median 2.51, range 0.14 to 6.61, GPU faster 17 of 27
CPU faster: 30 configurations, largest output among them 3000 cells; smallest output with GPU faster 1364
robustness, ratio of the fastest calls (minimum / minimum): median 2.48, range 0.11 to 7.03, GPU faster 52 of 81; for outputs above 4 million cells: 6.28, 6.13, 6.33, 6.14
p95 / median of the calls, median over configurations: serial 1.07, threads 1.16, GPU 1.07
configurations above 3,000 returned cells: 48, smallest output 29977, ratio median 4.25, range 1.61 to 7.32
configurations with max/min call spread above 3 in any implementation: 7 (kept)
routing rule (level >= 20 and budget >= T): best T in [4000, 5000, 6000, 8000, 10000, 15000, 20000, 25000, 30000] with 78 of 81 correct; T=5000 gives 78
held out dataset check: boundary: T=4000, 27 of 27; buildings: T=4000, 25 of 27; streets: T=4000, 26 of 27
point mapping: median 6.381 ms, max 80.904 ms; with mapping added to both sides the ratio has median 1.03, max 1.99

## A. Session 2: wall clock around the complete call, 81 configurations (Intel Core i7-12700K, NVIDIA GeForce RTX 4070, Windows 11)

### Delivery: fresh  (GPU faster in 52 of 81; median ratio 2.36; range 0.11 to 4.35)
| output cells | conf. | serial flat | threads flat | GPU | ratio median (range) | GPU faster |
|---|---|---|---|---|---|---|
| up to 3,000 | 33 | 0.0644 | 0.175 | 0.15 | 0.42 (0.11 to 1.11) | 4 of 33 |
| 3,000 to 40,000 | 12 | 0.729 | 0.481 | 0.197 | 2.58 (1.99 to 3.27) | 12 of 12 |
| 40,000 to 120,000 | 8 | 2.98 | 1.33 | 0.369 | 3.71 (2.33 to 4.07) | 8 of 8 |
| 120,000 to 400,000 | 10 | 9.46 | 1.83 | 0.693 | 2.65 (2.30 to 2.73) | 10 of 10 |
| 0.4 to 1.2 million | 9 | 57.2 | 7.70 | 2.07 | 3.84 (2.92 to 4.35) | 9 of 9 |
| 1.2 to 4 million | 5 | 153 | 21.4 | 6.24 | 3.44 (3.34 to 3.74) | 5 of 5 |
| 4 to 11 million | 4 | 653 | 92.9 | 23.1 | 4.02 (3.96 to 4.08) | 4 of 4 |
by level: 12: 0.41, 20: 3.00, 28: 2.92
by hops: 1: 0.50, 4: 2.73, 8: 3.72
boundary: median 2.33, range 0.11 to 4.35, GPU faster 16 of 27
buildings: median 2.36, range 0.16 to 4.15, GPU faster 18 of 27
streets: median 2.38, range 0.18 to 4.15, GPU faster 18 of 27
CPU faster: 29 configurations, largest output among them 3000 cells; smallest output with GPU faster 1364
robustness, ratio of the fastest calls (minimum / minimum): median 2.32, range 0.11 to 4.22, GPU faster 52 of 81; for outputs above 4 million cells: 3.97, 3.89, 4.00, 4.01
p95 / median of the calls, median over configurations: serial 1.08, threads 1.20, GPU 1.06
configurations above 3,000 returned cells: 48, smallest output 29977, ratio median 3.14, range 1.99 to 4.35
configurations with max/min call spread above 3 in any implementation: 5 (kept)
routing rule (level >= 20 and budget >= T): best T in [4000, 5000, 6000, 8000, 10000, 15000, 20000, 25000, 30000] with 77 of 81 correct; T=5000 gives 77
held out dataset check: boundary: T=4000, 27 of 27; buildings: T=4000, 25 of 27; streets: T=4000, 25 of 27
point mapping: median 6.241 ms, max 78.639 ms; with mapping added to both sides the ratio has median 1.03, max 2.08

### Delivery: reused  (GPU faster in 49 of 81; median ratio 2.74; range 0.11 to 7.17)
| output cells | conf. | serial flat | threads flat | GPU | ratio median (range) | GPU faster |
|---|---|---|---|---|---|---|
| up to 3,000 | 33 | 0.0632 | 0.182 | 0.145 | 0.41 (0.11 to 1.03) | 1 of 33 |
| 3,000 to 40,000 | 12 | 0.684 | 0.442 | 0.173 | 2.55 (2.06 to 3.18) | 12 of 12 |
| 40,000 to 120,000 | 8 | 2.95 | 1.31 | 0.32 | 4.04 (2.55 to 4.33) | 8 of 8 |
| 120,000 to 400,000 | 10 | 8.77 | 1.37 | 0.337 | 4.24 (3.42 to 4.57) | 10 of 10 |
| 0.4 to 1.2 million | 9 | 54.3 | 6.08 | 0.899 | 6.56 (4.64 to 7.17) | 9 of 9 |
| 1.2 to 4 million | 5 | 144 | 17.1 | 3.04 | 5.61 (5.34 to 5.76) | 5 of 5 |
| 4 to 11 million | 4 | 637 | 76.5 | 12.2 | 6.32 (6.18 to 6.38) | 4 of 4 |
by level: 12: 0.39, 20: 4.03, 28: 4.10
by hops: 1: 0.53, 4: 3.06, 8: 4.15
boundary: median 2.55, range 0.11 to 7.17, GPU faster 16 of 27
buildings: median 2.84, range 0.17 to 6.56, GPU faster 17 of 27
streets: median 2.80, range 0.18 to 6.76, GPU faster 16 of 27
CPU faster: 32 configurations, largest output among them 3000 cells; smallest output with GPU faster 2107
robustness, ratio of the fastest calls (minimum / minimum): median 2.50, range 0.11 to 7.00, GPU faster 51 of 81; for outputs above 4 million cells: 6.27, 6.14, 6.35, 6.11
p95 / median of the calls, median over configurations: serial 1.06, threads 1.13, GPU 1.05
configurations above 3,000 returned cells: 48, smallest output 29977, ratio median 4.24, range 2.06 to 7.17
configurations with max/min call spread above 3 in any implementation: 7 (kept)
routing rule (level >= 20 and budget >= T): best T in [4000, 5000, 6000, 8000, 10000, 15000, 20000, 25000, 30000] with 80 of 81 correct; T=5000 gives 80
held out dataset check: boundary: T=4000, 27 of 27; buildings: T=4000, 26 of 27; streets: T=4000, 27 of 27
point mapping: median 6.241 ms, max 78.639 ms; with mapping added to both sides the ratio has median 1.03, max 2.05

## A. Session 3: wall clock around the complete call, 81 configurations (Intel Core i7-12700K, NVIDIA GeForce RTX 4070, Windows 11)

### Delivery: fresh  (GPU faster in 51 of 81; median ratio 2.43; range 0.10 to 4.33)
| output cells | conf. | serial flat | threads flat | GPU | ratio median (range) | GPU faster |
|---|---|---|---|---|---|---|
| up to 3,000 | 33 | 0.0663 | 0.171 | 0.151 | 0.44 (0.10 to 1.13) | 3 of 33 |
| 3,000 to 40,000 | 12 | 0.734 | 0.521 | 0.201 | 2.66 (2.00 to 3.08) | 12 of 12 |
| 40,000 to 120,000 | 8 | 3.08 | 1.39 | 0.375 | 3.70 (2.42 to 4.31) | 8 of 8 |
| 120,000 to 400,000 | 10 | 9.78 | 1.82 | 0.697 | 2.64 (2.31 to 2.81) | 10 of 10 |
| 0.4 to 1.2 million | 9 | 57.3 | 7.63 | 2.09 | 3.72 (2.83 to 4.33) | 9 of 9 |
| 1.2 to 4 million | 5 | 153 | 21.9 | 6.25 | 3.50 (3.32 to 3.90) | 5 of 5 |
| 4 to 11 million | 4 | 653 | 91.7 | 23.2 | 3.99 (3.89 to 4.18) | 4 of 4 |
by level: 12: 0.41, 20: 2.81, 28: 2.85
by hops: 1: 0.53, 4: 2.73, 8: 3.72
boundary: median 2.42, range 0.10 to 4.33, GPU faster 16 of 27
buildings: median 2.41, range 0.18 to 4.02, GPU faster 18 of 27
streets: median 2.54, range 0.19 to 4.31, GPU faster 17 of 27
CPU faster: 30 configurations, largest output among them 3000 cells; smallest output with GPU faster 1477
robustness, ratio of the fastest calls (minimum / minimum): median 2.26, range 0.11 to 4.08, GPU faster 51 of 81; for outputs above 4 million cells: 3.94, 3.96, 4.05, 3.91
p95 / median of the calls, median over configurations: serial 1.09, threads 1.14, GPU 1.06
configurations above 3,000 returned cells: 48, smallest output 29977, ratio median 3.04, range 2.00 to 4.33
configurations with max/min call spread above 3 in any implementation: 6 (kept)
routing rule (level >= 20 and budget >= T): best T in [4000, 5000, 6000, 8000, 10000, 15000, 20000, 25000, 30000] with 78 of 81 correct; T=5000 gives 78
held out dataset check: boundary: T=4000, 27 of 27; buildings: T=4000, 25 of 27; streets: T=4000, 26 of 27
point mapping: median 6.278 ms, max 75.418 ms; with mapping added to both sides the ratio has median 1.03, max 2.14

### Delivery: reused  (GPU faster in 51 of 81; median ratio 2.53; range 0.11 to 7.42)
| output cells | conf. | serial flat | threads flat | GPU | ratio median (range) | GPU faster |
|---|---|---|---|---|---|---|
| up to 3,000 | 33 | 0.0627 | 0.182 | 0.148 | 0.41 (0.11 to 1.10) | 3 of 33 |
| 3,000 to 40,000 | 12 | 0.705 | 0.464 | 0.179 | 2.56 (2.05 to 3.22) | 12 of 12 |
| 40,000 to 120,000 | 8 | 2.92 | 1.29 | 0.327 | 3.94 (2.47 to 4.42) | 8 of 8 |
| 120,000 to 400,000 | 10 | 8.88 | 1.38 | 0.337 | 4.23 (3.49 to 4.49) | 10 of 10 |
| 0.4 to 1.2 million | 9 | 55.4 | 5.93 | 0.912 | 6.44 (4.73 to 7.42) | 9 of 9 |
| 1.2 to 4 million | 5 | 148 | 17.4 | 3.07 | 5.66 (5.34 to 6.31) | 5 of 5 |
| 4 to 11 million | 4 | 638 | 76.2 | 12.2 | 6.29 (6.19 to 6.54) | 4 of 4 |
by level: 12: 0.36, 20: 3.93, 28: 4.29
by hops: 1: 0.51, 4: 3.06, 8: 4.32
boundary: median 2.47, range 0.11 to 7.42, GPU faster 16 of 27
buildings: median 2.90, range 0.19 to 6.74, GPU faster 18 of 27
streets: median 2.85, range 0.18 to 7.11, GPU faster 17 of 27
CPU faster: 30 configurations, largest output among them 3000 cells; smallest output with GPU faster 1364
robustness, ratio of the fastest calls (minimum / minimum): median 2.39, range 0.11 to 7.10, GPU faster 52 of 81; for outputs above 4 million cells: 6.27, 6.12, 6.34, 6.13
p95 / median of the calls, median over configurations: serial 1.08, threads 1.15, GPU 1.06
configurations above 3,000 returned cells: 48, smallest output 29977, ratio median 4.30, range 2.05 to 7.42
configurations with max/min call spread above 3 in any implementation: 3 (kept)
routing rule (level >= 20 and budget >= T): best T in [4000, 5000, 6000, 8000, 10000, 15000, 20000, 25000, 30000] with 78 of 81 correct; T=5000 gives 78
held out dataset check: boundary: T=4000, 27 of 27; buildings: T=4000, 25 of 27; streets: T=4000, 26 of 27
point mapping: median 6.278 ms, max 75.418 ms; with mapping added to both sides the ratio has median 1.03, max 2.05

### Session 1: representative queries (median of 30 calls, ms; ratio = best CPU / GPU with 95% bootstrap interval)
| query | delivery | output | serial flat | threads flat | GPU | ratio [95% interval] |
|---|---|---|---|---|---|---|
| buildings L28 100000 seeds 8 hops | fresh | 10800000 | 704 | 100 | 25.0 | 4.02 [4.00, 4.04] |
| buildings L28 100000 seeds 8 hops | reused | 10800000 | 687 | 81.5 | 13.2 | 6.17 [6.15, 6.20] |
| streets L20 99657 seeds 8 hops | fresh | 9010953 | 591 | 85.0 | 20.6 | 4.12 [4.09, 4.15] |
| streets L20 99657 seeds 8 hops | reused | 9010953 | 581 | 69.3 | 10.8 | 6.43 [6.39, 6.47] |
| buildings L28 10000 seeds 8 hops | fresh | 1080000 | 62.9 | 7.66 | 2.16 | 3.54 [3.48, 3.60] |
| buildings L28 10000 seeds 8 hops | reused | 1080000 | 60.3 | 5.97 | 1.02 | 5.84 [5.73, 5.99] |
| boundary L28 19717 seeds 8 hops | fresh | 2129436 | 126 | 18.8 | 4.75 | 3.95 [3.91, 3.99] |
| boundary L28 19717 seeds 8 hops | reused | 2129436 | 121 | 15.5 | 2.43 | 6.37 [6.30, 6.43] |
| buildings L12 1914 seeds 4 hops | fresh | 1046 | 0.0842 | 0.221 | 0.172 | 0.49 [0.47, 0.52] |
| buildings L12 1914 seeds 4 hops | reused | 1046 | 0.0803 | 0.228 | 0.163 | 0.49 [0.49, 0.51] |
| boundary L12 287 seeds 1 hops | fresh | 312 | 0.0146 | 0.0432 | 0.134 | 0.11 [0.10, 0.13] |
| boundary L12 287 seeds 1 hops | reused | 312 | 0.0138 | 0.0427 | 0.123 | 0.11 [0.11, 0.13] |

### Session 2: representative queries (median of 30 calls, ms; ratio = best CPU / GPU with 95% bootstrap interval)
| query | delivery | output | serial flat | threads flat | GPU | ratio [95% interval] |
|---|---|---|---|---|---|---|
| buildings L28 100000 seeds 8 hops | fresh | 10800000 | 703 | 98.6 | 24.9 | 3.96 [3.92, 3.98] |
| buildings L28 100000 seeds 8 hops | reused | 10800000 | 688 | 82.0 | 13.3 | 6.18 [6.16, 6.27] |
| streets L20 99657 seeds 8 hops | fresh | 9010953 | 588 | 82.0 | 20.4 | 4.03 [4.02, 4.05] |
| streets L20 99657 seeds 8 hops | reused | 9010953 | 573 | 68.7 | 10.8 | 6.38 [6.35, 6.41] |
| buildings L28 10000 seeds 8 hops | fresh | 1080000 | 61.2 | 9.05 | 2.18 | 4.15 [4.06, 4.24] |
| buildings L28 10000 seeds 8 hops | reused | 1080000 | 58.4 | 6.46 | 0.99 | 6.53 [6.47, 6.61] |
| boundary L28 19717 seeds 8 hops | fresh | 2129436 | 126 | 17.5 | 4.67 | 3.74 [3.71, 3.77] |
| boundary L28 19717 seeds 8 hops | reused | 2129436 | 121 | 14.3 | 2.49 | 5.76 [5.71, 5.80] |
| buildings L12 1914 seeds 4 hops | fresh | 1046 | 0.0778 | 0.255 | 0.15 | 0.52 [0.51, 0.54] |
| buildings L12 1914 seeds 4 hops | reused | 1046 | 0.0766 | 0.197 | 0.143 | 0.54 [0.52, 0.55] |
| boundary L12 287 seeds 1 hops | fresh | 312 | 0.0144 | 0.0614 | 0.128 | 0.11 [0.11, 0.11] |
| boundary L12 287 seeds 1 hops | reused | 312 | 0.0141 | 0.0727 | 0.126 | 0.11 [0.11, 0.11] |

### Session 3: representative queries (median of 30 calls, ms; ratio = best CPU / GPU with 95% bootstrap interval)
| query | delivery | output | serial flat | threads flat | GPU | ratio [95% interval] |
|---|---|---|---|---|---|---|
| buildings L28 100000 seeds 8 hops | fresh | 10800000 | 702 | 101 | 25.1 | 4.02 [3.98, 4.04] |
| buildings L28 100000 seeds 8 hops | reused | 10800000 | 688 | 82.2 | 13.2 | 6.22 [6.19, 6.25] |
| streets L20 99657 seeds 8 hops | fresh | 9010953 | 586 | 86.0 | 20.6 | 4.18 [4.13, 4.20] |
| streets L20 99657 seeds 8 hops | reused | 9010953 | 571 | 70.6 | 10.8 | 6.54 [6.47, 6.60] |
| buildings L28 10000 seeds 8 hops | fresh | 1080000 | 61.0 | 7.63 | 2.24 | 3.41 [3.38, 3.46] |
| buildings L28 10000 seeds 8 hops | reused | 1080000 | 58.6 | 5.89 | 1.03 | 5.72 [5.68, 5.77] |
| boundary L28 19717 seeds 8 hops | fresh | 2129436 | 127 | 18.6 | 4.75 | 3.90 [3.87, 4.01] |
| boundary L28 19717 seeds 8 hops | reused | 2129436 | 122 | 15.3 | 2.43 | 6.31 [6.21, 6.36] |
| buildings L12 1914 seeds 4 hops | fresh | 1046 | 0.0801 | 0.249 | 0.159 | 0.50 [0.49, 0.52] |
| buildings L12 1914 seeds 4 hops | reused | 1046 | 0.0784 | 0.241 | 0.155 | 0.50 [0.48, 0.53] |
| boundary L12 287 seeds 1 hops | fresh | 312 | 0.0141 | 0.0531 | 0.132 | 0.11 [0.10, 0.11] |
| boundary L12 287 seeds 1 hops | reused | 312 | 0.0135 | 0.0525 | 0.124 | 0.11 [0.11, 0.11] |

## D. Controlled GPU variants (ratio of the variant median to the reference median of the same session, query and delivery)

### Delivery: fresh
| variant | buildings L28 100000 seeds 8 hops | streets L20 99657 seeds 8 hops | buildings L28 10000 seeds 8 hops | buildings L12 1914 seeds 4 hops | boundary L12 287 seeds 1 hops |
|---|---|---|---|---|---|
| reference median (ms) | 24.9 | 20.3 | 2.20 | 0.134 | 0.121 |
| kernel_per_ring | 1.04 | 1.03 | 1.14 | 2.41 | 1.55 |
| load_0.25 | 1.02 | 1.05 | 1.14 | 1.00 | 1.03 |
| load_0.7 | 0.99 | 0.99 | 0.97 | 1.01 | 1.04 |
| masked_hash | -- | -- | -- | 50.22 | 4.53 |
| morton_hash | 1.46 | 2.31 | 1.82 | 1.02 | 1.05 |
| no_arrival_skip | 1.06 | 1.07 | 0.98 | 1.67 | 1.04 |
| no_mechanisms | 1.51 | 1.51 | 1.30 | 1.14 | 1.20 |
| no_read_before_cas | 1.14 | 1.17 | 1.06 | 1.67 | 1.03 |
| pageable_transfer | 1.24 | 1.25 | 1.23 | 1.69 | 1.00 |
| preflight_every_call | 1.00 | 1.02 | 1.01 | 1.12 | 1.07 |
| realized_capacity | 1.00 | 1.02 | 0.99 | 1.20 | 1.02 |
| scatter_clear | 1.00 | 1.01 | 1.02 | 1.08 | 1.06 |
| separate_control_clears | 1.00 | 1.02 | 1.02 | 1.08 | 1.15 |
| thread_append | 1.04 | 1.06 | 1.06 | 1.04 | 1.06 |

### Delivery: reused
| variant | buildings L28 100000 seeds 8 hops | streets L20 99657 seeds 8 hops | buildings L28 10000 seeds 8 hops | buildings L12 1914 seeds 4 hops | boundary L12 287 seeds 1 hops |
|---|---|---|---|---|---|
| reference median (ms) | 13.2 | 10.7 | 0.986 | 0.136 | 0.126 |
| kernel_per_ring | 1.04 | 1.04 | 1.35 | 2.45 | 1.48 |
| load_0.25 | 1.05 | 1.06 | 1.32 | 1.00 | 0.98 |
| load_0.7 | 0.99 | 0.96 | 0.96 | 0.99 | 1.00 |
| masked_hash | -- | -- | -- | 48.68 | 4.32 |
| morton_hash | 1.86 | 3.47 | 2.58 | 1.01 | 0.99 |
| no_arrival_skip | 1.09 | 1.10 | 0.98 | 1.61 | 1.00 |
| no_mechanisms | -- | -- | -- | -- | -- |
| no_read_before_cas | 1.25 | 1.31 | 1.14 | 1.64 | 0.99 |
| pageable_transfer | -- | -- | -- | -- | -- |
| preflight_every_call | 1.00 | 1.00 | 1.00 | 1.01 | 1.00 |
| realized_capacity | 1.00 | 1.00 | 1.00 | 1.15 | 0.97 |
| scatter_clear | 1.00 | 1.00 | 1.00 | 0.97 | 1.01 |
| separate_control_clears | 1.00 | 1.00 | 1.01 | 1.07 | 1.11 |
| thread_append | 1.08 | 1.07 | 1.16 | 1.01 | 1.01 |

## B. Representation on one CPU thread (median of 30 calls, ms; ratio to the compact index)

calgary_points level 20, discovered set auto, seeds 99999, output 2914499:
  compact          147.284 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical     306.730 ms  ratio  2.08  adjacency storage 720 bytes
  bim_tuple        193.523 ms  ratio  1.31  adjacency storage 720 bytes
  bim_packed       153.864 ms  ratio  1.04  adjacency storage 720 bytes

calgary_points level 28, discovered set auto, seeds 100000, output 3000000:
  compact          157.452 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical     399.130 ms  ratio  2.53  adjacency storage 720 bytes
  bim_tuple        202.730 ms  ratio  1.29  adjacency storage 720 bytes
  bim_packed       163.448 ms  ratio  1.04  adjacency storage 720 bytes

compact level 10, discovered set auto, seeds 100000, output 3124:
  compact            9.277 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical       9.767 ms  ratio  1.05  adjacency storage 720 bytes
  bim_tuple         16.264 ms  ratio  1.75  adjacency storage 720 bytes
  bim_packed         8.950 ms  ratio  0.96  adjacency storage 720 bytes
  stored             8.265 ms  ratio  0.89  adjacency storage 1,509,949,440 bytes

compact level 16, discovered set auto, seeds 100000, output 3124:
  compact            8.653 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical       9.277 ms  ratio  1.07  adjacency storage 720 bytes
  bim_tuple         16.418 ms  ratio  1.90  adjacency storage 720 bytes
  bim_packed         8.945 ms  ratio  1.03  adjacency storage 720 bytes

compact level 28, discovered set auto, seeds 100000, output 3124:
  compact           11.061 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical      12.769 ms  ratio  1.15  adjacency storage 720 bytes
  bim_tuple         18.343 ms  ratio  1.66  adjacency storage 720 bytes
  bim_packed        10.258 ms  ratio  0.93  adjacency storage 720 bytes

scattered level 10, discovered set auto, seeds 100000, output 2928633:
  compact          145.675 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical     294.968 ms  ratio  2.02  adjacency storage 720 bytes
  bim_tuple        188.138 ms  ratio  1.29  adjacency storage 720 bytes
  bim_packed       147.831 ms  ratio  1.01  adjacency storage 720 bytes
  stored           160.213 ms  ratio  1.10  adjacency storage 1,509,949,440 bytes

scattered level 28, discovered set auto, seeds 100000, output 3000000:
  compact          156.982 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical     399.729 ms  ratio  2.55  adjacency storage 720 bytes
  bim_tuple        202.451 ms  ratio  1.29  adjacency storage 720 bytes
  bim_packed       162.840 ms  ratio  1.04  adjacency storage 720 bytes

calgary_points level 20, discovered set tight, seeds 99999, output 2914499:
  compact          147.838 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical     305.782 ms  ratio  2.07  adjacency storage 720 bytes
  bim_tuple        193.186 ms  ratio  1.31  adjacency storage 720 bytes
  bim_packed       155.885 ms  ratio  1.05  adjacency storage 720 bytes

calgary_points level 28, discovered set tight, seeds 100000, output 3000000:
  compact          158.500 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical     398.417 ms  ratio  2.51  adjacency storage 720 bytes
  bim_tuple        202.452 ms  ratio  1.28  adjacency storage 720 bytes
  bim_packed       164.951 ms  ratio  1.04  adjacency storage 720 bytes

compact level 10, discovered set tight, seeds 100000, output 3124:
  compact            3.275 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical       4.811 ms  ratio  1.47  adjacency storage 720 bytes
  bim_tuple          4.358 ms  ratio  1.33  adjacency storage 720 bytes
  bim_packed         3.335 ms  ratio  1.02  adjacency storage 720 bytes
  stored             2.997 ms  ratio  0.92  adjacency storage 1,509,949,440 bytes

compact level 16, discovered set tight, seeds 100000, output 3124:
  compact            3.371 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical       5.104 ms  ratio  1.51  adjacency storage 720 bytes
  bim_tuple          4.633 ms  ratio  1.37  adjacency storage 720 bytes
  bim_packed         3.571 ms  ratio  1.06  adjacency storage 720 bytes

compact level 28, discovered set tight, seeds 100000, output 3124:
  compact            3.883 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical       7.513 ms  ratio  1.94  adjacency storage 720 bytes
  bim_tuple          5.139 ms  ratio  1.32  adjacency storage 720 bytes
  bim_packed         4.005 ms  ratio  1.03  adjacency storage 720 bytes

scattered level 10, discovered set tight, seeds 100000, output 2928633:
  compact          150.855 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical     296.984 ms  ratio  1.97  adjacency storage 720 bytes
  bim_tuple        187.726 ms  ratio  1.24  adjacency storage 720 bytes
  bim_packed       147.719 ms  ratio  0.98  adjacency storage 720 bytes
  stored           157.505 ms  ratio  1.04  adjacency storage 1,509,949,440 bytes

scattered level 28, discovered set tight, seeds 100000, output 3000000:
  compact          157.660 ms  ratio  1.00  adjacency storage 720 bytes
  hierarchical     398.355 ms  ratio  2.53  adjacency storage 720 bytes
  bim_tuple        202.849 ms  ratio  1.29  adjacency storage 720 bytes
  bim_packed       164.458 ms  ratio  1.04  adjacency storage 720 bytes

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
