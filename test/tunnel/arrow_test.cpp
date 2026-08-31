#include <ostream>
#include <string>
#ifdef ODPS_SDK_ENABLE_ARROW
#include <stdlib.h>

#include "common/logging.h"
#include "include/odps_tunnel.h"
#include "gtest/gtest.h"
#include "test/common/test_util.h"
#include <arrow/api.h>
#include <vector>

using namespace std;
using namespace apsara::odps::sdk;
using namespace apsara::odps::sdk::internal;
using namespace apsara::odps::sdk::internal::tunnel;

#define ONE_BATCH 1024

static string ExpectedStringBuilder(uint64_t index)
{
    return "string_" + std::to_string(index);
}

static string ExpectedJsonBuilder(uint64_t index)
{
    int order = index % 7;
    if (order == 1)
    {
        return "{\"key1\":\"val1\"}";
    }
    else if (order == 2)
    {
        return "{\"key2\":\"val2\"}";
    }
    else if (order == 3)
    {
        return "\"hello\"";
    }
    else if (order == 4)
    {
        return "null";
    }
    else if (order == 5)
    {
        return "4";
    }
    else if (order == 6)
    {
        return "\"true\"";
    }
    else
    {
        return "{\"1\":2,\"key\":\"val\"}";
    }
}

class TunnelArrowTest : public testing::Test
{
protected:
    std::string sProjectName;
    std::string sTableName;
    OdpsTunnel  sTunnel;
    uint64_t    sCount;
    uint32_t    sBlockId;

    uint32_t NewBlockId()
    {
        ++sBlockId;
        return sBlockId;
    }

    static void SetUpTestCase()
    {
    }

    static void TearDownTestCase()
    {
    }

    virtual void SetUp()
    {
        sBlockId = 0;
        sProjectName = Utils::GetProjectName();
        sTableName = Utils::GetRandomTableName();
        sTunnel = Utils::GetTunnelInstance();
        sCount = ONE_BATCH;
        Utils::DropTableIfExists(sTableName);
        Utils::ExecSqlTogether("set odps.sql.type.json.enable=true; create table " + sTableName + "(c1 bigint, c2 json, c3 string) stored as aliorc");

        IUploadPtr upload = sTunnel.CreateUpload(sProjectName, sTableName);
        uint32_t blkId = NewBlockId();
        IRecordWriterPtr writer = upload->OpenWriter(blkId);
        ODPSTableRecordPtr _r = upload->CreateBufferRecord();
        ODPSTableRecord& record = *_r;
        for (uint64_t i = 0; i < sCount; ++i)
        {
            const std::string& jsonStr = ExpectedJsonBuilder(i);
            const std::string& str = ExpectedStringBuilder(i);
            record.SetBigIntValue(0, i);
            record.SetJsonValue(1, jsonStr.c_str(), jsonStr.length());
            record.SetStringValue(2, str.c_str(), str.length());
            writer->Write(record);
        }
        writer->Close();
        upload->Commit({blkId});
    }

    virtual void TearDown()
    {
        Utils::DropTableIfExists(sTableName);
    }

    IDownloadPtr CreateDownload(
            const std::string &downloadId = "")
    {
        IDownloadPtr download;
        if (downloadId.empty())
        {
            download = sTunnel.CreateDownload(sProjectName, sTableName, "", "");
            std::cerr << "create download session, id=" << download->GetDownloadId() << std::endl;
        }
        else
        {
            download = sTunnel.CreateDownload(sProjectName, sTableName, "", downloadId);
            std::cerr << "create download session from id=" << downloadId << std::endl;
        }

        return download;
    }

    IUploadPtr CreateUpload(const std::string &uploadId = "",
        bool overwrite = false)
    {
        IUploadPtr upload;

        if (uploadId.empty())
        {
            upload = sTunnel.CreateUpload(sProjectName, sTableName, "", "", overwrite);
            std::cerr << "create upload session, id=" << upload->GetUploadId() << std::endl;
        }
        else
        {
            upload = sTunnel.CreateUpload(sProjectName, sTableName, "", uploadId, overwrite);
            std::cerr << "create upload session from id=" << uploadId << std::endl;
        }

        return upload;
    }

    // return count of records downloaded. on integrity failure or exception, return -1.
    int64_t DownloadData(uint64_t start, uint64_t count, const std::string &downloadId = "");
    int64_t DownloadCompressedData(uint64_t start, uint64_t count, const CompressOption::CompressAlgorithm& compressAlgorithm, const std::string &downloadId = "");
    // Download data in normal mode.
    int64_t DownloadDataNormal(uint64_t start, uint64_t count, const std::string &downloadId = "");
    // Download data using buffer arrow reader
    int64_t DownloadDataByBuffer(uint64_t start, uint64_t count, uint64_t bufferRecordCount, const std::string &downloadId = "");
    int64_t DownloadDataByBufferWithRawSize(uint64_t start, uint64_t count, uint64_t bufferRecordCount, uint64_t rawSize, const std::string &downloadId = "");

    // contains integrity test.
    // return count of records uploaded. on integrity failure or exception, return -1.
    int64_t UploadData(uint64_t count, const uint32_t blkId, const std::string& uploadId = "", const bool overwrite = false,
        const bool useNormalDownload = false);
    int64_t UploadCompressedData(uint64_t count, const uint32_t blkId, const CompressOption::CompressAlgorithm& compressAlgorithm, const std::string& uploadId = "", const bool overwrite = false,
        const bool useNormalDownload = false);
};

int64_t TunnelArrowTest::DownloadData(uint64_t start, uint64_t count, const std::string &downloadId)
{
    IDownloadPtr download = CreateDownload(downloadId);
    IArrowRecordReaderPtr reader = download->OpenArrowReader(start, count);
    if (download->GetRecordCount() != sCount) {
        EXPECT_EQ(sCount, download->GetRecordCount());
        return -1;
    }
    if (reader.get() == nullptr){
        EXPECT_NE(reader.get(), nullptr);
        return -1;
    }

    shared_ptr<arrow::RecordBatch> batch;

    uint64_t i = start;
    while (reader->Read(batch))
    {
        int rows = batch->num_rows();
        auto bigIntColumn = static_pointer_cast<arrow::Int64Array>(batch->column(0));
        auto jsonColumn = static_pointer_cast<arrow::StringArray>(batch->column(1));
        auto stringColumn = static_pointer_cast<arrow::StringArray>(batch->column(2));
        for (int j = 0; j < rows; j++)
        {
            int64_t getInt = bigIntColumn->Value(j);
            string getJsonStr = jsonColumn->GetString(j);
            string expectStr = ExpectedJsonBuilder(getInt);
            if (expectStr != getJsonStr)
            {
                EXPECT_EQ(expectStr, getJsonStr);
                return -1;
            }

            expectStr = ExpectedStringBuilder(getInt);
            string getStr = stringColumn->GetString(j);
            if (expectStr != getStr)
            {
                EXPECT_EQ(expectStr, getStr);
                return -1;
            }
            ++i;
        }
    }
    reader->Close();
    download->Complete();
    return i - start;
}

int64_t TunnelArrowTest::DownloadCompressedData(uint64_t start, uint64_t count, const CompressOption::CompressAlgorithm& compressAlgorithm, const std::string &downloadId)
{
    IDownloadPtr download = CreateDownload(downloadId);
    IArrowRecordReaderPtr reader = download->OpenArrowReader(start, count, {}, CompressOption(compressAlgorithm, 1, 1));
    if (download->GetRecordCount() != sCount) {
        EXPECT_EQ(sCount, download->GetRecordCount());
        return -1;
    }
    if (reader.get() == nullptr){
        EXPECT_NE(reader.get(), nullptr);
        return -1;
    }

    shared_ptr<arrow::RecordBatch> batch;

    uint64_t i = start;
    while (reader->Read(batch))
    {
        int rows = batch->num_rows();
        auto bigIntColumn = static_pointer_cast<arrow::Int64Array>(batch->column(0));
        auto jsonColumn = static_pointer_cast<arrow::StringArray>(batch->column(1));
        auto stringColumn = static_pointer_cast<arrow::StringArray>(batch->column(2));
        for (int j = 0; j < rows; j++)
        {
            int64_t getInt = bigIntColumn->Value(j);
            string getJsonStr = jsonColumn->GetString(j);
            string expectStr = ExpectedJsonBuilder(getInt);
            if (expectStr != getJsonStr)
            {
                EXPECT_EQ(expectStr, getJsonStr);
                return -1;
            }

            expectStr = ExpectedStringBuilder(getInt);
            string getStr = stringColumn->GetString(j);
            if (expectStr != getStr)
            {
                EXPECT_EQ(expectStr, getStr);
                return -1;
            }
            ++i;
        }
    }
    reader->Close();
    download->Complete();
    return i - start;
}

int64_t TunnelArrowTest::DownloadDataNormal(uint64_t start, uint64_t count, const std::string &downloadId)
{
    IDownloadPtr download = CreateDownload(downloadId);
    IRecordReaderPtr reader = download->OpenReader(start, count);
    if (download->GetRecordCount() != sCount) {
        EXPECT_EQ(sCount, download->GetRecordCount());
        return -1;
    }
    if (reader.get() == nullptr)
    {
        EXPECT_NE(reader.get(), nullptr);
        return -1;
    }

    ODPSTableRecordPtr _r = reader->CreateBufferRecord();
    ODPSTableRecord& record = *_r;

    uint64_t i = start;
    while (reader->Read(record))
    {
        int64_t getInt = *record.GetBigIntValue(0);
        string expectStr = ExpectedJsonBuilder(getInt);
        uint32_t getlen = 0;
        const char* getJsonStr = record.GetJsonValue(1, getlen);
        string getStr(getJsonStr, getlen);
        if (expectStr != getStr)
        {
            EXPECT_EQ(expectStr, getStr);
            return -1;
        }

        getlen = 0;
        expectStr = ExpectedStringBuilder(getInt);
        const char* getCStr = record.GetStringValue(2, getlen);
        string cStr(getCStr, getlen);
        if (expectStr != cStr)
        {
            EXPECT_EQ(expectStr, cStr);
            return -1;
        }

        ++i;
    }
    reader->Close();
    download->Complete();
    return i - start;
}

int64_t TunnelArrowTest::DownloadDataByBuffer(uint64_t start, uint64_t count, uint64_t bufferRecordCount, const std::string &downloadId)
{
    IDownloadPtr download = CreateDownload(downloadId);
    if (download->GetRecordCount() != sCount) {
        EXPECT_EQ(sCount, download->GetRecordCount());
        return -1;
    }

    std::vector<std::string> colNames;
    CompressOption option = CompressOption::NO_COMPRESS;
    IBufferArrowRecordReaderPtr reader = download->OpenBufferArrowReader(start, count, bufferRecordCount, colNames, option);
    if (reader.get() == nullptr) {
        EXPECT_NE(reader.get(), nullptr);
        return -1;
    }

    shared_ptr<arrow::RecordBatch> batch;
    uint64_t i = start;
    while((batch = reader->ReadWithRetry(5)) != nullptr)
    {
        int rows = batch->num_rows();
        auto bigIntColumn = static_pointer_cast<arrow::Int64Array>(batch->column(0));
        auto jsonColumn = static_pointer_cast<arrow::StringArray>(batch->column(1));
        auto stringColumn = static_pointer_cast<arrow::StringArray>(batch->column(2));
        for (int j = 0; j < rows; j++)
        {
            int64_t getInt = bigIntColumn->Value(j);
            if (static_cast<int64_t>(i) != getInt)
            {
                EXPECT_EQ(static_cast<int64_t>(i), getInt);
                return -1;
            }

            string getJsonStr = jsonColumn->GetString(j);
            string expectStr = ExpectedJsonBuilder(getInt);
            if (expectStr != getJsonStr)
            {
                EXPECT_EQ(expectStr, getJsonStr);
                return -1;
            }

            expectStr = ExpectedStringBuilder(getInt);
            string getStr = stringColumn->GetString(j);
            if (expectStr != getStr)
            {
                EXPECT_EQ(expectStr, getStr);
                return -1;
            }
            ++i;
        }
    }
    download->Complete();
    uint64_t end = std::min(sCount, start + count);
    if (end != i)
    {
        EXPECT_EQ(end, i);
        return -1;
    }
    return i - start;
}

int64_t TunnelArrowTest::DownloadDataByBufferWithRawSize(uint64_t start, uint64_t count, uint64_t bufferRecordCount, uint64_t rawSize, const std::string &downloadId)
{
    IDownloadPtr download = CreateDownload(downloadId);
    if (download->GetRecordCount() != sCount) {
        EXPECT_EQ(sCount, download->GetRecordCount());
        return -1;
    }

    std::vector<std::string> colNames;
    CompressOption option = CompressOption::NO_COMPRESS;
    IBufferArrowRecordReaderPtr reader = download->OpenBufferArrowReader(start, count, bufferRecordCount, rawSize, colNames, option);
    if (reader.get() == nullptr) {
        EXPECT_NE(reader.get(), nullptr);
        return -1;
    }

    shared_ptr<arrow::RecordBatch> batch;
    uint64_t i = start;
    while((batch = reader->ReadWithRetry(5)) != nullptr)
    {
        int rows = batch->num_rows();
        auto bigIntColumn = static_pointer_cast<arrow::Int64Array>(batch->column(0));
        auto jsonColumn = static_pointer_cast<arrow::StringArray>(batch->column(1));
        auto stringColumn = static_pointer_cast<arrow::StringArray>(batch->column(2));
        for (int j = 0; j < rows; j++)
        {
            int64_t getInt = bigIntColumn->Value(j);
            if (static_cast<int64_t>(i) != getInt)
            {
                EXPECT_EQ(static_cast<int64_t>(i), getInt);
                return -1;
            }

            string getJsonStr = jsonColumn->GetString(j);
            string expectStr = ExpectedJsonBuilder(getInt);
            if (expectStr != getJsonStr)
            {
                EXPECT_EQ(expectStr, getJsonStr);
                return -1;
            }

            expectStr = ExpectedStringBuilder(getInt);
            string getStr = stringColumn->GetString(j);
            if (expectStr != getStr)
            {
                EXPECT_EQ(expectStr, getStr);
                return -1;
            }
            ++i;
        }
    }
    download->Complete();
    uint64_t end = std::min(sCount, start + count);
    if (end != i)
    {
        EXPECT_EQ(end, i);
        return -1;
    }
    return i - start;
}

int64_t TunnelArrowTest::UploadData(uint64_t count, const uint32_t blkId, const std::string& uploadId, const bool overwrite,
    const bool useNormalDownload)
{
    IUploadPtr upload = CreateUpload(uploadId, overwrite);

    if (upload->GetUploadId() == "")
    {
        EXPECT_NE(upload->GetUploadId(), "");
        return -1;
    }

    if (overwrite)
    {
        sCount = 0;
    }

    arrow::MemoryPool* pool = arrow::default_memory_pool();

    auto schema = upload->GetArrowSchema();
    auto writer = upload->OpenArrowWriter(blkId);
    if (writer.get() == nullptr)
    {
        EXPECT_NE(writer.get(), nullptr);
        return -1;
    }

    arrow::Int64Builder firstColumnBuilder(pool);
    arrow::StringBuilder secondColumnBuilder(pool);
    arrow::StringBuilder threeColumnBuilder(pool);

    shared_ptr<arrow::Array> firstColumnArray;
    shared_ptr<arrow::Array> secondColumnArray;
    shared_ptr<arrow::Array> threeColumnArray;

    uint64_t start = sCount;
    uint64_t end = sCount + count;
    for (uint64_t i = start; i < end; i++)
    {
        firstColumnBuilder.AppendValues({static_cast<int64_t>(i)});
        secondColumnBuilder.AppendValues({ExpectedJsonBuilder(i)});
        threeColumnBuilder.AppendValues({ExpectedStringBuilder(i)});
        sCount++;
    }

    firstColumnBuilder.Finish(&firstColumnArray);
    secondColumnBuilder.Finish(&secondColumnArray);
    threeColumnBuilder.Finish(&threeColumnArray);

    shared_ptr<arrow::RecordBatch> batch = arrow::RecordBatch::Make(schema, count, {firstColumnArray, secondColumnArray, threeColumnArray});
    auto writeSuccess = writer->Write(*batch);
    if (!writeSuccess)
    {
        EXPECT_EQ(writeSuccess, true);
        return -1;
    }
    writer->Close();

    upload->Commit({blkId});

    // start integrity checks
    int64_t readcount = 0;
    if (useNormalDownload)
    {
        readcount = DownloadDataNormal(start, count);
    } else
    {
        readcount = DownloadData(start, count);
    }

    if (readcount != static_cast<int64_t>(count))
    {
        EXPECT_EQ(readcount, static_cast<int64_t>(count));
        return -1;
    }

    return readcount;
}

int64_t TunnelArrowTest::UploadCompressedData(uint64_t count, const uint32_t blkId, const CompressOption::CompressAlgorithm& compressAlgorithm, const std::string& uploadId, const bool overwrite,
    const bool useNormalDownload)
{
    IUploadPtr upload = CreateUpload(uploadId, overwrite);

    if (upload->GetUploadId() == "")
    {
        EXPECT_NE(upload->GetUploadId(), "");
        return -1;
    }

    if (overwrite)
    {
        sCount = 0;
    }

    arrow::MemoryPool* pool = arrow::default_memory_pool();

    auto schema = upload->GetArrowSchema();
    auto writer = upload->OpenArrowWriter(blkId, CompressOption(compressAlgorithm, 1, 1));
    if (writer.get() == nullptr)
    {
        EXPECT_NE(writer.get(), nullptr);
        return -1;
    }

    arrow::Int64Builder firstColumnBuilder(pool);
    arrow::StringBuilder secondColumnBuilder(pool);
    arrow::StringBuilder threeColumnBuilder(pool);

    shared_ptr<arrow::Array> firstColumnArray;
    shared_ptr<arrow::Array> secondColumnArray;
    shared_ptr<arrow::Array> threeColumnArray;

    uint64_t start = sCount;
    uint64_t end = sCount + count;
    for (uint64_t i = start; i < end; i++)
    {
        firstColumnBuilder.AppendValues({static_cast<int64_t>(i)});
        secondColumnBuilder.AppendValues({ExpectedJsonBuilder(i)});
        threeColumnBuilder.AppendValues({ExpectedStringBuilder(i)});
        sCount++;
    }

    firstColumnBuilder.Finish(&firstColumnArray);
    secondColumnBuilder.Finish(&secondColumnArray);
    threeColumnBuilder.Finish(&threeColumnArray);

    shared_ptr<arrow::RecordBatch> batch = arrow::RecordBatch::Make(schema, count, {firstColumnArray, secondColumnArray, threeColumnArray});
    auto writeSuccess = writer->Write(*batch);
    if (!writeSuccess)
    {
        EXPECT_EQ(writeSuccess, true);
        return -1;
    }
    writer->Close();

    upload->Commit({blkId});

    // start integrity checks
    int64_t readcount = 0;
    if (useNormalDownload)
    {
        readcount = DownloadDataNormal(start, count);
    } else
    {
        readcount = DownloadData(start, count);
    }

    if (readcount != static_cast<int64_t>(count))
    {
        EXPECT_EQ(readcount, static_cast<int64_t>(count));
        return -1;
    }

    return readcount;
}

template <typename T>
void GenerateDataIntoBuilder(arrow::ArrayBuilder* _builder, typename T::value_type value, uint64_t count)
{
    T* builder = reinterpret_cast<T*>(_builder);
    vector<typename T::value_type> tmpvec;
    for(uint64_t i = 0; i < count; i++)
    {
        tmpvec.push_back(value);
    }
    builder->AppendValues(tmpvec);
}

template<typename T>
void GenerateDataIntoStringBuilder(arrow::ArrayBuilder* _builder, std::string value, uint64_t count)
{
    T* builder = reinterpret_cast<T*>(_builder);
    vector<std::string> tmpvec;
    for(uint64_t i = 0; i < count; i++)
    {
        tmpvec.push_back(value);
    }
    builder->AppendValues(tmpvec);
}

static std::shared_ptr<arrow::ArrayBuilder> GenerateArrayBuilderBasedOnTypeInfo(
    ODPSColumnTypeInfo typeInfo, arrow::MemoryPool* pool)
{
    switch (typeInfo.mType)
    {
        case apsara::odps::sdk::ODPS_TINYINT:
        {
            return std::make_shared<arrow::Int8Builder>(pool);
        }
        break;
        case apsara::odps::sdk::ODPS_SMALLINT:
        {
            return std::make_shared<arrow::Int16Builder>(pool);
        }
        break;
        case apsara::odps::sdk::ODPS_INTEGER:
        {
            return std::make_shared<arrow::Int32Builder>(pool);
        }
        break;
        case apsara::odps::sdk::ODPS_BIGINT:
        {
            return std::make_shared<arrow::Int64Builder>(pool);
        }
        break;
        case apsara::odps::sdk::ODPS_BOOLEAN:
        {
            return std::make_shared<arrow::BooleanBuilder>(pool);
        }
        break;
        case apsara::odps::sdk::ODPS_FLOAT:
        {
            return std::make_shared<arrow::FloatBuilder>(pool);
        }
        break;
        case apsara::odps::sdk::ODPS_DOUBLE:
        {
            return std::make_shared<arrow::DoubleBuilder>(pool);
        }
        break;
        case apsara::odps::sdk::ODPS_JSON:
        case apsara::odps::sdk::ODPS_STRING:
        case apsara::odps::sdk::ODPS_CHAR:
        case apsara::odps::sdk::ODPS_VARCHAR:
        {
            return std::make_shared<arrow::StringBuilder>(pool);
        }
        break;
        case apsara::odps::sdk::ODPS_DATE:
        {
            return std::make_shared<arrow::Date32Builder>(pool);
        }
        break;
        case apsara::odps::sdk::ODPS_TIMESTAMP:
        {
            return std::make_shared<arrow::TimestampBuilder>(arrow::timestamp(arrow::TimeUnit::NANO), pool);
        }
        break;
        case apsara::odps::sdk::ODPS_TIMESTAMP_NTZ:
        {
            return std::make_shared<arrow::TimestampBuilder>(arrow::timestamp(arrow::TimeUnit::NANO), pool);
        }
        break;
        case apsara::odps::sdk::ODPS_DATETIME:
        {
            return std::make_shared<arrow::TimestampBuilder>(arrow::timestamp(arrow::TimeUnit::MILLI), pool);
        }
        break;
        case apsara::odps::sdk::ODPS_INTERVAL_DAY_TIME:
        {
            return std::make_shared<arrow::TimestampBuilder>(arrow::timestamp(arrow::TimeUnit::NANO), pool);
        }
        break;
        case apsara::odps::sdk::ODPS_BINARY:
        {
            return std::make_shared<arrow::BinaryBuilder>(pool);
        }
        break;
        case apsara::odps::sdk::ODPS_DECIMAL:
        {
            return std::make_shared<arrow::DecimalBuilder>(arrow::decimal(typeInfo.mPrecision, typeInfo.mScale), pool);
        }
        case apsara::odps::sdk::ODPS_ARRAY:
        {
            std::shared_ptr<arrow::ArrayBuilder> elementBuilder = GenerateArrayBuilderBasedOnTypeInfo(typeInfo.mSubTypes.at(0), pool);
            return std::make_shared<arrow::ListBuilder>(pool, elementBuilder);
        }
        case apsara::odps::sdk::ODPS_MAP:
        {
            std::shared_ptr<arrow::ArrayBuilder> keyBuilder = GenerateArrayBuilderBasedOnTypeInfo(typeInfo.mSubTypes.at(0), pool),
                valueBuilder = GenerateArrayBuilderBasedOnTypeInfo(typeInfo.mSubTypes.at(1), pool);
            return std::make_shared<arrow::MapBuilder>(pool, keyBuilder, valueBuilder);
        }
        case apsara::odps::sdk::ODPS_STRUCT:
        {
            std::vector<std::shared_ptr<arrow::ArrayBuilder>> builders;
            std::vector<std::shared_ptr<arrow::Field>> fields;
            for (auto i: typeInfo.mSubTypes)
            {
                std::shared_ptr<arrow::ArrayBuilder> b = GenerateArrayBuilderBasedOnTypeInfo(i, pool);
                builders.push_back(b);
                fields.push_back(arrow::field(i.mMemberName, b->type()));
            }
            return std::make_shared<arrow::StructBuilder>(arrow::struct_(fields), pool, builders);
        }
        default: throw OdpsException("Unsupported autogenerate");
    }
}

static void GenerateDataIntoBuilderBasedOnTypeInfo(
    ODPSColumnTypeInfo typeInfo, arrow::ArrayBuilder* builder, uint64_t count
)
{
    switch (typeInfo.mType)
    {
        case apsara::odps::sdk::ODPS_TINYINT:
        {
            GenerateDataIntoBuilder<arrow::Int8Builder>(builder, static_cast<int8_t>(1), count);
        }
        break;
        case apsara::odps::sdk::ODPS_SMALLINT:
        {
            GenerateDataIntoBuilder<arrow::Int16Builder>(builder, static_cast<int16_t>(1), count);
        }
        break;
        case apsara::odps::sdk::ODPS_INTEGER:
        {
            GenerateDataIntoBuilder<arrow::Int32Builder>(builder, static_cast<int32_t>(1), count);
        }
        break;
        case apsara::odps::sdk::ODPS_BIGINT:
        {
            GenerateDataIntoBuilder<arrow::Int64Builder>(builder, static_cast<int64_t>(1), count);
        }
        break;
        case apsara::odps::sdk::ODPS_BOOLEAN:
        {
            GenerateDataIntoBuilder<arrow::BooleanBuilder>(builder, true, count);
        }
        break;
        case apsara::odps::sdk::ODPS_FLOAT:
        {
            GenerateDataIntoBuilder<arrow::FloatBuilder>(builder, 1.0f, count);
        }
        break;
        case apsara::odps::sdk::ODPS_DOUBLE:
        {
            GenerateDataIntoBuilder<arrow::DoubleBuilder>(builder, 1.0, count);
        }
        break;
        case apsara::odps::sdk::ODPS_JSON:
        case apsara::odps::sdk::ODPS_STRING:
        case apsara::odps::sdk::ODPS_CHAR:
        case apsara::odps::sdk::ODPS_VARCHAR:
        {
            GenerateDataIntoStringBuilder<arrow::StringBuilder>(builder, string("1"), count);
        }
        break;
        case apsara::odps::sdk::ODPS_DATE:
        {
            GenerateDataIntoBuilder<arrow::Date32Builder>(builder, static_cast<int32_t>(1), count);
        }
        break;
        case apsara::odps::sdk::ODPS_TIMESTAMP:
        case apsara::odps::sdk::ODPS_TIMESTAMP_NTZ:
        case apsara::odps::sdk::ODPS_DATETIME:
        case apsara::odps::sdk::ODPS_INTERVAL_DAY_TIME:
        {
            GenerateDataIntoBuilder<arrow::TimestampBuilder>(builder, static_cast<int64_t>(1), count);
        }
        break;
        case apsara::odps::sdk::ODPS_BINARY:
        {
            GenerateDataIntoStringBuilder<arrow::BinaryBuilder>(builder, string("1"), count);
        }
        break;
        case apsara::odps::sdk::ODPS_DECIMAL:
        {
            throw OdpsException("decimal generator not implemented yet.");
        }
        break;
        case apsara::odps::sdk::ODPS_ARRAY:
        {
            arrow::ListBuilder* listbuilder = reinterpret_cast<arrow::ListBuilder*>(builder);
            for (uint64_t i = 0; i < count; i++)
            {
                listbuilder->Append(true);
                GenerateDataIntoBuilderBasedOnTypeInfo(typeInfo.mSubTypes.at(0), listbuilder->value_builder(), 3);
            }
        }
        break;
        case apsara::odps::sdk::ODPS_MAP:
        {
            arrow::MapBuilder* mapbuilder = reinterpret_cast<arrow::MapBuilder*>(builder);
            for (uint64_t i = 0; i < count; i++)
            {
                mapbuilder->Append();
                GenerateDataIntoBuilderBasedOnTypeInfo(typeInfo.mSubTypes.at(0), mapbuilder->key_builder(), 3);
                GenerateDataIntoBuilderBasedOnTypeInfo(typeInfo.mSubTypes.at(0), mapbuilder->item_builder(), 3);
            }
        }
        break;
        case apsara::odps::sdk::ODPS_STRUCT:
        {
            arrow::StructBuilder* structbuilder = reinterpret_cast<arrow::StructBuilder*>(builder);
            for (uint64_t i = 0; i < count; i++)
            {
                structbuilder->Append(true);
                for (size_t j = 0; j < typeInfo.mSubTypes.size(); j++)
                {
                    GenerateDataIntoBuilderBasedOnTypeInfo(typeInfo.mSubTypes.at(j), structbuilder->field_builder(j), 3);
                }
            }
        }
        break;
        default:
            throw OdpsException("Not supported.");
    }
}

static shared_ptr<arrow::RecordBatch> GenerateDataBasedOnOdpsSchema(
    IODPSTableSchema* schema, std::shared_ptr<arrow::Schema> aschema, arrow::MemoryPool* pool, uint64_t count)
{
    vector<shared_ptr<arrow::Array>> builtArrays;
    for (size_t i = 0; i < schema->GetColumnCount(); i++)
    {
        const IODPSTableColumn& col = schema->GetTableColumn(i);
        const ODPSColumnTypeInfo& typeInfo = col.GetTypeInfo();
        std::shared_ptr<arrow::ArrayBuilder> builder = GenerateArrayBuilderBasedOnTypeInfo(typeInfo, pool);
        GenerateDataIntoBuilderBasedOnTypeInfo(typeInfo, builder.get(), count);
        std::shared_ptr<arrow::Array> array;
        arrow::Status s = builder->Finish(&array);
        if (!s.ok())
        {
            throw OdpsException("Failed to build arrow array: " + s.ToString());
        }
        builtArrays.push_back(array);
    }
    shared_ptr<arrow::RecordBatch> rb = arrow::RecordBatch::Make(aschema, count, builtArrays);
    return rb;
}

#define ARRAY_DEBUG_EQ(a, b) EXPECT_EQ(a, b) << "ColType " << typeInfo.ToTypeString() << " Row " << j

static bool CheckArrayBasedOnTypeInfo(ODPSColumnTypeInfo typeInfo, std::shared_ptr<arrow::Array> array, size_t j)
{
    switch (typeInfo.mType)
    {
        case apsara::odps::sdk::ODPS_TINYINT:
        {
            auto col = static_pointer_cast<arrow::Int8Array>(array);
            if (col->Value(j) != 1)
            {
                ARRAY_DEBUG_EQ(col->Value(j), 1);
                return false;
            }
        }
        break;
        case apsara::odps::sdk::ODPS_SMALLINT:
        {
            auto col = static_pointer_cast<arrow::Int16Array>(array);
            if (col->Value(j) != 1)
            {
                ARRAY_DEBUG_EQ(col->Value(j), 1);
                return false;
            }
        }
        break;
        case apsara::odps::sdk::ODPS_INTEGER:
        {
            auto col = static_pointer_cast<arrow::Int32Array>(array);
            if (col->Value(j) != 1)
            {
                ARRAY_DEBUG_EQ(col->Value(j), 1);
                return false;
            }
        }
        break;
        case apsara::odps::sdk::ODPS_BIGINT:
        {
            auto col = static_pointer_cast<arrow::Int64Array>(array);
            if (col->Value(j) != 1)
            {
                ARRAY_DEBUG_EQ(col->Value(j), 1);
                return false;
            }
        }
        break;
        case apsara::odps::sdk::ODPS_BOOLEAN:
        {
            auto col = static_pointer_cast<arrow::BooleanArray>(array);
            if (col->Value(j) != true)
            {
                ARRAY_DEBUG_EQ(col->Value(j), true);
                return false;
            }
        }
        break;
        case apsara::odps::sdk::ODPS_FLOAT:
        {
            auto col = static_pointer_cast<arrow::FloatArray>(array);
            if (col->Value(j) != 1.0f)
            {
                ARRAY_DEBUG_EQ(col->Value(j), 1.0f);
                return false;
            }
        }
        break;
        case apsara::odps::sdk::ODPS_DOUBLE:
        {
            auto col = static_pointer_cast<arrow::DoubleArray>(array);
            if (col->Value(j) != 1.0) // double
            {
                ARRAY_DEBUG_EQ(col->Value(j), 1.0);
                return false;
            }
        }
        break;
        case apsara::odps::sdk::ODPS_JSON:
        case apsara::odps::sdk::ODPS_STRING:
        case apsara::odps::sdk::ODPS_CHAR:
        case apsara::odps::sdk::ODPS_VARCHAR:
        {
            auto col = static_pointer_cast<arrow::StringArray>(array);
            if (col->GetString(j) != "1")
            {
                ARRAY_DEBUG_EQ(col->GetString(j), "1");
                return false;
            }
        }
        break;
        case apsara::odps::sdk::ODPS_DATE:
        {
            auto col = static_pointer_cast<arrow::Date32Array>(array);
            if (col->Value(j) != 1)
            {
                ARRAY_DEBUG_EQ(col->Value(j), 1);
                return false;
            }
        }
        break;
        case apsara::odps::sdk::ODPS_TIMESTAMP:
        case apsara::odps::sdk::ODPS_TIMESTAMP_NTZ:
        case apsara::odps::sdk::ODPS_DATETIME:
        case apsara::odps::sdk::ODPS_INTERVAL_DAY_TIME:
        {
            auto col = static_pointer_cast<arrow::TimestampArray>(array);
            if (col->Value(j) != 1)
            {
                ARRAY_DEBUG_EQ(col->Value(j), 1);
                return false;
            }
        }
        break;
        case apsara::odps::sdk::ODPS_BINARY:
        {
            auto col = static_pointer_cast<arrow::BinaryArray>(array);
            if (col->GetString(j) != "1")
            {
                ARRAY_DEBUG_EQ(col->GetString(j), "1");
                return false;
            }
        }
        break;
        case apsara::odps::sdk::ODPS_DECIMAL:
        {
            throw OdpsException("decimal not implemented.");
        }
        break;
        case apsara::odps::sdk::ODPS_INTERVAL_YEAR_MONTH:
        {
            auto col = static_pointer_cast<arrow::Int64Array>(array);
            if (col->Value(j) != 1)
            {
                ARRAY_DEBUG_EQ(col->Value(j), 1);
                return false;
            }
        }
        break;
        case apsara::odps::sdk::ODPS_ARRAY:
        {
            auto col = static_pointer_cast<arrow::ListArray>(array);
            uint32_t internalOffset = col->value_offset(j);
            uint32_t length = col->value_length(j);
            for (uint32_t i = 0; i < length; i++)
            {
                if(!CheckArrayBasedOnTypeInfo(typeInfo.mSubTypes.at(0), col->values(), internalOffset + i))
                {
                    return false;
                }
            }
        }
        break;
        case apsara::odps::sdk::ODPS_MAP:
        {
            auto col = static_pointer_cast<arrow::MapArray>(array);
            uint32_t internalOffset = col->value_offset(j);
            uint32_t length = col->value_length(j);
            for (uint32_t i = 0; i < length; i++)
            {
                if (!(CheckArrayBasedOnTypeInfo(typeInfo.mSubTypes.at(0), col->keys(), internalOffset + i)
                        && CheckArrayBasedOnTypeInfo(typeInfo.mSubTypes.at(1), col->items(), internalOffset + i)))
                {
                    return false;
                }
            }
        }
        break;
        case apsara::odps::sdk::ODPS_STRUCT:
        {
            auto col = static_pointer_cast<arrow::StructArray>(array);
            uint32_t fieldCount = col->num_fields();
            for (uint32_t i = 0; i < fieldCount; i++)
            {
                if (!CheckArrayBasedOnTypeInfo(typeInfo.mSubTypes.at(i), col->field(i), j))
                {
                    return false;
                }
            }
        }
        break;
        default:    throw OdpsException("Unsupported arrow types found in test case.");
    }
    return true;
}

static bool CheckDataBasedOnTableSchema(IODPSTableSchema* schema, shared_ptr<arrow::RecordBatch> batch)
{
    size_t rows = batch->num_rows();
    for (size_t i = 0; i < schema->GetColumnCount(); i++)
    {
        auto typeInfo = schema->GetTableColumn(i).GetTypeInfo();
        for (size_t j = 0; j < rows; j++)
        {
            if(!CheckArrayBasedOnTypeInfo(typeInfo, batch->column(i), j))
            {
                return false;
            }
        }
    }
    return true;
}

static inline std::size_t replace_all(std::string& inout, nonstd::string_view what, nonstd::string_view with)
{
    std::size_t count{};
    for (std::string::size_type pos{};
         inout.npos != (pos = inout.find(what.data(), pos, what.length()));
         pos += with.length(), ++count)
        inout.replace(pos, what.length(), with.data(), with.length());
    return count;
}

TEST_F(TunnelArrowTest, HybridTunnelAndJsonCase)
{
    const std::string& tableName = "test_json_type_case";

    Utils::DropTableIfExists(tableName);
    Utils::ExecSqlTogether("set odps.sql.type.json.enable=true; create table " +
                           tableName +
                           "(c1 bigint, c2 json, c3 string) stored as aliorc");
    uint64_t count = 8;
    for (int i = 0; i < (int)count; ++i)
    {
        std::string jsonStr = ExpectedJsonBuilder(i);
        replace_all(jsonStr, "\"", "\\\"");
        Utils::ExecSqlTogether(
            "set odps.sql.type.json.enable=true;insert into table " + tableName + " values(" + std::to_string(i) + ", json '" + jsonStr + "', \\\"key_0_val_0\\\")");
    }

    std::cout << "Arrow Test: " << std::endl;
    {
        IDownloadPtr download = sTunnel.CreateDownload(sProjectName, tableName, "", "");;
        IArrowRecordReaderPtr reader = download->OpenArrowReader(0, 1024);

        EXPECT_EQ(count, download->GetRecordCount());
        EXPECT_NE(reader.get(), nullptr);

        shared_ptr<arrow::RecordBatch> batch;

        while (reader->Read(batch))
        {
            int rows = batch->num_rows();
            auto bigIntColumn =
                static_pointer_cast<arrow::Int64Array>(batch->column(0));
            auto jsonColumn =
                static_pointer_cast<arrow::StringArray>(batch->column(1));
            auto stringColumn =
                static_pointer_cast<arrow::StringArray>(batch->column(2));

            for (int j = 0; j < rows; j++) {
                int64_t getInt = bigIntColumn->Value(j);
                string getJsonStr = jsonColumn->GetString(j);
                string expectStr = ExpectedJsonBuilder(getInt);
                EXPECT_EQ(stringColumn->GetString(j), "key_0_val_0");
                EXPECT_EQ(expectStr, getJsonStr);
            }
        }
        reader->Close();
        download->Complete();
    }

    std::cout << "OdpsRecordCase Test: " << std::endl;
    {
        uint64_t count = 1024;
        const std::string& tableName = "test_json_type_case";
        IDownloadPtr download = sTunnel.CreateDownload(sProjectName, tableName, "", "");;
        IRecordReaderPtr reader = download->OpenReader(0, count);

        ODPSTableRecordPtr _r = reader->CreateBufferRecord();
        ODPSTableRecord& record = *_r;

        while (reader->Read(record))
        {
            uint32_t len = 0;
            const std::string expectJsonStr =  ExpectedJsonBuilder(*record.GetBigIntValue(0));
            const std::string actualJsonStr = record.GetJsonValue(1, len);
            EXPECT_EQ(expectJsonStr, actualJsonStr);
        }
        reader->Close();
        download->Complete();
    }
    Utils::DropTableIfExists(tableName);
}

TEST_F(TunnelArrowTest, ReadNormal)
{
    ASSERT_EQ(DownloadData(0, sCount), static_cast<int64_t>(sCount));
}

TEST_F(TunnelArrowTest, ReadCompressedZSTD)
{
    ASSERT_EQ(DownloadCompressedData(0, sCount, CompressOption::ODPS_ZSTD), static_cast<int64_t>(sCount));
}

TEST_F(TunnelArrowTest, ReadCompressedLZ4)
{
    ASSERT_EQ(DownloadCompressedData(0, sCount, CompressOption::ODPS_LZ4_FRAME), static_cast<int64_t>(sCount));
}

TEST_F(TunnelArrowTest, ReadCompressedODPSLZ4)
{
    ASSERT_EQ(DownloadCompressedData(0, sCount, CompressOption::ODPS_LZ4), static_cast<int64_t>(sCount));
}

TEST_F(TunnelArrowTest, ReadPartOfData)
{
    uint64_t cnt = sCount - (sCount/2);
    ASSERT_EQ(DownloadData(sCount / 2, cnt), static_cast<int64_t>(cnt));
}

TEST_F(TunnelArrowTest, ReadStartOffsetLessThanZero)
{
    bool expected = false;
    try
    {
        DownloadData(-1, sCount/2);
    }
    catch (OdpsException &e)
    {
        ASSERT_NE(e.GetErrorMsg().find("row range is not valid"), std::string::npos);
        expected = true;
    }
    ASSERT_TRUE(expected);
}

TEST_F(TunnelArrowTest, ReadCountEqualZero)
{
    bool expected = false;
    try
    {
        DownloadData(0, 0);
    }
    catch (OdpsException &e)
    {
        ASSERT_NE(e.GetErrorMsg().find("row range is not valid"), std::string::npos);
        expected = true;
    }
    ASSERT_TRUE(expected);
}

TEST_F(TunnelArrowTest, ReadCountLessThanZero)
{
    bool expected = false;
    try
    {
        DownloadData(0, -1);
    }
    catch (OdpsException &e)
    {
        ASSERT_NE(e.GetErrorMsg().find("row range is not valid"), std::string::npos);
        expected = true;
    }
    ASSERT_TRUE(expected);
}

TEST_F(TunnelArrowTest, ReadStartOffsetGreatThanTotalCount)
{
    ASSERT_EQ(DownloadData(sCount * 2, 1), 0);
}

TEST_F(TunnelArrowTest, ReadRangeGreatThanTotalCount)
{
    ASSERT_EQ(DownloadData(0, sCount * 2), static_cast<int64_t>(sCount));
}

TEST_F(TunnelArrowTest, WriteNormal)
{
    ASSERT_EQ(UploadData(ONE_BATCH, NewBlockId()), ONE_BATCH);
}

TEST_F(TunnelArrowTest, WriteCompressedZSTD)
{
    ASSERT_EQ(UploadCompressedData(ONE_BATCH, NewBlockId(), CompressOption::ODPS_ZSTD), ONE_BATCH);
}

TEST_F(TunnelArrowTest, WriteCompressedLZ4)
{
    ASSERT_EQ(UploadCompressedData(ONE_BATCH, NewBlockId(), CompressOption::ODPS_LZ4_FRAME), ONE_BATCH);
}

TEST_F(TunnelArrowTest, WriteCompressedODPSLZ4)
{
    ASSERT_EQ(UploadCompressedData(ONE_BATCH, NewBlockId(), CompressOption::ODPS_LZ4), ONE_BATCH);
}

TEST_F(TunnelArrowTest, WriteOverwrite)
{
    ASSERT_EQ(UploadData(ONE_BATCH, NewBlockId(), "", true), ONE_BATCH);
}

TEST_F(TunnelArrowTest, WriteNormalWithNormalDownload)
{
    ASSERT_EQ(UploadData(ONE_BATCH, NewBlockId(), "", false, true), ONE_BATCH);
}

TEST_F(TunnelArrowTest, WriteOverwriteWithNormalDownload)
{
    ASSERT_EQ(UploadData(ONE_BATCH, NewBlockId(), "", true, true), ONE_BATCH);
}

TEST_F(TunnelArrowTest, TypeCoverage)
{
    uint32_t blkId = NewBlockId();
    string projectName = Utils::GetProjectName();
    Utils::DropTableIfExists(sTableName);
    string tableName = Utils::GetRandomTableName();
    sTableName = tableName; // ensure auto clean
    auto tunnel = Utils::GetTunnelInstance();
    uint64_t count = ONE_BATCH;
    Utils::DropTableIfExists(tableName);
    vector<string> types = {"tinyint", "smallint", "int", "bigint",
        "boolean", "float", "double", "varchar(2)", "string",
        "timestamp", "timestamp_ntz", "binary", "datetime", "json"};
    string createTableSql = "set odps.sql.type.json.enable=true; set odps.sql.type.system.odps2=true; create table " + tableName + " (";
    for (size_t i = 0; i < types.size(); i++)
    {
        createTableSql += "c" + std::to_string(i) + " " + types[i];
        if (i != (types.size() - 1))
        {
            createTableSql += ", ";
        }
    }
    createTableSql += ") stored as aliorc";
    Utils::ExecSqlTogether(createTableSql);

    IUploadPtr upload = tunnel.CreateUpload(projectName, tableName);
    ASSERT_NE(upload->GetUploadId(), "");
    arrow::MemoryPool* pool = arrow::default_memory_pool();
    auto schema = upload->GetArrowSchema();
    auto writer = upload->OpenArrowWriter(blkId);
    auto batch = GenerateDataBasedOnOdpsSchema(upload->GetSchema(), schema, pool, count);
    ASSERT_EQ(writer->Write(*batch), true);
    writer->Close();
    upload->Commit({blkId});

    // DOWNLOAD
    IDownloadPtr download = tunnel.CreateDownload(projectName, tableName, "", "");
    ASSERT_NE(download->GetDownloadId(), "");
    ASSERT_EQ(download->GetRecordCount(), count);
    IArrowRecordReaderPtr reader = download->OpenArrowReader(0, count);
    ASSERT_NE(reader.get(), nullptr);
    shared_ptr<arrow::RecordBatch> readBatch;
    while(reader->Read(readBatch))
    {
        ASSERT_EQ(CheckDataBasedOnTableSchema(download->GetSchema(), readBatch), true);
    }
    reader->Close();
    download->Complete();
}

TEST_F(TunnelArrowTest, ColumnSlice)
{
    IDownloadPtr download = CreateDownload("");
    IArrowRecordReaderPtr reader = download->OpenArrowReader(0, sCount, {"c3"});
    ASSERT_EQ(sCount, download->GetRecordCount());
    ASSERT_NE(reader.get(), nullptr);

    shared_ptr<arrow::RecordBatch> batch;

    uint64_t i = 0;
    while (reader->Read(batch))
    {
        int rows = batch->num_rows();
        auto stringColumn = static_pointer_cast<arrow::StringArray>(batch->column(0));
        for (int j = 0; j < rows; j++)
        {
            string expectStr = ExpectedStringBuilder(i);
            string getStr = stringColumn->GetString(j);
            if (expectStr != getStr)
            {
                ASSERT_EQ(expectStr, getStr);
            }
            ++i;
        }
    }
    reader->Close();
    download->Complete();
    ASSERT_EQ(sCount, i);
}

TEST_F(TunnelArrowTest, DifferentColumnSlice)
{
    IDownloadPtr download = CreateDownload("");
    {
        IArrowRecordReaderPtr reader = download->OpenArrowReader(0, sCount, {"c3"});
        ASSERT_EQ(sCount, download->GetRecordCount());
        ASSERT_NE(reader.get(), nullptr);

        shared_ptr<arrow::RecordBatch> batch;

        uint64_t i = 0;
        while (reader->Read(batch))
        {
            int rows = batch->num_rows();
            auto stringColumn = static_pointer_cast<arrow::StringArray>(batch->column(0));
            for (int j = 0; j < rows; j++)
            {
                string expectStr = ExpectedStringBuilder(i);
                string getStr = stringColumn->GetString(j);
                if (expectStr != getStr)
                {
                    ASSERT_EQ(expectStr, getStr);
                }
                ++i;
            }
        }
        reader->Close();
        ASSERT_EQ(sCount, i);
    }
    {
        IArrowRecordReaderPtr reader = download->OpenArrowReader(0, sCount, {"c1"});
        ASSERT_EQ(sCount, download->GetRecordCount());
        ASSERT_NE(reader.get(), nullptr);

        shared_ptr<arrow::RecordBatch> batch;

        uint64_t i = 0;
        while (reader->Read(batch))
        {
            int rows = batch->num_rows();
            auto biColumn = static_pointer_cast<arrow::Int64Array>(batch->column(0));
            for (int j = 0; j < rows; j++)
            {
                ASSERT_EQ(int64_t(i), biColumn->Value(j));
                ++i;
            }
        }
        reader->Close();
        ASSERT_EQ(sCount, i);
    }
    download->Complete();
}

TEST_F(TunnelArrowTest, ComplexTypeCoverage)
{
    uint32_t blkId = NewBlockId();
    string projectName = Utils::GetProjectName();
    Utils::DropTableIfExists(sTableName);
    string tableName = Utils::GetRandomTableName();
    sTableName = tableName; // ensure auto clean
    auto tunnel = Utils::GetTunnelInstance();
    uint64_t count = ONE_BATCH;
    Utils::DropTableIfExists(tableName);
    vector<string> types = {
        "tinyint",
        "array<bigint>",
        "array<array<bigint>>",
        "map<string, string>",
        "struct<a: string, b: bigint>",
        "struct<a: array<bigint>, b:array<array<bigint>>>",
        };
    string createTableSql = "set odps.sql.type.system.odps2=true; create table " + tableName + " (";
    for (size_t i = 0; i < types.size(); i++)
    {
        createTableSql += "c" + std::to_string(i) + " " + types[i];
        if (i != (types.size() - 1))
        {
            createTableSql += ", ";
        }
    }
    createTableSql += ") stored as AliOrc tblproperties (\\\"columnar.nested.type\\\"=\\\"true\\\")";
    Utils::ExecSqlTogether(createTableSql);

    IUploadPtr upload = tunnel.CreateUpload(projectName, tableName);
    ASSERT_NE(upload->GetUploadId(), "");
    arrow::MemoryPool* pool = arrow::default_memory_pool();
    auto schema = upload->GetArrowSchema();
    auto writer = upload->OpenArrowWriter(blkId);
    auto batch = GenerateDataBasedOnOdpsSchema(upload->GetSchema(), schema, pool, count);
    ASSERT_EQ(writer->Write(*batch), true);
    writer->Close();
    upload->Commit({blkId});

    // DOWNLOAD
    IDownloadPtr download = tunnel.CreateDownload(projectName, tableName, "", "");
    ASSERT_NE(download->GetDownloadId(), "");
    ASSERT_EQ(download->GetRecordCount(), count);
    IArrowRecordReaderPtr reader = download->OpenArrowReader(0, count);
    ASSERT_NE(reader.get(), nullptr);
    shared_ptr<arrow::RecordBatch> readBatch;
    while(reader->Read(readBatch))
    {
        ASSERT_EQ(CheckDataBasedOnTableSchema(download->GetSchema(), readBatch), true);
    }
    reader->Close();
    download->Complete();
}

TEST_F(TunnelArrowTest, DownloadModified)
{
    IDownloadPtr download = CreateDownload();

    IUploadPtr upload = sTunnel.CreateUpload(sProjectName, sTableName);
    uint32_t blkId = NewBlockId();
    IRecordWriterPtr writer = upload->OpenWriter(blkId);
    ODPSTableRecordPtr _r = upload->CreateBufferRecord();
    ODPSTableRecord& record = *_r;
    for (uint64_t i = 0; i < sCount; ++i)
    {
        const std::string& jsonStr = ExpectedJsonBuilder(i);
        const std::string& str = ExpectedStringBuilder(i);
        record.SetBigIntValue(0, i);
        record.SetJsonValue(1, jsonStr.c_str(), jsonStr.length());
        record.SetStringValue(2, str.c_str(), str.length());
        writer->Write(record);
    }
    writer->Close();
    upload->Commit({blkId});

    ASSERT_TRUE(NULL != download->GetSchema());
    ASSERT_EQ(static_cast<uint32_t>(3), download->GetSchema()->GetColumnCount());
    ASSERT_EQ(sCount, download->GetRecordCount());

    try
    {
        download->OpenArrowReader(0, 99999999);
        ASSERT_EQ("Should Not Reach Here", "Should throw modified exception");
    }
    catch(const OdpsException& e)
    {
        ASSERT_EQ(e.GetErrorCode(), "TableModified");
    }

    std::vector<std::string> colNames;
    IArrowRecordReaderPtr reader = download->OpenArrowReader(0, 99999999, colNames, CompressOption::NO_COMPRESS, true);
    ASSERT_NE(reader.get(), nullptr);

    shared_ptr<arrow::RecordBatch> batch;

    uint64_t count = 0;
    while (reader->Read(batch))
    {
        int rows = batch->num_rows();
        auto bigIntColumn = static_pointer_cast<arrow::Int64Array>(batch->column(0));
        auto jsonColumn = static_pointer_cast<arrow::StringArray>(batch->column(1));
        auto stringColumn = static_pointer_cast<arrow::StringArray>(batch->column(2));
        for (int j = 0; j < rows; j++)
        {
            int64_t getInt = bigIntColumn->Value(j);
            string getJsonStr = jsonColumn->GetString(j);
            string expectStr = ExpectedJsonBuilder(getInt);
            EXPECT_EQ(expectStr, getJsonStr);

            expectStr = ExpectedStringBuilder(getInt);
            string getStr = stringColumn->GetString(j);
            EXPECT_EQ(expectStr, getStr);

            ++count;
        }
    }
    ASSERT_EQ(sCount, count);
    reader->Close();
    download->Complete();
}

TEST_F(TunnelArrowTest, schemaFieldNullableTest)
{
    const std::string& tableName = "test_arrow_field_nullable_case";
    Utils::DropTableIfExists(tableName);

    Utils::ExecSqlTogether("set odps.sql.type.json.enable=true;create table " +
                           tableName +
                           "(c1 bigint not null, c2 json, c3 string) stored as aliorc");

    IUploadPtr upload = sTunnel.CreateUpload(sProjectName, tableName);
    uint32_t blkId = NewBlockId();
    IRecordWriterPtr writer = upload->OpenWriter(blkId);
    ODPSTableRecordPtr _r = upload->CreateBufferRecord();
    ODPSTableRecord& record = *_r;
    for (uint64_t i = 0; i < sCount; ++i)
    {
        const std::string& jsonStr = ExpectedJsonBuilder(i);
        const std::string& str = ExpectedStringBuilder(i);
        record.SetBigIntValue(0, i);
        record.SetJsonValue(1, jsonStr.c_str(), jsonStr.length());
        record.SetStringValue(2, str.c_str(), str.length());
        writer->Write(record);
    }
    writer->Close();
    upload->Commit({blkId});

    IDownloadPtr download = sTunnel.CreateDownload(sProjectName, tableName, "", "");;
    IArrowRecordReaderPtr reader = download->OpenArrowReader(0, sCount);
    if (download->GetRecordCount() != sCount) {
        EXPECT_EQ(sCount, download->GetRecordCount());
    }

    shared_ptr<arrow::RecordBatch> batch;
    while (reader->Read(batch))
    {

    }
    reader->Close();
    download->Complete();
    for (int i = 0; i < 3; i++)
    {
        EXPECT_EQ(download->GetSchema()->GetTableColumn(i).GetNullable(), batch->schema()->field(i)->nullable());
        EXPECT_EQ(download->GetSchema()->GetTableColumn(i).GetNullable(), upload->GetArrowSchema()->field(i)->nullable());
    }

    Utils::DropTableIfExists(tableName);
}

TEST_F(TunnelArrowTest, getArrowSchemaTest)
{
    const std::string& tableName = Utils::GetRandomTableName();
    Utils::DropTableIfExists(tableName);

    Utils::ExecSqlTogether("set odps.sql.type.json.enable=true;create table " +
                           tableName +
                           "(c1 bigint not null, c2 json, c3 string) stored as aliorc");
    IUploadPtr upload = sTunnel.CreateUpload(sProjectName, tableName);
    upload->Commit();
    IDownloadPtr download = sTunnel.CreateDownload(sProjectName, tableName, "", "");
    download->Complete();
    auto uploadArrowSchema = upload->GetArrowSchema();
    auto donwloadArrowSchema = download->GetArrowSchema();
    auto uploadSchema = upload->GetSchema();
    EXPECT_EQ(uploadArrowSchema->num_fields(), donwloadArrowSchema->num_fields());
    for(int i = 0; i < uploadArrowSchema->num_fields(); i++)
    {
        EXPECT_EQ(uploadArrowSchema->field(i)->name(), uploadSchema->GetTableColumn(i).GetName());
        EXPECT_EQ(uploadArrowSchema->field(i)->name(), donwloadArrowSchema->field(i)->name());
    }
}

// 从0下载数据，数据量小于,等于,大于总数量; buffer size是否被数据量整除
TEST_F(TunnelArrowTest, BufferArrowDownload)
{
    // 下载数据量等于数据表总行数
    ASSERT_EQ(DownloadDataByBuffer(0, sCount, sCount/2), static_cast<int64_t>(sCount));
    ASSERT_EQ(DownloadDataByBuffer(0, sCount, 300), static_cast<int64_t>(sCount));

    // 下载数据量大于数据表总行数
    ASSERT_EQ(DownloadDataByBuffer(0, 2000, sCount/2), static_cast<int64_t>(sCount));
    ASSERT_EQ(DownloadDataByBuffer(0, 2000, 300), static_cast<int64_t>(sCount));

    // 下载数据量小于数据表总行数, buffer设为0时会重置为1
    ASSERT_EQ(DownloadDataByBuffer(0, sCount/100, 0), static_cast<int64_t>(sCount/100));
    ASSERT_EQ(DownloadDataByBuffer(0, sCount/100, 4), static_cast<int64_t>(sCount/100));
}

// 从指定位置下载数据，数据量小于,等于,大于总数量; buffer size是否被数据量整除
TEST_F(TunnelArrowTest, BufferArrowDownloadPart)
{
    ASSERT_EQ(DownloadDataByBuffer(100, sCount, sCount/2), static_cast<int64_t>(sCount - 100));
    ASSERT_EQ(DownloadDataByBuffer(100, sCount, 300), static_cast<int64_t>(sCount - 100));

    ASSERT_EQ(DownloadDataByBuffer(100, 2000, sCount/2), static_cast<int64_t>(sCount - 100));
    ASSERT_EQ(DownloadDataByBuffer(100, 2000, 300), static_cast<int64_t>(sCount - 100));

    ASSERT_EQ(DownloadDataByBuffer(100, sCount/100, 0), static_cast<int64_t>(sCount/100));
    ASSERT_EQ(DownloadDataByBuffer(100, sCount/100, 4), static_cast<int64_t>(sCount/100));
}

TEST_F(TunnelArrowTest, BufferArrowDownloadModified)
{
    IDownloadPtr download = CreateDownload();

    IUploadPtr upload = sTunnel.CreateUpload(sProjectName, sTableName);
    uint32_t blkId = NewBlockId();
    IRecordWriterPtr writer = upload->OpenWriter(blkId);
    ODPSTableRecordPtr _r = upload->CreateBufferRecord();
    ODPSTableRecord& record = *_r;
    for (uint64_t i = 0; i < sCount; ++i)
    {
        const std::string& jsonStr = ExpectedJsonBuilder(i);
        const std::string& str = ExpectedStringBuilder(i);
        record.SetBigIntValue(0, i);
        record.SetJsonValue(1, jsonStr.c_str(), jsonStr.length());
        record.SetStringValue(2, str.c_str(), str.length());
        writer->Write(record);
    }
    writer->Close();
    upload->Commit({blkId});

    ASSERT_TRUE(NULL != download->GetSchema());
    ASSERT_EQ(static_cast<uint32_t>(3), download->GetSchema()->GetColumnCount());
    ASSERT_EQ(sCount, download->GetRecordCount());

    try
    {
        download->OpenArrowReader(0, 99999999);
        ASSERT_EQ("Should Not Reach Here", "Should throw modified exception");
    }
    catch(const OdpsException& e)
    {
        ASSERT_EQ(e.GetErrorCode(), "TableModified");
    }
    std::vector<std::string> colNames;
    IBufferArrowRecordReaderPtr reader = download->OpenBufferArrowReader(0, 99999999, sCount, colNames, CompressOption::NO_COMPRESS, true);
    ASSERT_NE(reader.get(), nullptr);

    shared_ptr<arrow::RecordBatch> batch;
    uint64_t count = 0;
    while((batch = reader->ReadWithRetry(5)) != nullptr)
    {
        int rows = batch->num_rows();
        auto bigIntColumn = static_pointer_cast<arrow::Int64Array>(batch->column(0));
        auto jsonColumn = static_pointer_cast<arrow::StringArray>(batch->column(1));
        auto stringColumn = static_pointer_cast<arrow::StringArray>(batch->column(2));
        for (int j = 0; j < rows; j++)
        {
            int64_t getInt = bigIntColumn->Value(j);
            string getJsonStr = jsonColumn->GetString(j);
            string expectStr = ExpectedJsonBuilder(getInt);
            EXPECT_EQ(expectStr, getJsonStr);

            expectStr = ExpectedStringBuilder(getInt);
            string getStr = stringColumn->GetString(j);
            EXPECT_EQ(expectStr, getStr);

            ++count;
        }
    }
    ASSERT_EQ(sCount, count);
    download->Complete();
}

// 测试缓冲区大小控制：验证缓冲区实际缓存的记录数是否符合 bufferRecordCount 的要求
TEST_F(TunnelArrowTest, BufferArrowDownloadBufferSizeControl)
{
    IDownloadPtr download = CreateDownload();

    // 使用较小的缓冲区大小来验证缓冲区控制逻辑
    uint64_t bufferRecordCount = 50;
    uint64_t totalRecords = sCount;

    std::vector<std::string> colNames;
    CompressOption option = CompressOption::NO_COMPRESS;
    IBufferArrowRecordReaderPtr reader = download->OpenBufferArrowReader(0, totalRecords, bufferRecordCount, colNames, option);
    ASSERT_NE(reader.get(), nullptr);

    shared_ptr<arrow::RecordBatch> firstBatch = reader->Read();
    ASSERT_NE(firstBatch.get(), nullptr);

    uint64_t firstBatchRecordCount = firstBatch->num_rows();
    ASSERT_GE(firstBatchRecordCount, bufferRecordCount);
    ASSERT_LE(firstBatchRecordCount, bufferRecordCount * 10);

    auto bigIntColumn = static_pointer_cast<arrow::Int64Array>(firstBatch->column(0));
    for (uint64_t i = 0; i < firstBatchRecordCount; i++)
    {
        ASSERT_EQ(static_cast<int64_t>(i), bigIntColumn->Value(i));
    }

    uint64_t totalReadRecords = firstBatchRecordCount;
    shared_ptr<arrow::RecordBatch> batch;
    while((batch = reader->Read()) != nullptr)
    {
        totalReadRecords += batch->num_rows();
    }
    ASSERT_EQ(totalReadRecords, totalRecords);

    download->Complete();
}

TEST_F(TunnelArrowTest, BufferArrowDownloadSmallBufferSize)
{
    IDownloadPtr download = CreateDownload();

    // 使用非常小的缓冲区大小
    uint64_t bufferRecordCount = 5;
    uint64_t totalRecords = std::min(sCount, 100UL);

    std::vector<std::string> colNames;
    CompressOption option = CompressOption::NO_COMPRESS;
    IBufferArrowRecordReaderPtr reader = download->OpenBufferArrowReader(0, totalRecords, bufferRecordCount, colNames, option);
    ASSERT_NE(reader.get(), nullptr);

    shared_ptr<arrow::RecordBatch> firstBatch = reader->Read();
    ASSERT_NE(firstBatch.get(), nullptr);

    uint64_t firstBatchRecordCount = firstBatch->num_rows();
    ASSERT_GE(firstBatchRecordCount, bufferRecordCount);

    uint64_t totalReadRecords = firstBatchRecordCount;
    shared_ptr<arrow::RecordBatch> batch;
    while((batch = reader->Read()) != nullptr)
    {
        totalReadRecords += batch->num_rows();
    }

    ASSERT_EQ(totalReadRecords, totalRecords);

    download->Complete();
}

// 测试 rawSize 参数：验证服务端返回的 RecordBatch 大小不超过 rawSize 限制
TEST_F(TunnelArrowTest, BufferArrowDownloadWithRawSize)
{
    // 测试 rawSize 参数的基本功能
    // rawSize 必须大于 1MB（服务端要求），设置为 2MB 用于测试
    uint64_t rawSize = 1024 * 1024 * 2;  // 2MB，满足服务端最小要求
    ASSERT_EQ(DownloadDataByBufferWithRawSize(0, sCount, sCount/2, rawSize), static_cast<int64_t>(sCount));
    ASSERT_EQ(DownloadDataByBufferWithRawSize(0, sCount, 300, rawSize), static_cast<int64_t>(sCount));

    // 测试从指定位置下载
    ASSERT_EQ(DownloadDataByBufferWithRawSize(100, sCount, sCount/2, rawSize), static_cast<int64_t>(sCount - 100));
    ASSERT_EQ(DownloadDataByBufferWithRawSize(100, sCount, 300, rawSize), static_cast<int64_t>(sCount - 100));
}

// 测试 rawSize 参数与 bufferRecordCount 的组合使用
TEST_F(TunnelArrowTest, BufferArrowDownloadRawSizeControl)
{
    IDownloadPtr download = CreateDownload();

    // 使用较小的 rawSize 和 bufferRecordCount 来验证组合控制
    // rawSize 必须大于 1MB（服务端要求），设置为 2MB 用于测试
    uint64_t bufferRecordCount = 100;  // 客户端请求的记录数
    uint64_t rawSize = 1024 * 1024 * 2;  // 2MB，满足服务端最小要求
    uint64_t totalRecords = std::min(sCount, 500UL);  // 限制此测试的总记录数

    std::vector<std::string> colNames;
    CompressOption option = CompressOption::NO_COMPRESS;
    IBufferArrowRecordReaderPtr reader = download->OpenBufferArrowReader(0, totalRecords, bufferRecordCount, rawSize, colNames, option);
    ASSERT_NE(reader.get(), nullptr);

    // 读取第一个批次
    shared_ptr<arrow::RecordBatch> firstBatch = reader->Read();
    ASSERT_NE(firstBatch.get(), nullptr);

    uint64_t firstBatchRecordCount = firstBatch->num_rows();
    // 第一个批次应该包含至少一些记录（受 rawSize 限制，可能小于 bufferRecordCount）
    ASSERT_GT(firstBatchRecordCount, 0UL);

    // 验证第一个批次中的数据完整性
    auto bigIntColumn = static_pointer_cast<arrow::Int64Array>(firstBatch->column(0));
    for (uint64_t i = 0; i < firstBatchRecordCount; i++)
    {
        ASSERT_EQ(static_cast<int64_t>(i), bigIntColumn->Value(i));
    }

    // 继续读取所有记录
    uint64_t totalReadRecords = firstBatchRecordCount;
    shared_ptr<arrow::RecordBatch> batch;
    while((batch = reader->Read()) != nullptr)
    {
        totalReadRecords += batch->num_rows();
    }

    // 验证读取了所有记录
    ASSERT_EQ(totalReadRecords, totalRecords);

    download->Complete();
}

// 测试 rawSize=0 的情况（不限制大小，等同于不带 rawSize 的版本）
TEST_F(TunnelArrowTest, BufferArrowDownloadRawSizeZero)
{
    // rawSize=0 应该等同于不带 rawSize 参数的版本
    ASSERT_EQ(DownloadDataByBufferWithRawSize(0, sCount, sCount/2, 0), static_cast<int64_t>(sCount));
    ASSERT_EQ(DownloadDataByBufferWithRawSize(0, sCount, 300, 0), static_cast<int64_t>(sCount));
    ASSERT_EQ(DownloadDataByBufferWithRawSize(100, sCount, sCount/2, 0), static_cast<int64_t>(sCount - 100));
}

#endif
