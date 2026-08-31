/**  A lz4 implementation of protobuf Stream.
 *
 * #include "lz4_stream.h" <BR>
 *
 * @see protobuf and lz4 docs
 */

#ifndef ALIBABA_GOOGLE_PROTOBUF_IO_LZ4_STREAM_H__
#define ALIBABA_GOOGLE_PROTOBUF_IO_LZ4_STREAM_H__

#include <string>
#include <lz4frame.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/io/zero_copy_stream.h>

namespace google {
namespace protobuf {
namespace io {

// Abstract interface similar to an input stream but designed to minimize
// copying.

class Lz4InputStream : public ZeroCopyInputStream {
    public:
        Lz4InputStream(ZeroCopyInputStream* subStream, int desBufferSize = 64 * 1024);
        virtual ~Lz4InputStream();

        // Obtains a chunk of data from the stream.
        //
        // Preconditions:
        // * "size" and "data" are not NULL.
        //
        // Postconditions:
        // * If the returned value is false, there is no more data to return or
        //   an error occurred.  All errors are permanent.
        // * Otherwise, "size" points to the actual number of bytes read and "data"
        //   points to a pointer to a buffer containing these bytes.
        // * Ownership of this buffer remains with the stream, and the buffer
        //   remains valid only until some other method of the stream is called
        //   or the stream is destroyed.
        // * It is legal for the returned buffer to have zero size, as long
        //   as repeatedly calling Next() eventually yields a buffer with non-zero
        //   size.
        virtual bool Next(const void** data, int* size) override;

        // Backs up a number of bytes, so that the next call to Next() returns
        // data again that was already returned by the last call to Next().  This
        // is useful when writing procedures that are only supposed to read up
        // to a certain point in the input, then return.  If Next() returns a
        // buffer that goes beyond what you wanted to read, you can use BackUp()
        // to return to the point where you intended to finish.
        //
        // Preconditions:
        // * The last method called must have been Next().
        // * count must be less than or equal to the size of the last buffer
        //   returned by Next().
        //
        // Postconditions:
        // * The last "count" bytes of the last buffer returned by Next() will be
        //   pushed back into the stream.  Subsequent calls to Next() will return
        //   the same data again before producing new data.
        virtual void BackUp(int count) override;

        // Skips a number of bytes.  Returns false if the end of the stream is
        // reached or some input error occurred.  In the end-of-stream case, the
        // stream is advanced to the end of the stream (so ByteCount() will return
        // the total size of the stream).
        virtual bool Skip(int count) override;

        // Returns the total number of bytes read since this object was created.
        virtual int64 ByteCount() const override { return mByteCount; }

        int64_t getDecompressCost() { return mDecompressTimer; }//ms

        Lz4InputStream(Lz4InputStream&) = delete;
        void operator=(Lz4InputStream&) = delete;

    private:
        void Decompress();
        void ReadData();

    private:
        ZeroCopyInputStream* mSubStream;
        int64_t mByteCount;
        LZ4F_decompressionContext_t mDecompressContext;
        char* mOutputBuffer;
        int mOutputBufferSize;
        int mOutputBufferFilled;
        int mOutputBufferRemains;
        const void* mInputBuffer;
        int mInputBufferSize;
        size_t mInputBufferOffset;
        int64_t mDecompressTimer;
        bool mFailed;
};

// Abstract interface similar to an output stream but designed to minimize
// copying.
class Lz4OutputStream : public ZeroCopyOutputStream {
    public:
        Lz4OutputStream(ZeroCopyOutputStream* subStream, int srcBufferSize = 64 * 1024);
        virtual ~Lz4OutputStream();

        // Obtains a buffer into which data can be written.  Any data written
        // into this buffer will eventually (maybe instantly, maybe later on)
        // be written to the output.
        //
        // Preconditions:
        // * "size" and "data" are not NULL.
        //
        // Postconditions:
        // * If the returned value is false, an error occurred.  All errors are
        //   permanent.
        // * Otherwise, "size" points to the actual number of bytes in the buffer
        //   and "data" points to the buffer.
        // * Ownership of this buffer remains with the stream, and the buffer
        //   remains valid only until some other method of the stream is called
        //   or the stream is destroyed.
        // * Any data which the caller stores in this buffer will eventually be
        //   written to the output (unless BackUp() is called).
        // * It is legal for the returned buffer to have zero size, as long
        //   as repeatedly calling Next() eventually yields a buffer with non-zero
        //   size.
        virtual bool Next(void** data, int* size) override;

        // Backs up a number of bytes, so that the end of the last buffer returned
        // by Next() is not actually written.  This is needed when you finish
        // writing all the data you want to write, but the last buffer was bigger
        // than you needed.  You don't want to write a bunch of garbage after the
        // end of your data, so you use BackUp() to back up.
        //
        // Preconditions:
        // * The last method called must have been Next().
        // * count must be less than or equal to the size of the last buffer
        //   returned by Next().
        // * The caller must not have written anything to the last "count" bytes
        //   of that buffer.
        //
        // Postconditions:
        // * The last "count" bytes of the last buffer returned by Next() will be
        //   ignored.
        virtual void BackUp(int count) override;

        // Returns the total number of bytes written since this object was created.
        virtual int64 ByteCount() const override { return mByteCount; }

        int64_t getCompressCost() { return mCompressTimer; }

        Lz4OutputStream(Lz4OutputStream&) = delete;
        void operator=(Lz4OutputStream&) = delete;

    private:
        void CompressUpdate();
        void WriteBytes(const char* bytes, int len);
        void WriteHeader();
        void WriteFooter();
        void Flush();
        void Close();

    private:
        ZeroCopyOutputStream* mSubStream;
        int64_t mByteCount;
        LZ4F_compressionContext_t mCompressContext;
        char* mOutputBuffer;
        //mOutputSize=LZ4F_compressBound(srcBufferSize, nullptr)
        int mOutputSize;
        char* mInputBuffer; //src buf size = srcBufferSize
        int mInputFilled;
        int mInputSize; //srcBufferSize
        int64_t mCompressTimer;
        bool mClosed;
        char* mNextOutputBuffer;
        int mNextOutputBufferSize;
        int mNextOutputBufferFilled;
};

}  // namespace io
}  // namespace protobuf
}  // namespace google

#endif  // ALIBABA_GOOGLE_PROTOBUF_IO_LZ4_STREAM_H__

