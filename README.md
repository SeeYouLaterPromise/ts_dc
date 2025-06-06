# Efficient Time-series Floating-point Compression

> We gratefully acknowledge the authors of `Machete` for releasing their source code, which provided a solid foundation for our further experiments.

## Evaluated Compressors

* Gorilla: A fast **lossless** time series database compressor. Codes in the repo are written based on the implementation in InfluxDB: https://github.com/influxdata/influxdb.git
* Chimp128: A **lossless** compressor based on Gorilla. Codes in this repo are based on https://github.com/panagiotisl/chimp.git
* Elf: A **lossless** compressor based on Gorilla improved for digital-place-limited data. Codes in this repo are based on https://github.com/Spatio-Temporal-Lab/elf
* ZStandard: version 1.3.3
* Deflate (A.K.A. GZip): version 1.2.11
* SZ3: Code from https://github.com/szcompressor/SZ3.git
* LFZip: Code based on https://github.com/shubhamchandak94/LFZip.git
* Machete: A novel lossy and efficient compressor with improved compression ratio for small error bounds under the point-wise absolute error control. Code from https://github.com/Gyhanis/Machete

### compression_test.cpp
