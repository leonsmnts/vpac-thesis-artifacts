# Measurement and reference implementation source code

The directories directly implement the layered implementation described in the thesis.

- `net/` implements the `hot_[sender|replier|receiver]` applications, with the `hot_replier` acting as the upper networking layer of the representative protection workload.

- `measurement/` implements the measurement preprocessing layer.

- `protection/` implements the ANSI protection computation kernel layer.

`timing.[c|h]` implement general benchmarking capability of function invocations.
