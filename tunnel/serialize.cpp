#include "google/protobuf/stubs/common.h"
#include "google/protobuf/wire_format_lite.h"
#include "google/protobuf/wire_format_lite_inl.h"
#include "google/protobuf/io/gzip_stream.h"

#include "odps_tunnel.h"
#include "error_code.h"
#include "serialize.h"
#include "common/logging.h"
#include "util/timer.h"

//#include "proto_serialize.h"
#include "google/protobuf/stubs/common.h"
#include "google/protobuf/wire_format_lite.h"
#include "google/protobuf/wire_format_lite_inl.h"
#include "google/protobuf/io/gzip_stream.h"
#include "util.h"
#include "util/utils.h"
#include "zstd_stream.h"
#include "lz4_stream.h"

// protobuf used
typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#define PROTOBUF_TOTAL_BYTES_LIMIT 536870912 // 512MB
#define PROTOBUF_WARNING_THRESHOLD 469762048 // 448MB

#define META_TOTAL_RECORDS_TAG 33554430 // magic num 2^25-2
#define META_CHECKSUM_TAG 33554431 //magic num 2^25-1
#define RECORD_END_TAG 33553408 //magic num 2^25-1024
#define METRICS_END_TAG 33554176 // magic num 2^25 - 256

using namespace std;

// XXX: use protobuf internal
using namespace google::protobuf::internal;
using namespace google::protobuf::io;

using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::util;
using namespace apsara::odps::sdk::internal::tunnel;

static apsara::odps::sdk::logging::Logger* sLogger = apsara::odps::sdk::logging::GetLogger("/odps/tunnel/internal");

static int64_t DecodeDate(int64_t ts)
{
    return ts / SECONDS_PER_DAY;
}

static int64_t EncodeDate(int64_t ts)
{
    return ts * SECONDS_PER_DAY;
}

template <typename T>
static void PrimitiveTypeCrc(crc_32c_type& crc,T v)
{
    crc.process_bytes(&v, sizeof(T));
}

static void TrimOrPadding(std::string& str, const ODPSColumnTypeInfo& type)
{
    if (ODPS_CHAR == type.mType || ODPS_VARCHAR == type.mType)
    {
        if ((ODPS_CHAR == type.mType && (type.mSpecifiedLength < 1 || type.mSpecifiedLength > 255))
            || (ODPS_VARCHAR == type.mType && (type.mSpecifiedLength < 1 || type.mSpecifiedLength > 65535)))
        {
            throw OdpsTunnelException("CHAR/VARCHAR length invalid: " + std::to_string(type.mSpecifiedLength));
        }

        std::string::size_type pos = 0;
        int64_t len = 0;
        while(pos < str.size() && len < type.mSpecifiedLength)
        {
            pos = FindNextCharUtf8(str, pos);
            ++len;
        }

        if ((ODPS_CHAR == type.mType && len < type.mSpecifiedLength))
        {
            str.append(type.mSpecifiedLength - len, ' ');
        }
        else if (len >= type.mSpecifiedLength)
        {
            str.resize(pos);
        }
    }
}

//-----------------------------------------------------------------------------
//
// HttpInputStream
//
//-----------------------------------------------------------------------------
HttpInputStream::HttpInputStream(
            const ResponsePtr response,
            const int bufferSize)
{
    mResponsePtr = response;
    mBufferSize = bufferSize;

    mBuffer = new char[mBufferSize];
    mPos = 0;
    mDataSize = 0;
    mBytes = 0;
    mLatency = 0;
}

HttpInputStream::~HttpInputStream()
{
    delete [] mBuffer;
}

bool HttpInputStream::Next(const void** data, int* size)
{
    if (mDataSize - mPos == 0) // empty
    {
        int64_t start = util::Timer::GetCurrentTimeInMicroSeconds();
        int64_t read = mResponsePtr->ReadBody(mBuffer, mBufferSize);
        mLatency += (util::Timer::GetCurrentTimeInMicroSeconds() - start);
        if (read <= 0)
        {
            return false;
        }
        else
        {
            *data = mBuffer;
            *size = read;
            mBytes += read;
            mDataSize = read;
            mPos = read;
            return true;
        }
    }
    else
    {
        *data = mBuffer + mPos;
        *size = mDataSize - mPos;
        mPos = mDataSize;
        mBytes += *size;

        return true;
    }

}

void HttpInputStream::BackUp(int count)
{
    mPos -= count;
    if(mPos < 0)
    {
        abort();
    }
}

bool HttpInputStream::Skip(int count)
{
    mPos += count;
    if(mPos > mDataSize)
    {
        abort();
    }
    return true;
}

//-----------------------------------------------------------------------------
//
// HttpOutputStream
//
//-----------------------------------------------------------------------------
HttpOutputStream::HttpOutputStream(HttpConnectionPtr request,const int bufferSize)
{
    mRequestPtr = request;
    mBufferSize = bufferSize;
    mBuffer = new char[mBufferSize];
    mPos = 0;
    mBytes = 0;
    mLatency = 0;
}

HttpOutputStream::~HttpOutputStream()
{
    Flush();
    delete [] mBuffer;
}

bool HttpOutputStream::Flush()
{
    try
    {
        if (mPos > 0)
        {
            int64_t start = util::Timer::GetCurrentTimeInMicroSeconds();
            int64_t wrote = mRequestPtr->Write(mBuffer, mPos);
            mLatency += (util::Timer::GetCurrentTimeInMicroSeconds() - start);
            if (wrote != mPos)
            {
                return false;
            }
            mBytes += mPos;
            mPos = 0;
        }
        return true;
    }
    catch(OdpsTunnelException& e)
    {
        LOG_ERROR(sLogger, ("HttpOutputStream flush failed", e.what()));
        return false;
    }
}

bool HttpOutputStream::Next(void** data, int* size)
{
    if (mPos > 0)
    {
        if (!Flush())
        {
            return false;
        }
        mPos = 0;
    }
    *data = mBuffer;
    *size = mBufferSize;
    mPos = mBufferSize;
    return true;
}

void HttpOutputStream::BackUp(int count)
{
    mPos -= count;
    if (mPos < 0)
    {
        LOG_ERROR(sLogger, ("HttpOutputStream", "BackUp") ("Pos", mPos) ("count", count));
        abort();
    }
}
//-----------------------------------------------------------------------------
//
// ProtoSerializer
//
// ----------------------------------------------------------------------------
ProtoSerializer::ProtoSerializer(google::protobuf::io::ZeroCopyOutputStream* out,
            ODPSTableSchema* schema,
            bool compress,
            bool doStreamChecksum,
            int protobufTotalBytesLimit,
            CompressOption* option)
{
    mOutput = NULL;
    mCodedOutput = NULL;
    mSchema = schema;

    mProtobufTotalBytesLimit = protobufTotalBytesLimit;
    if (mProtobufTotalBytesLimit > PROTOBUF_WARNING_THRESHOLD)
    {
        throw OdpsTunnelException(INTERNAL_ERROR, "protobufTotalBytesLimit > " + std::to_string(PROTOBUF_WARNING_THRESHOLD));
    }

    mTotalRecords = 0;

    mIsCompress = compress;
    if (mIsCompress)
    {
        if (option == NULL)
        {
            google::protobuf::io::GzipOutputStream::Options tmpOption;
            tmpOption.format = google::protobuf::io::GzipOutputStream::ZLIB;
            tmpOption.compression_level = Z_BEST_SPEED;
            tmpOption.compression_strategy = Z_DEFAULT_STRATEGY;
            mOutput = new GzipOutputStream(out, tmpOption);
        }
        else if (option->algorithm == CompressOption::ODPS_ZLIB)
        {
            google::protobuf::io::GzipOutputStream::Options tmpOption;
            tmpOption.format = GzipOutputStream::ZLIB;
            tmpOption.compression_level = option->level;
            tmpOption.compression_strategy = option->strategy;
            mOutput = new GzipOutputStream(out, tmpOption);
        }
        else if (option->algorithm == CompressOption::ODPS_ZSTD)
        {
            mOutput = new ZstdOutputStream(out, option->level, option->bsize, option->mode > 0);
        }
        else if (option->algorithm == CompressOption::ODPS_LZ4_FRAME)
        {
            mOutput = new Lz4OutputStream(out);
        }
        else
        {
            throw OdpsTunnelException(INTERNAL_ERROR, "Unsupported Protobuf compression " + CompressOptionToEncoding(*option));
        }
    }
    else
    {
        mOutput = out;
    }

    mBytes = mOutput->ByteCount();
    mDoStreamChecksum = doStreamChecksum;
}

ProtoSerializer::~ProtoSerializer()
{
    if (mCodedOutput != NULL)
    {
        delete mCodedOutput;
    }

    if (mIsCompress && mOutput != NULL)
    {
        delete mOutput;
    }
}

bool ProtoSerializer::Serialize(const ODPSTableRecord& r)
{
    if (mCodedOutput == NULL || mCodedOutput->ByteCount() - mBytes > mProtobufTotalBytesLimit)
    {
        if (mCodedOutput != NULL)
        {
            mBytes = mCodedOutput->ByteCount();
            delete mCodedOutput;
            mCodedOutput = NULL;

            LOG_DEBUG(sLogger, ("New", "CodedOutputStream") ("Bytes", mBytes));
        }

        mCodedOutput = new CodedOutputStream(mOutput);
    }

    LOG_DEBUG(sLogger, ("CodecOutput", "") ("TotalBytesLimit", mProtobufTotalBytesLimit)
                ("ByteCount", mCodedOutput->ByteCount()));

    if (mCodedOutput->HadError())
    {
        return false;
    }

    // convert sqlrecord to protobuf, use wire_format
    uint32_t fieldCount = mSchema->GetColumnCount();

    mRecordCrc.reset(); // reset crc
    for(size_t i = 0; i < fieldCount; ++i)
    {
        WriteCell(i + 1, r, mSchema->GetTableColumn(i).GetType());
    }

    // Write End Tag
    uint32_t crc = 0;
    if (mDoStreamChecksum)
    {
        crc = mCodedChecksum.GetChecksum();
        mCodedChecksum.PutUInt32(RECORD_END_TAG, crc);
    }
    else
    {
        crc = mRecordCrc.checksum(); // checksum
        mCrcCrc.process_bytes(&crc, sizeof(crc)); // crccrc
    }

    WireFormatLite::WriteUInt32(RECORD_END_TAG, crc, mCodedOutput);

    ++mTotalRecords;

    return true;
}

void ProtoSerializer::WriteCell(const int32_t index, const ODPSTableRecord& r, ODPSColumnType type)
{
    uint32_t recordPos = index - 1;
    if (r.IsNullValue(recordPos))
    {
        return;
    }

    mRecordCrc.process_bytes(&index, sizeof(index)); // crc

    switch(type)
    {
        case ODPS_JSON:
        case ODPS_STRING:
        case ODPS_CHAR:
        case ODPS_VARCHAR:
        case ODPS_BINARY:
        case ODPS_DECIMAL:
        {
            uint32_t length;
            const char* v = r.GetStringValue(index - 1, length, type);
            WireFormatLite::WriteTag(index, WireFormatLite::WIRETYPE_LENGTH_DELIMITED, mCodedOutput);
            mCodedOutput->WriteVarint32(length);
            mCodedOutput->WriteRaw(v, length);
            LOG_DEBUG(sLogger, ("Serialize V_STRING", (const char*)v) ("Length", length));

            if (mDoStreamChecksum)
            {
                mCodedChecksum.PutTag(index, WireFormatLite::WIRETYPE_LENGTH_DELIMITED);
                mCodedChecksum.PutVarint32(length);
                mCodedChecksum.PutRaw(v, length);
            }
            else
            {
                mRecordCrc.process_bytes(v, length); // crc
            }
            break;
        }
        case ODPS_BIGINT:
        case ODPS_TINYINT:
        case ODPS_SMALLINT:
        case ODPS_INTEGER:
        case ODPS_DATETIME:
        case ODPS_DATE:
        case ODPS_INTERVAL_YEAR_MONTH:
            {
                const int64_t* v = r.GetIntValue(index - 1, type);
                WireFormatLite::WriteSInt64(index, *v, mCodedOutput);

                if (mDoStreamChecksum)
                {
                    mCodedChecksum.PutSInt64(index, *v);
                }
                else
                {
                    mRecordCrc.process_bytes(v, sizeof(*v)); // crc
                }
                break;
            }
        case ODPS_FLOAT:
            {
                const float * v = r.GetFloatValue(index - 1);
                WireFormatLite::WriteFloat(index, *v, mCodedOutput);

                if (mDoStreamChecksum)
                {
                    mCodedChecksum.PutDouble(index, *v);
                }
                else
                {
                    mRecordCrc.process_bytes(v, sizeof(*v)); // crc
                }
                break;
            }
        case ODPS_DOUBLE:
            {
                const double * v = r.GetDoubleValue(index - 1);
                WireFormatLite::WriteDouble(index, *v, mCodedOutput);

                if (mDoStreamChecksum)
                {
                    mCodedChecksum.PutDouble(index, *v);
                }
                else
                {
                    mRecordCrc.process_bytes(v, sizeof(*v)); // crc
                }
                break;
            }
        case ODPS_BOOLEAN:
            {
                const bool* v = r.GetBoolValue(index - 1);
                WireFormatLite::WriteBool(index, *v, mCodedOutput);

                if (mDoStreamChecksum)
                {
                    mCodedChecksum.PutBool(index, *v);
                }
                else
                {
                    mRecordCrc.process_bytes(v, sizeof(*v)); // crc
                }
                break;
            }
        case ODPS_TIMESTAMP:
        case ODPS_TIMESTAMP_NTZ:
        case ODPS_INTERVAL_DAY_TIME:
            {
                const TimeStamp* v = r.GetTimeValue(index - 1, type);
                WireFormatLite::WriteTag(index, WireFormatLite::WIRETYPE_LENGTH_DELIMITED, mCodedOutput);
                int64_t second = v->GetSecond();
                int32_t nano = v->GetNano();
                WireFormatLite::WriteSInt64NoTag(second, mCodedOutput);
                WireFormatLite::WriteSInt32NoTag(nano, mCodedOutput);

                mRecordCrc.process_bytes(&second, sizeof(second)); // crc
                mRecordCrc.process_bytes(&nano, sizeof(nano)); // crc
                break;
            }
        case ODPS_STRUCT:
            {
                shared_ptr<ODPSStruct> structValue = r.GetStructValue(recordPos);
                WireFormatLite::WriteTag(index, WireFormatLite::WIRETYPE_LENGTH_DELIMITED, mCodedOutput);
                WriteComplexValue(structValue);
            }
            break;
        case ODPS_ARRAY:
            {
                shared_ptr<ODPSArray> arrayValue = r.GetArrayValue(recordPos);
                WireFormatLite::WriteTag(index, WireFormatLite::WIRETYPE_LENGTH_DELIMITED, mCodedOutput);
                WriteComplexValue(arrayValue);
            }
            break;
        case ODPS_MAP:
            {
                shared_ptr<ODPSMap> mapValue = r.GetMapValue(recordPos);
                WireFormatLite::WriteTag(index, WireFormatLite::WIRETYPE_LENGTH_DELIMITED, mCodedOutput);
                WriteComplexValue(mapValue);
            }
            break;
        default:
            throw OdpsTunnelException(INTERNAL_ERROR, "Invalid type : " + std::to_string(type));
    }

    LOG_DEBUG(sLogger, ("Serialize FIELD", "") ("IDX", index)("CRC", ToHexString<uint32_t>(mRecordCrc.checksum())));
}

void ProtoSerializer::WriteNestedValue(const void* value, const ODPSColumnTypeInfo& type)
{
    WireFormatLite::WriteBoolNoTag(false, mCodedOutput);

    switch(type.mType)
    {
        case ODPS_TINYINT:
        case ODPS_SMALLINT:
        case ODPS_INTEGER:
        case ODPS_BIGINT:
        case ODPS_DATE:
        case ODPS_DATETIME:
        case ODPS_INTERVAL_YEAR_MONTH:
            {
                int64_t int64Value;
                if (ODPS_TINYINT == type.mType)
                {
                    int64Value = *static_cast<const int8_t*>(value);
                }
                else if (ODPS_SMALLINT == type.mType)
                {
                    int64Value = *static_cast<const int16_t*>(value);
                }
                else if (ODPS_INTEGER == type.mType)
                {
                    int64Value = *static_cast<const int32_t*>(value);
                }
                else if (ODPS_DATE == type.mType)
                {
                    int64Value = DecodeDate(*static_cast<const int64_t*>(value));
                }
                else
                {
                    int64Value = *static_cast<const int64_t*>(value);
                }
                WireFormatLite::WriteSInt64NoTag(int64Value, mCodedOutput);
                PrimitiveTypeCrc(mRecordCrc, int64Value);
                break;
            }
        case ODPS_FLOAT:
            {
                float floatValue = *static_cast<const float*>(value);
                WireFormatLite::WriteFloatNoTag(floatValue, mCodedOutput);
                PrimitiveTypeCrc(mRecordCrc, floatValue);
                break;
            }
        case ODPS_DOUBLE:
            {
                double doubleValue = *static_cast<const double*>(value);
                WireFormatLite::WriteDoubleNoTag(doubleValue, mCodedOutput);
                PrimitiveTypeCrc(mRecordCrc, doubleValue);
                break;
            }
        case ODPS_BOOLEAN:
            {
                bool boolValue = *static_cast<const bool*>(value);
                WireFormatLite::WriteBoolNoTag(boolValue, mCodedOutput);
                PrimitiveTypeCrc(mRecordCrc, boolValue);
                break;
            }
        case ODPS_JSON:
        case ODPS_CHAR:
        case ODPS_VARCHAR:
        case ODPS_STRING:
        case ODPS_BINARY:
            {
                std::string stringValue = *static_cast<const std::string*>(value);
                // will not do padding to STRING & BINARY.
                TrimOrPadding(stringValue, type);
                mCodedOutput->WriteVarint32(stringValue.size());
                mCodedOutput->WriteRaw(stringValue.c_str(), stringValue.size());
                mRecordCrc.process_bytes(stringValue.c_str(), stringValue.size());
                break;
            }
        case ODPS_TIMESTAMP:
        case ODPS_TIMESTAMP_NTZ:
        case ODPS_INTERVAL_DAY_TIME:
            {
                TimeStamp ts = *static_cast<const TimeStamp*>(value);
                WireFormatLite::WriteSInt64NoTag(ts.GetSecond(), mCodedOutput);
                WireFormatLite::WriteSInt32NoTag(ts.GetNano(), mCodedOutput);
                PrimitiveTypeCrc(mRecordCrc, ts.GetSecond());
                PrimitiveTypeCrc(mRecordCrc, ts.GetNano());
                break;
            }
        case ODPS_DECIMAL:
            {
                // TODO: replace with REAL decimal types.
                std::string stringValue = *static_cast<const std::string*>(value);
                mCodedOutput->WriteVarint32(stringValue.length());
                mCodedOutput->WriteRaw(stringValue.c_str(), stringValue.length());
                mRecordCrc.process_bytes(stringValue.c_str(), stringValue.length());
                break;
            }
        case ODPS_ARRAY:
        case ODPS_MAP:
        case ODPS_STRUCT:
            {
                IODPSProtoSerializablePtr complexValue = *static_cast<const IODPSProtoSerializablePtr*>(value);
                WriteComplexValue(complexValue);
                break;
            }
        default:
            {
                LOG_ERROR(sLogger, ("unsupported value type", type.mType));
                throw OdpsTunnelException("Unsupported value type in serailization: " + std::to_string(type.mType));
            }
    }
    LOG_DEBUG(sLogger, ("Serialize NVALUE", "") ("CRC", ToHexString<uint32_t>(mRecordCrc.checksum())));
}

#define ARRAY_VALUES_GENERATE(ODPSTYPE, SIGNTYPE, CTYPE) \
case ODPSTYPE: \
{ \
    for(uint32_t i = 0; i < length; i++) \
    { \
        if (arrayValue->IsNull(i)) \
        { \
            WireFormatLite::WriteBoolNoTag(true, mCodedOutput); \
        } \
        else \
        { /* the not null indicator is written in WriteNestedValue */ \
            CTYPE _value = arrayValue->Get##SIGNTYPE(i); \
            WriteNestedValue(&_value, elementType); \
        } \
    } \
}\
break

#define STRUCT_VALUES_GENERATE(ODPSTYPE, SIGNTYPE, CTYPE) \
case ODPSTYPE: \
{ \
    CTYPE _value = structValue->Get##SIGNTYPE(i); \
    WriteNestedValue(&_value, memberType); \
} \
break

void ProtoSerializer::WriteComplexValue(IODPSProtoSerializablePtr complexValue)
{
    ODPSColumnTypeInfo type = complexValue->GetTypeInfo();
    switch(type.mType)
    {
        case ODPS_ARRAY:
        {
            shared_ptr<ODPSArray> arrayValue = dynamic_pointer_cast<ODPSArray>(complexValue);
            uint32_t length = arrayValue->Size();
            WireFormatLite::WriteUInt32NoTag(length, mCodedOutput);
            ODPSColumnTypeInfo elementType = arrayValue->GetTypeInfo().mSubTypes[0];
            switch(elementType.mType)
            {
                ARRAY_VALUES_GENERATE(ODPS_BIGINT, BigInt, int64_t);
                ARRAY_VALUES_GENERATE(ODPS_TINYINT, TinyInt, int8_t);
                ARRAY_VALUES_GENERATE(ODPS_SMALLINT, SmallInt, int16_t);
                ARRAY_VALUES_GENERATE(ODPS_INTEGER, Integer, int32_t);
                ARRAY_VALUES_GENERATE(ODPS_FLOAT, Float, float);
                ARRAY_VALUES_GENERATE(ODPS_DOUBLE, Double, double);
                ARRAY_VALUES_GENERATE(ODPS_BOOLEAN, Bool, bool);
                ARRAY_VALUES_GENERATE(ODPS_DATETIME, DatetimeValue, int64_t);
                ARRAY_VALUES_GENERATE(ODPS_DATE, DateValue, int64_t);
                ARRAY_VALUES_GENERATE(ODPS_INTERVAL_YEAR_MONTH, IntervalYearMonthValue, int64_t);
                ARRAY_VALUES_GENERATE(ODPS_STRING, String, std::string);
                ARRAY_VALUES_GENERATE(ODPS_CHAR, Char, std::string);
                ARRAY_VALUES_GENERATE(ODPS_VARCHAR, Varchar, std::string);
                ARRAY_VALUES_GENERATE(ODPS_BINARY, Binary, std::string);
                ARRAY_VALUES_GENERATE(ODPS_DECIMAL, Decimal, std::string);
                ARRAY_VALUES_GENERATE(ODPS_TIMESTAMP, Timestamp, TimeStamp);
                ARRAY_VALUES_GENERATE(ODPS_TIMESTAMP_NTZ, TimestampNTZ, TimeStamp);
                ARRAY_VALUES_GENERATE(ODPS_INTERVAL_DAY_TIME, IntervalDayTimeValue, TimeStamp);
                ARRAY_VALUES_GENERATE(ODPS_ARRAY, Array, IODPSProtoSerializablePtr);
                ARRAY_VALUES_GENERATE(ODPS_MAP, Map, IODPSProtoSerializablePtr);
                ARRAY_VALUES_GENERATE(ODPS_STRUCT, Struct, IODPSProtoSerializablePtr);
                default:
                    throw OdpsTunnelException("Unsupported type in array element: " + GetTypeName(elementType.mType));
            }
        }
        break;
        case ODPS_MAP:
        {
            shared_ptr<ODPSMap> mapValue = dynamic_pointer_cast<ODPSMap>(complexValue);
            shared_ptr<ODPSArray> keyArray = mapValue->GetKeys();
            shared_ptr<ODPSArray> valueArray = mapValue->GetValues();
            WriteComplexValue(keyArray);
            WriteComplexValue(valueArray);
        }
        break;
        case ODPS_STRUCT:
        {
            shared_ptr<ODPSStruct> structValue = dynamic_pointer_cast<ODPSStruct>(complexValue);
            for (uint32_t i = 0; i < structValue->Size(); i++)
            {
                if (structValue->IsNull(i))
                {
                    WireFormatLite::WriteBoolNoTag(true, mCodedOutput);
                    continue;
                }
                const ODPSColumnTypeInfo& memberType = structValue->GetMemberType(i);
                switch(memberType.mType)
                {
                    STRUCT_VALUES_GENERATE(ODPS_BIGINT, BigInt, int64_t);
                    STRUCT_VALUES_GENERATE(ODPS_TINYINT, TinyInt, int8_t);
                    STRUCT_VALUES_GENERATE(ODPS_SMALLINT, SmallInt, int16_t);
                    STRUCT_VALUES_GENERATE(ODPS_INTEGER, Integer, int32_t);
                    STRUCT_VALUES_GENERATE(ODPS_FLOAT, Float, float);
                    STRUCT_VALUES_GENERATE(ODPS_DOUBLE, Double, double);
                    STRUCT_VALUES_GENERATE(ODPS_BOOLEAN, Bool, bool);
                    STRUCT_VALUES_GENERATE(ODPS_DATETIME, DatetimeValue, int64_t);
                    STRUCT_VALUES_GENERATE(ODPS_DATE, DateValue, int64_t);
                    STRUCT_VALUES_GENERATE(ODPS_INTERVAL_YEAR_MONTH, IntervalYearMonthValue, int64_t);
                    STRUCT_VALUES_GENERATE(ODPS_STRING, String, std::string);
                    STRUCT_VALUES_GENERATE(ODPS_CHAR, Char, std::string);
                    STRUCT_VALUES_GENERATE(ODPS_VARCHAR, Varchar, std::string);
                    STRUCT_VALUES_GENERATE(ODPS_BINARY, Binary, std::string);
                    STRUCT_VALUES_GENERATE(ODPS_DECIMAL, Decimal, std::string);
                    STRUCT_VALUES_GENERATE(ODPS_TIMESTAMP, Timestamp, TimeStamp);
                    STRUCT_VALUES_GENERATE(ODPS_TIMESTAMP_NTZ, TimestampNTZ, TimeStamp);
                    STRUCT_VALUES_GENERATE(ODPS_INTERVAL_DAY_TIME, IntervalDayTimeValue, TimeStamp);
                    STRUCT_VALUES_GENERATE(ODPS_ARRAY, Array, IODPSProtoSerializablePtr);
                    STRUCT_VALUES_GENERATE(ODPS_MAP, Map, IODPSProtoSerializablePtr);
                    STRUCT_VALUES_GENERATE(ODPS_STRUCT, Struct, IODPSProtoSerializablePtr);
                    default:
                        throw OdpsTunnelException("Unsupported type in struct member: " + GetTypeName(memberType.mType));
                }
            }
        }
        break;
        default:
        throw OdpsTunnelException("Non-complex type written as complex: " + GetTypeName(type.mType));
    }
}

void ProtoSerializer::Complete()
{
    // for write a empty block
    if (mCodedOutput == NULL)
    {
        mCodedOutput = new CodedOutputStream(mOutput);
    }

    //Write meta
    int32_t crc = 0;
    if (mDoStreamChecksum)
    {
        mCodedChecksum.PutSInt64(META_TOTAL_RECORDS_TAG, mTotalRecords);
        crc = mCodedChecksum.GetChecksum();
    }
    else
    {
        crc = mCrcCrc.checksum();
    }

    WireFormatLite::WriteSInt64(META_TOTAL_RECORDS_TAG, mTotalRecords, mCodedOutput);

    WireFormatLite::WriteUInt32(META_CHECKSUM_TAG, crc, mCodedOutput);

    LOG_DEBUG(sLogger, ("Serialize META", "") ("META_TOTAL_RECORDS_TAG", META_TOTAL_RECORDS_TAG) ("VALUE", mTotalRecords)
                ("META_CHECKSUM_TAG", META_CHECKSUM_TAG) ("VALUE", ToHexString<uint32_t>(mCrcCrc.checksum())));

    delete mCodedOutput;
    mCodedOutput = NULL;

    if (mIsCompress && mOutput != NULL)
    {
        delete mOutput;
        mOutput = NULL;
    }
}

//-----------------------------------------------------------------------------
//
// ProtoDeserializer
//
// ----------------------------------------------------------------------------

ProtoDeserializer::ProtoDeserializer(google::protobuf::io::ZeroCopyInputStream* in,
                ODPSTableSchema* schema,
                bool compressed,
                bool doStreamChecksum,
                int protobufTotalBytesLimit,
                CompressOption* option)
{
    mInput = NULL;
    mCodedInput = NULL;
    mSchema = schema;

    mProtobufTotalBytesLimit = protobufTotalBytesLimit;
    if (mProtobufTotalBytesLimit > PROTOBUF_WARNING_THRESHOLD)
    {
        throw OdpsTunnelException(INTERNAL_ERROR, "protobufTotalBytesLimit > " + std::to_string(PROTOBUF_WARNING_THRESHOLD));
    }

    mTotalRecords = 0;

    mNullMap.reset();
    mGotTotalRecords = false;
    mGotMetrics = false;

    mReceivedTotalRecords = -1;
    mReceivedChecksum = -1;

    if (mSchema->GetColumnCount() > MAX_ODPS_COLUMNS)
    {
        throw OdpsTunnelException(INTERNAL_ERROR, "too more columns:" + std::to_string(mSchema->GetColumnCount()));
    }

    mIsCompressed = compressed;
    if (mIsCompressed)
    {
        if (option == NULL || option->algorithm == CompressOption::ODPS_ZLIB)
        {
            mInput = new GzipInputStream(in);
        }
        else if (option->algorithm == CompressOption::ODPS_ZSTD)
        {
            mInput = new ZstdInputStream(in);
        }
        else if (option->algorithm == CompressOption::ODPS_LZ4_FRAME)
        {
            mInput = new Lz4InputStream(in);
        }
        else
        {
            throw OdpsTunnelException(INTERNAL_ERROR, "unsupported protobuf input compression " + CompressOptionToEncoding(*option));
        }
    }
    else
    {
        mInput = in;
    }
    mBytes = mInput->ByteCount();

    mDoStreamChecksum = doStreamChecksum;
}

bool ProtoDeserializer::Deserialize(ODPSTableRecord& r)
{

    if (mCodedInput == NULL || mInput->ByteCount() - mBytes > mProtobufTotalBytesLimit)
    {
        if (mCodedInput != NULL)
        {
            mBytes = mInput->ByteCount();
            delete mCodedInput;
            mCodedInput = NULL;

            LOG_DEBUG(sLogger, ("New", "CodedInputStream") ("Bytes", mBytes));
        }

        mCodedInput = new CodedInputStream(mInput);
        mCodedInput->SetTotalBytesLimit(PROTOBUF_TOTAL_BYTES_LIMIT, PROTOBUF_WARNING_THRESHOLD);
    }

    LOG_DEBUG(sLogger, ("CodedInputStream", "") ("TotalBytesLimit", mProtobufTotalBytesLimit)
                ("ByteCount", mInput->ByteCount()));

    uint32_t fieldCount = mSchema->GetColumnCount();
    uint32_t tag = 0;
    uint32_t fieldNum = 0;
    uint32_t idx = 0;

    mNullMap.reset(); // reset all columns to NULL
    while (true)
    {
        tag = mCodedInput->ReadTag();
        fieldNum = 0;
        if (tag > 0)
        {
            fieldNum = WireFormatLite::GetTagFieldNumber(tag);
        }

        if (tag <= 0)
        {
            throw OdpsTunnelException(INTERNAL_ERROR, "Read tag error, maybe EOF reached.");
        }

        // -- process special tags --------------------------------------------
        //
        // has got META_TOTAL_RECORDS_TAG but this is not META_CHECKSUM_TAG
        if (mGotTotalRecords)
        {
            // read metrics
            if (fieldNum == 1 && !mGotMetrics)
            {
                std::string metrics;
                if(!WireFormatLite::ReadBytes(mCodedInput, &metrics))
                {
                    throw OdpsTunnelException(INTERNAL_ERROR, "Read metrics string failed");
                }
                mRecordCrc.process_bytes(&fieldNum, sizeof(fieldNum));
                if (mDoStreamChecksum)
                {
                    mCodedChecksum.PutTag(fieldNum, WireFormatLite::WIRETYPE_LENGTH_DELIMITED);
                    mCodedChecksum.PutVarint32(metrics.length());
                    mCodedChecksum.PutRaw(metrics.data(), metrics.length());
                }
                else
                {
                    mRecordCrc.process_bytes(metrics.data(), metrics.length());
                }
                if(!metrics.empty())
                {
                    FromJsonString(mServerMetrics, metrics);
                }
                mGotMetrics = true;
            }
            // read metrics checksum
            else if (fieldNum == METRICS_END_TAG)
            {
                if(!mGotMetrics)
                {
                    throw OdpsTunnelException(INTERNAL_ERROR, "Read METRICS_END_TAG with no metrics.");
                }
                uint32_t crc;
                if (!WireFormatLite::ReadPrimitive<uint32, WireFormatLite::TYPE_UINT32>(mCodedInput, &crc))
                {
                    throw OdpsTunnelException(INTERNAL_ERROR, "Read record crc error.");
                }

                uint32_t checksum = 0;
                if (mDoStreamChecksum)
                {
                    checksum = mCodedChecksum.GetChecksum();
                    mCodedChecksum.PutUInt32(METRICS_END_TAG, checksum);
                }
                else
                {
                    checksum = mRecordCrc.checksum();
                }
                if(checksum != crc)
                {
                    throw OdpsTunnelException(INTERNAL_ERROR, "Record checksum error, received: " + ToHexString<uint32_t>(crc)
                                + ", actually:" + ToHexString<uint32_t>(checksum));
                }
            }
            // read total check sum
            else if (fieldNum == META_CHECKSUM_TAG)
            {
                if (!WireFormatLite::ReadPrimitive<uint32, WireFormatLite::TYPE_UINT32>(mCodedInput, &mReceivedChecksum))
                {
                    throw OdpsTunnelException(INTERNAL_ERROR, "Read crccrc error.");
                }
                LOG_DEBUG(sLogger, ("Deserialize META_CHECKSUM", "") ("META_CHECKSUM_TAG", META_CHECKSUM_TAG)
                            ("VALUE", ToHexString<uint32_t>(mReceivedChecksum)));

                uint32_t checksum = 0;
                if (mDoStreamChecksum)
                {
                    checksum = mCodedChecksum.GetChecksum();
                }
                else
                {
                    checksum = mCrcCrc.checksum();
                }
                if(checksum != mReceivedChecksum)
                {
                    throw OdpsTunnelException(INTERNAL_ERROR, "Record total checksum error, received: " + std::to_string(mReceivedChecksum)
                                + ", actually:" + std::to_string(checksum));
                }
                return false; // reached the end
            }
            else
            {
                throw OdpsTunnelException(INTERNAL_ERROR, "Read tag error, expect META CHECKSUM TAG or METRICS TAG.");
            }
            continue;
        }

        // reached the end of record
        if (fieldNum == RECORD_END_TAG)
        {
            uint32_t crc;
            if (!WireFormatLite::ReadPrimitive<uint32, WireFormatLite::TYPE_UINT32>(mCodedInput, &crc))
            {
                throw OdpsTunnelException(INTERNAL_ERROR, "Read record crc error.");
            }
            LOG_DEBUG(sLogger, ("Deserialize RECORD_END", "") ("RECORD_END_TAG", RECORD_END_TAG)
                        ("VALUE", ToHexString<uint32_t>(crc)));

            uint32_t checksum = 0;
            if (mDoStreamChecksum)
            {
                checksum = mCodedChecksum.GetChecksum();
                mCodedChecksum.PutUInt32(RECORD_END_TAG, checksum);
            }
            else
            {
                checksum = mRecordCrc.checksum();
            }
            if(checksum != crc)
            {
                throw OdpsTunnelException(INTERNAL_ERROR, "Record checksum error, received: " + ToHexString<uint32_t>(crc)
                            + ", actually:" + ToHexString<uint32_t>(checksum));
            }

            // fill NULL columns
            for (uint32_t i = 0 ; i < fieldCount ; ++i)
            {
                if (mNullMap[i] == 0)
                {
                    r.SetNullValue(i);
                }
            }

            ++mTotalRecords;

            if (!mDoStreamChecksum)
            {
                mCrcCrc.process_bytes(&checksum, sizeof(checksum));
                mRecordCrc.reset();
            }

            return true; // got a record
        }
        // got the total records
        else if (fieldNum == META_TOTAL_RECORDS_TAG)
        {
            mGotTotalRecords = true;

            if (!WireFormatLite::ReadPrimitive<int64, WireFormatLite::TYPE_SINT64>(mCodedInput, &mReceivedTotalRecords))
            {
                throw OdpsTunnelException(INTERNAL_ERROR, "Read total records error.");
            }
            LOG_DEBUG(sLogger, ("Deserialize META_TOTAL_RECORDS", "") ("META_TOTAL_RECORDS_TAG", META_TOTAL_RECORDS_TAG)
                        ("VALUE", mReceivedTotalRecords));

            if (mReceivedTotalRecords != mTotalRecords)
            {
                throw OdpsTunnelException(INTERNAL_ERROR, "Record count error, received: " + std::to_string(mReceivedTotalRecords)
                            + ", actually:" + std::to_string(mTotalRecords));
            }

            if (mDoStreamChecksum)
            {
                mCodedChecksum.PutSInt64(META_TOTAL_RECORDS_TAG, mTotalRecords);
            }
            continue;
        }

        // -- end of process special tags -------------------------------------

        // normal column
        if(fieldNum < 1 || fieldNum > fieldCount)
        {
            throw OdpsTunnelException(INTERNAL_ERROR, "Invalid field number." + std::to_string(fieldNum));
        }
        idx = fieldNum - 1;

        if (!mDoStreamChecksum)
        {
            mRecordCrc.process_bytes(&fieldNum, sizeof(fieldNum));
        }

        uint32_t recordIdx = idx;
        mNullMap[recordIdx] = 1; // set the column to not NULL

        ODPSColumnType type = mSchema->GetTableColumn(recordIdx).GetType();
        switch(type)
        {
            case ODPS_BIGINT:
            case ODPS_TINYINT:
            case ODPS_SMALLINT:
            case ODPS_INTEGER:
            case ODPS_DATETIME:
            case ODPS_DATE:
            case ODPS_INTERVAL_YEAR_MONTH:
                {
                    int64_t value = 0;
                    if (!WireFormatLite::ReadPrimitive<int64, WireFormatLite::TYPE_SINT64>(mCodedInput, &value))
                    {
                        throw OdpsTunnelException(INTERNAL_ERROR, "Read int64 failed");
                    }
                    r.SetIntValue(recordIdx, value, type);

                    if (mDoStreamChecksum)
                    {
                        mCodedChecksum.PutSInt64(fieldNum, value);
                    }
                    else
                    {
                        mRecordCrc.process_bytes(&value, sizeof(value));
                    }
                    break;
                }
            case ODPS_JSON:
            case ODPS_STRING:
            case ODPS_CHAR:
            case ODPS_VARCHAR:
            case ODPS_BINARY:
            case ODPS_DECIMAL:
                {
                    std::string value;
                    if(!WireFormatLite::ReadBytes(mCodedInput, &value))
                    {
                        throw OdpsTunnelException(INTERNAL_ERROR, "Read string failed");
                    }
                    LOG_DEBUG(sLogger, ("Deserialize V_STRING", value.data()) ("Length", value.length()));
                    r.SetStringValue(recordIdx, value.data(), value.length(), type);

                    if (mDoStreamChecksum)
                    {
                        mCodedChecksum.PutTag(fieldNum, WireFormatLite::WIRETYPE_LENGTH_DELIMITED);
                        mCodedChecksum.PutVarint32(value.length());
                        mCodedChecksum.PutRaw(value.data(), value.length());
                    }
                    else
                    {
                        mRecordCrc.process_bytes(value.data(), value.length());
                    }

                    break;
                }
            case ODPS_BOOLEAN:
                {
                    bool value = false;
                    if (!WireFormatLite::ReadPrimitive<bool, WireFormatLite::TYPE_BOOL>(mCodedInput, &value))
                    {
                        throw OdpsTunnelException(INTERNAL_ERROR, "Read bool failed");
                    }
                    r.SetBoolValue(recordIdx, value);

                    if (mDoStreamChecksum)
                    {
                        mCodedChecksum.PutBool(fieldNum, value);
                    }
                    else
                    {
                        mRecordCrc.process_bytes(&value, sizeof(value));
                    }

                    break;
                }
            case ODPS_DOUBLE:
                {
                    double value = 0;
                    if (!WireFormatLite::ReadPrimitive<double, WireFormatLite::TYPE_DOUBLE>(mCodedInput, &value))
                    {
                        throw OdpsTunnelException(INTERNAL_ERROR, "Read double failed");
                    }
                    r.SetDoubleValue(recordIdx, value);

                    if (mDoStreamChecksum)
                    {
                        mCodedChecksum.PutDouble(fieldNum, value);
                    }
                    else
                    {
                        mRecordCrc.process_bytes(&value, sizeof(value));
                    }

                    break;
                }
            case ODPS_FLOAT:
                {
                    float value = 0;
                    if (!WireFormatLite::ReadPrimitive<float, WireFormatLite::TYPE_FLOAT>(mCodedInput, &value))
                    {
                        throw OdpsTunnelException(INTERNAL_ERROR, "Read double failed");
                    }
                    r.SetFloatValue(recordIdx, value);

                    if (mDoStreamChecksum)
                    {
                        mCodedChecksum.PutDouble(fieldNum, value);
                    }
                    else
                    {
                        mRecordCrc.process_bytes(&value, sizeof(value));
                    }

                    break;
                }
            case ODPS_TIMESTAMP:
            case ODPS_TIMESTAMP_NTZ:
            case ODPS_INTERVAL_DAY_TIME:
                {
                    int64_t second;
                    int32_t nano;
                    if (!WireFormatLite::ReadPrimitive<int64, WireFormatLite::TYPE_SINT64>(mCodedInput, &second)
                      || !WireFormatLite::ReadPrimitive<int32, WireFormatLite::TYPE_SINT32>(mCodedInput, &nano))
                    {
                        throw OdpsTunnelException(INTERNAL_ERROR, "Read type TimeStamp error.");
                    }

                    mRecordCrc.process_bytes(&second, sizeof(second)); // crc
                    mRecordCrc.process_bytes(&nano, sizeof(nano)); // crc
                    r.SetTimeValue(recordIdx, second, nano, type);
                    break;
                }
            case ODPS_ARRAY:
                {
                    ODPSColumnTypeInfo typeinfo = r.GetSchema()->GetTableColumn(recordIdx).GetTypeInfo();
                    shared_ptr<ODPSArray> arrayValue = dynamic_pointer_cast<ODPSArray>(ReadComplexValue(typeinfo));
                    r.SetArrayValue(recordIdx, arrayValue);
                }
                break;
            case ODPS_MAP:
                {
                    ODPSColumnTypeInfo typeinfo = r.GetSchema()->GetTableColumn(recordIdx).GetTypeInfo();
                    shared_ptr<ODPSMap> mapValue = dynamic_pointer_cast<ODPSMap>(ReadComplexValue(typeinfo));
                    r.SetMapValue(recordIdx, mapValue);
                }
                break;
            case ODPS_STRUCT:
                {
                    ODPSColumnTypeInfo typeinfo = r.GetSchema()->GetTableColumn(recordIdx).GetTypeInfo();
                    shared_ptr<ODPSStruct> structValue = dynamic_pointer_cast<ODPSStruct>(ReadComplexValue(typeinfo));
                    r.SetStructValue(recordIdx, structValue);
                }
                break;
            default:
                throw OdpsTunnelException(INTERNAL_ERROR, "Invalid Type At index:" + std::to_string((recordIdx)));
                break;
        }
        LOG_DEBUG(sLogger, ("Deserialize FIELD", "") ("IDX", fieldNum)("CRC", ToHexString<uint32_t>(mRecordCrc.checksum())) );
    }

    return false;
}

#define READ_PRIM_GOOD_OR_FAIL(WIRETYPE, CTYPE, SAVETO) \
if (!WireFormatLite::ReadPrimitive<CTYPE, WireFormatLite::WIRETYPE>(mCodedInput, &SAVETO)) \
{ \
    throw OdpsTunnelException("Cannot Deserialize: read " + string(#CTYPE) + " failed."); \
}

void ProtoDeserializer::ReadNestedValue(shared_ptr<void>* value, const ODPSColumnTypeInfo& type)
{
    bool isNull = false;
    if (!WireFormatLite::ReadPrimitive<bool, WireFormatLite::TYPE_BOOL>(mCodedInput, &isNull))
    {
        throw OdpsTunnelException("Cannot Deserialize: read null indicator failed.");
    }
    if (isNull)
    {
        return;
    }

    switch(type.mType)
    {
        case ODPS_TINYINT:
        case ODPS_SMALLINT:
        case ODPS_INTEGER:
        case ODPS_BIGINT:
        case ODPS_DATE:
        case ODPS_DATETIME:
        case ODPS_INTERVAL_YEAR_MONTH:
            {
                int64_t int64Value;
                READ_PRIM_GOOD_OR_FAIL(TYPE_SINT64, int64_t, int64Value);
                PrimitiveTypeCrc(mRecordCrc, int64Value);
                if (ODPS_TINYINT == type.mType)
                {
                    *value = make_shared<int8_t>(int64Value);
                }
                else if (ODPS_SMALLINT == type.mType)
                {
                    *value = make_shared<int16_t>(int64Value);
                }
                else if (ODPS_INTEGER == type.mType)
                {
                    *value = make_shared<int32_t>(int64Value);
                }
                else if (ODPS_DATE == type.mType)
                {
                    *value = make_shared<int64_t>(EncodeDate(int64Value));
                }
                else
                {
                    *value = make_shared<int64_t>(int64Value);
                }
                break;
            }
        case ODPS_FLOAT:
            {
                float floatValue;
                READ_PRIM_GOOD_OR_FAIL(TYPE_FLOAT, float, floatValue);
                PrimitiveTypeCrc(mRecordCrc, floatValue);
                *value = make_shared<float>(floatValue);
                break;
            }
        case ODPS_DOUBLE:
            {
                double doubleValue;
                READ_PRIM_GOOD_OR_FAIL(TYPE_DOUBLE, double, doubleValue);
                PrimitiveTypeCrc(mRecordCrc, doubleValue);
                *value = make_shared<double>(doubleValue);
                break;
            }
        case ODPS_BOOLEAN:
            {
                bool boolValue;
                READ_PRIM_GOOD_OR_FAIL(TYPE_BOOL, bool, boolValue);
                PrimitiveTypeCrc(mRecordCrc, boolValue);
                *value = make_shared<bool>(boolValue);
                break;
            }
        case ODPS_JSON:
        case ODPS_CHAR:
        case ODPS_VARCHAR:
        case ODPS_STRING:
        case ODPS_BINARY:
            {
                std::string stringValue;
                if (!WireFormatLite::ReadString(mCodedInput, &stringValue))
                {
                    throw OdpsTunnelException("Cannot Deserialize: read string failed.");
                }
                // will not do padding to STRING & BINARY.
                mRecordCrc.process_bytes(stringValue.c_str(), stringValue.size());
                TrimOrPadding(stringValue, type);
                *value = make_shared<string>(stringValue);
                break;
            }
        case ODPS_TIMESTAMP:
        case ODPS_TIMESTAMP_NTZ:
        case ODPS_INTERVAL_DAY_TIME:
            {
                int64_t sec;
                int32_t nano;
                TimeStamp ts;
                READ_PRIM_GOOD_OR_FAIL(TYPE_SINT64, int64_t, sec);
                READ_PRIM_GOOD_OR_FAIL(TYPE_SINT32, int32_t, nano);
                PrimitiveTypeCrc(mRecordCrc, sec);
                PrimitiveTypeCrc(mRecordCrc, nano);
                *value = make_shared<TimeStamp>(sec, nano);
                break;
            }
        case ODPS_DECIMAL:
            {
                // TODO: replace with REAL decimal types.
                std::string stringValue;
                if (!WireFormatLite::ReadString(mCodedInput, &stringValue))
                {
                    throw OdpsTunnelException("Cannot Deserialize: read string failed.");
                }
                mRecordCrc.process_bytes(stringValue.c_str(), stringValue.size());
                *value = make_shared<string>(stringValue);
                break;
            }
        case ODPS_ARRAY:
        case ODPS_MAP:
        case ODPS_STRUCT:
            {
                *value = make_shared<IODPSProtoSerializablePtr>(ReadComplexValue(type));
                break;
            }
        default:
            {
                LOG_ERROR(sLogger, ("unsupported value type", type.mType));
                throw OdpsTunnelException("Unsupported value type in deserialization: " + std::to_string(type.mType));
            }
    }
    LOG_DEBUG(sLogger, ("Deserialize NVALUE", "") ("CRC", ToHexString<uint32_t>(mRecordCrc.checksum())));
}

#define ARRAY_READ_VALUES(ODPSTYPE, SIGNTYPE, CTYPE) \
case ODPSTYPE: \
{ \
    for(uint32_t i = 0; i < length; i++) \
    { \
        shared_ptr<void> buffer; \
        ReadNestedValue(&buffer, elementType); \
        if (buffer) \
        { \
            arrayValue->Append##SIGNTYPE##Value(*static_pointer_cast<CTYPE>(buffer)); \
        } \
        else \
        { \
            arrayValue->AppendNull(); \
        } \
    } \
} \
break

#define ARRAY_READ_VALUES_COMPLEX(ODPSTYPE, SIGNTYPE, CTYPE) \
case ODPSTYPE: \
{ \
    for(uint32_t i = 0; i < length; i++) \
    { \
        shared_ptr<void> buffer; \
        ReadNestedValue(&buffer, elementType); \
        if (buffer) \
        { \
            arrayValue->Append##SIGNTYPE##Value(dynamic_pointer_cast<CTYPE>(*static_pointer_cast<IODPSProtoSerializablePtr>(buffer))); \
        } \
        else \
        { \
            arrayValue->AppendNull(); \
        } \
    } \
} \
break

#define STRUCT_READ_VALUES(ODPSTYPE, SIGNTYPE, CTYPE) \
case ODPSTYPE: \
{ \
    shared_ptr<void> buffer; \
    ReadNestedValue(&buffer, memberType); \
    if (buffer) \
    { \
        structValue->Set##SIGNTYPE##Value(i, *static_pointer_cast<CTYPE>(buffer)); \
    } \
} \
break

#define STRUCT_READ_VALUES_COMPLEX(ODPSTYPE, SIGNTYPE, CTYPE) \
case ODPSTYPE: \
{ \
    shared_ptr<void> buffer; \
    ReadNestedValue(&buffer, memberType); \
    if (buffer) \
    { \
        structValue->Set##SIGNTYPE##Value(i, dynamic_pointer_cast<CTYPE>(*static_pointer_cast<IODPSProtoSerializablePtr>(buffer))); \
    } \
} \
break


IODPSProtoSerializablePtr ProtoDeserializer::ReadComplexValue(const ODPSColumnTypeInfo& type)
{
    switch(type.mType)
    {
        case ODPS_ARRAY:
        {
            shared_ptr<ODPSArray> arrayValue = make_shared<ODPSArray>(type);
            uint32_t length;
            READ_PRIM_GOOD_OR_FAIL(TYPE_UINT32, uint32_t, length);
            LOG_DEBUG(sLogger, ("Read ARRAY length", length));
            ODPSColumnTypeInfo elementType = arrayValue->GetTypeInfo().mSubTypes[0];
            switch(elementType.mType)
            {
                ARRAY_READ_VALUES(ODPS_BIGINT, BigInt, int64_t);
                ARRAY_READ_VALUES(ODPS_TINYINT, TinyInt, int8_t);
                ARRAY_READ_VALUES(ODPS_SMALLINT, SmallInt, int16_t);
                ARRAY_READ_VALUES(ODPS_INTEGER, Integer, int32_t);
                ARRAY_READ_VALUES(ODPS_FLOAT, Float, float);
                ARRAY_READ_VALUES(ODPS_DOUBLE, Double, double);
                ARRAY_READ_VALUES(ODPS_BOOLEAN, Bool, bool);
                ARRAY_READ_VALUES(ODPS_DATETIME, Datetime, int64_t);
                ARRAY_READ_VALUES(ODPS_DATE, Date, int64_t);
                ARRAY_READ_VALUES(ODPS_INTERVAL_YEAR_MONTH, IntervalYearMonth, int64_t);
                ARRAY_READ_VALUES(ODPS_STRING, String, std::string);
                ARRAY_READ_VALUES(ODPS_CHAR, Char, std::string);
                ARRAY_READ_VALUES(ODPS_VARCHAR, Varchar, std::string);
                ARRAY_READ_VALUES(ODPS_BINARY, Binary, std::string);
                ARRAY_READ_VALUES(ODPS_DECIMAL, Decimal, std::string);
                ARRAY_READ_VALUES(ODPS_TIMESTAMP, Timestamp, TimeStamp);
                ARRAY_READ_VALUES(ODPS_TIMESTAMP_NTZ, TimestampNTZ, TimeStamp);
                ARRAY_READ_VALUES(ODPS_INTERVAL_DAY_TIME, IntervalDayTime, TimeStamp);
                ARRAY_READ_VALUES_COMPLEX(ODPS_ARRAY, Array, ODPSArray);
                ARRAY_READ_VALUES_COMPLEX(ODPS_MAP, Map, ODPSMap);
                ARRAY_READ_VALUES_COMPLEX(ODPS_STRUCT, Struct, ODPSStruct);
                default:
                    throw OdpsTunnelException("Unsupported type in array element: " + GetTypeName(elementType.mType));
            }
            return arrayValue;
        }
        break;
        case ODPS_MAP:
        {
            shared_ptr<ODPSMap> mapValue = make_shared<ODPSMap>(type);
            ODPSColumnTypeInfo keyArrayType = mapValue->GetKeys()->GetTypeInfo();
            ODPSColumnTypeInfo valueArrayType = mapValue->GetValues()->GetTypeInfo();
            shared_ptr<ODPSArray> keyArray = dynamic_pointer_cast<ODPSArray>(ReadComplexValue(keyArrayType));
            shared_ptr<ODPSArray> valueArray = dynamic_pointer_cast<ODPSArray>(ReadComplexValue(valueArrayType));
            mapValue->Recover(keyArray, valueArray);
            return mapValue;
        }
        break;
        case ODPS_STRUCT:
        {
            shared_ptr<ODPSStruct> structValue = make_shared<ODPSStruct>(type);
            for (uint32_t i = 0; i < structValue->Size(); i++)
            {
                const ODPSColumnTypeInfo memberType = structValue->GetMemberType(i);
                switch(memberType.mType)
                {
                    STRUCT_READ_VALUES(ODPS_BIGINT, BigInt, int64_t);
                    STRUCT_READ_VALUES(ODPS_TINYINT, TinyInt, int8_t);
                    STRUCT_READ_VALUES(ODPS_SMALLINT, SmallInt, int16_t);
                    STRUCT_READ_VALUES(ODPS_INTEGER, Integer, int32_t);
                    STRUCT_READ_VALUES(ODPS_FLOAT, Float, float);
                    STRUCT_READ_VALUES(ODPS_DOUBLE, Double, double);
                    STRUCT_READ_VALUES(ODPS_BOOLEAN, Bool, bool);
                    STRUCT_READ_VALUES(ODPS_DATETIME, Datetime, int64_t);
                    STRUCT_READ_VALUES(ODPS_DATE, Date, int64_t);
                    STRUCT_READ_VALUES(ODPS_INTERVAL_YEAR_MONTH, IntervalYearMonth, int64_t);
                    STRUCT_READ_VALUES(ODPS_STRING, String, std::string);
                    STRUCT_READ_VALUES(ODPS_CHAR, Char, std::string);
                    STRUCT_READ_VALUES(ODPS_VARCHAR, Varchar, std::string);
                    STRUCT_READ_VALUES(ODPS_BINARY, Binary, std::string);
                    STRUCT_READ_VALUES(ODPS_DECIMAL, Decimal, std::string);
                    STRUCT_READ_VALUES(ODPS_TIMESTAMP, Timestamp, TimeStamp);
                    STRUCT_READ_VALUES(ODPS_TIMESTAMP_NTZ, TimestampNTZ, TimeStamp);
                    STRUCT_READ_VALUES(ODPS_INTERVAL_DAY_TIME, IntervalDayTime, TimeStamp);
                    STRUCT_READ_VALUES_COMPLEX(ODPS_ARRAY, Array, ODPSArray);
                    STRUCT_READ_VALUES_COMPLEX(ODPS_MAP, Map, ODPSMap);
                    STRUCT_READ_VALUES_COMPLEX(ODPS_STRUCT, Struct, ODPSStruct);
                    default:
                        throw OdpsTunnelException("Unsupported type in struct member: " + GetTypeName(memberType.mType));
                }
            }
            return structValue;
        }
        break;
        default:
        throw OdpsTunnelException("Deserializing non-complex type in readcomplex: " + GetTypeName(type.mType));
    }
}
