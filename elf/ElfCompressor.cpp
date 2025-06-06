#include <cstdint>
#include <assert.h>
#include <math.h>

#include "elf.h"
#include "defs.h"
#include "BitStream/BitWriter.h"
#include "BitStream/BitReader.h"

class AbstractElfCompressor {
private:
        size_t size = 32;
        int lastBetaStar = __INT32_MAX__;
protected:
        virtual int writeInt(int n, int len) = 0;
        virtual int writeBit(bool bit) = 0;
        virtual int xorCompress(long vPrimeLong) = 0;

public:
        void addValue(double v) {
                // when you assign v to data.d, the corresponding bit pattern is stored in the union as data.i
                // the concrete definition of the union is in defs.h
                DOUBLE data = {.d = v};
                long vPrimeLong;
                assert(!isnan(v));
                if (v == 0.0) {
                        size += writeInt(2,2);
                        vPrimeLong = data.i;
                // } else if (isnan(v)) {
                //         size += writeInt(2,2);
                //         vPrimeLong = 0xfff8000000000000L & data.i;
                } else {
                        int* alphaAndBetaStar = getAlphaAndBetaStar(v, lastBetaStar);
                        int e = ((int) (data.i >> 52)) & 0x7ff;
                        int gAlpha = getFAlpha(alphaAndBetaStar[0]) + e - 1023;
                        int eraseBits = 52 - gAlpha;
                        long mask = 0xffffffffffffffffL << eraseBits;
                        long delta = (~mask) & data.i;
                        if (delta != 0 && eraseBits > 4) {
                                if (alphaAndBetaStar[1] == lastBetaStar) {
                                        size += writeBit(false);
                                } else {
                                        size += writeInt(alphaAndBetaStar[1] | 0x30, 6);
                                        lastBetaStar = alphaAndBetaStar[1];
                                }
                                vPrimeLong = mask & data.i;
                        } else {
                                size += writeInt(2,2);
                                vPrimeLong = data.i;
                        }

                        delete [] alphaAndBetaStar;
                        size += xorCompress(vPrimeLong);
                }
                // size += xorCompress(vPrimeLong);
        }

        int getSize() {
                return size;
        }
};

static const short leadingRepresentation[] = 
{0, 0, 0, 0, 0, 0, 0, 0,
1, 1, 1, 1, 2, 2, 2, 2,
3, 3, 4, 4, 5, 5, 6, 6,
7, 7, 7, 7, 7, 7, 7, 7,
7, 7, 7, 7, 7, 7, 7, 7,
7, 7, 7, 7, 7, 7, 7, 7,
7, 7, 7, 7, 7, 7, 7, 7,
7, 7, 7, 7, 7, 7, 7, 7
};

static const short leadingRound[] = 
{0, 0, 0, 0, 0, 0, 0, 0,
8, 8, 8, 8, 12, 12, 12, 12,
16, 16, 18, 18, 20, 20, 22, 22,
24, 24, 24, 24, 24, 24, 24, 24,
24, 24, 24, 24, 24, 24, 24, 24,
24, 24, 24, 24, 24, 24, 24, 24,
24, 24, 24, 24, 24, 24, 24, 24,
24, 24, 24, 24, 24, 24, 24, 24
};

class ElfXORCompressor {
private: 
        int storedLeadingZeros = __INT32_MAX__;
        int storedTrailingZeros = __INT32_MAX__;
        uint64_t storedVal = 0;
        bool first = true;
        size_t size = 32;
        size_t length = 0;
        BitWriter writer;
        uint32_t *output;

        int writeFirst(long value) {
                first = false;
                storedVal = value;
                length = 1;
                // ctzl: count triailing zeros (in) long
                // __builtin_ctzl is a GCC/Clang built-in (intrinsic) function:
                // pat attention: if the input is zero, the result is undefined.
                assert(value != 0);
                // However, in this case, Case Zero will be handled by Abstract Class already.
                int trailingZeros = __builtin_ctzl(value); 

                // write the trailing count
                write(&writer, trailingZeros, 6); // (Case Zero will be handled by Abstract Class already.)
                // write the bits excluding the trailing zeros
                // here (trailingZeros + 1) is intentional to exclude the definite 1 bit at the end to save one bit space. (greedy)
                writeLong(&writer, storedVal >> (trailingZeros + 1), 63 - trailingZeros);
                size += 69 - trailingZeros;
                return 69 - trailingZeros;
        }

        int compressValue(long value) {
                int thisSize = 0;
                uint64_t _xor = storedVal ^ value;
                if (_xor == 0) {
                        write(&writer, 1, 2);
                        size += 2;
                        thisSize += 2;
                } else {
                        // clzl: count leading zeros (in) long
                        int leadingZeros = leadingRound[__builtin_clzl(_xor)];
                        int trailingZeros = __builtin_ctzl(_xor);

                        if (leadingZeros == storedLeadingZeros && trailingZeros >= storedTrailingZeros) {
                                int centerBits = 64 - storedLeadingZeros - storedTrailingZeros;
                                int len = 2 + centerBits;
                                // The maximum length of writeLong is 64, so do the if-logic?
                                if (len > 64) {
                                        write(&writer, 0, 2);
                                        writeLong(&writer, _xor >> storedTrailingZeros, centerBits);
                                } else {
                                        writeLong(&writer, _xor >> storedTrailingZeros, len);
                                }

                                size += len;
                                thisSize += len;
                        } else {
                                storedLeadingZeros = leadingZeros;
                                storedTrailingZeros = trailingZeros;
                                int centerBits = 64 - storedLeadingZeros - storedTrailingZeros;

                                if (centerBits <= 16) {
                                        write(&writer, (((0x2 << 3) | leadingRepresentation[storedLeadingZeros]) << 4) | (centerBits & 0xf), 9);
                                        writeLong(&writer, _xor >> (storedTrailingZeros + 1), centerBits - 1);

                                        size += 8 + centerBits;
                                        thisSize += 8 + centerBits;
                                } else {
                                        write(&writer, (((0x3 << 3) | leadingRepresentation[storedLeadingZeros]) << 6) | (centerBits & 0x3f), 11);
                                        writeLong(&writer, _xor >> (storedTrailingZeros + 1), centerBits - 1);

                                        size += 10 + centerBits;
                                        thisSize += 10 + centerBits;
                                }
                        }
                        storedVal = value;
                } 
                length++;
                return thisSize;
        }

public:
        BitWriter* getWriter() {
                return &writer;
        }

        // This function is to initize the memory for `BitWriter`.
        // `length` is the number of data points to be compressed.
        void init(size_t length) {
                // It multiplies the length by 12 to estimate the buffer size needed.
                length *= 12;
                output = (uint32_t*) malloc(length + 4);
                // this is a common trick to reserve the first 4 bytes (the first uint32_t) of the buffer for metadata
                // often to store the length of the data. -> you can see `*output = length;` in `close()`.
                initBitWriter(&writer, output+1, length/sizeof(uint32_t));
        }

        int addValue(long value) {
                if (first) {
                        return writeFirst(value);
                } else {
                        return compressValue(value);
                }
        }

        int addValue(double value) {
                DOUBLE data = {.d = value};
                if (first) {
                        return writeFirst(data.i);
                } else {
                        return compressValue(data.i);
                }
        }

        void close() {
                // extract the first uint32_t to get the total length of the compressed data.
                *output = length;
                flush(&writer);
        }

        size_t getSize() {
                return size;
        }

        uint32_t* getOut() {
                return output;
        }
};

class ElfCompressor: public AbstractElfCompressor {
private:
        ElfXORCompressor xorCompressor;

protected:
        int writeInt(int n, int len) override {
                write(xorCompressor.getWriter(), n, len);
                return len;
        }

        int writeBit(bool bit) override {
                write(xorCompressor.getWriter(), bit, 1);
                return 1;
        }

        int xorCompress(long vPrimeLong) override {
                return xorCompressor.addValue(vPrimeLong);
        }

public: 
        void init(int length) {
                xorCompressor.init(length);
        }
        uint32_t* getBytes() {
                return xorCompressor.getOut();
        }

        void close() {
                writeInt(2,2);
                xorCompressor.close();
        }
};

ssize_t elf_encode(double* in, ssize_t len, uint8_t** out, double error) {
        ElfCompressor compressor;
        compressor.init(len);

        // Here implmentation of the end of ELF is NOT NaN.
        for (int i = 0; i < len; i++) {
                if (i == 4219) {
                        // asm is a GCC/Clang extension to allow inline assembly
                        // it used as a marker or breakpoint at i == 4219
                        asm("nop");
                }
                compressor.addValue(in[i]);
        }
        compressor.close();
        *out = (uint8_t*) compressor.getBytes();
        return (compressor.getSize() + 31) / 32 * 4;
}