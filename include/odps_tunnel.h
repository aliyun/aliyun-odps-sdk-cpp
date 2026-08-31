#ifndef APSARA_ODPS_TUNNEL_H
#define APSARA_ODPS_TUNNEL_H

#include <stdint.h>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <list>
#include <memory>
#include <limits>

#include "configuration.h"
#include "odps_exception.h"
#include "odps_table.h"
#ifdef ODPS_SDK_ENABLE_ARROW
#include <arrow/api.h>
#endif

namespace apsara{ namespace odps{ namespace sdk{

/**
 *	@brief 用于向odps上传数据
 */
class IRecordWriter
{
public:
    virtual ~IRecordWriter() {}

    /**
     *	@brief 写入一个Record对象
     *
     *	@param ODPSTableRecord 一个Record对象的引用
     *	@return 返回是否写入成功
     */
    virtual bool Write(const ODPSTableRecord& r) = 0;

    /**
     *	@brief 用来释放相关资源，在使用完后必须调用。
     */
    virtual void Close() = 0;

    virtual std::string GetTraceId() = 0;

    virtual int64_t GetWrittenSize() = 0;

    virtual int64_t GetResponseSize() = 0;

    /**
     *	@brief 获取写入数据的 metrics
     */
    virtual std::string GetMetrics() = 0;
};

typedef std::shared_ptr<IRecordWriter> IRecordWriterPtr;

/**
 *	@brief flush选项，预留参数
 */
class FlushOption
{
};

/**
 *	@brief flush结果
 */
class FlushResult
{
public:
    std::string mTraceId;
    int64_t mFlushSize;
    int64_t mRecordCount;
};

/**
 *	@brief 用于向odps上传数据
 *	数据先写入缓存
 *	调用Flush会执行IO操作把数据发送到server端
 *	调用Reset后RecordPack可以复用
 */
class IRecordPack
{
public:
    virtual ~IRecordPack() {}

    /**
     *	@brief 写入一个Record对象到缓存
     *
     *	@param ODPSTableRecord 一个Record对象的引用
     *	@return 返回是否写入成功
     */
    virtual bool Append(const ODPSTableRecord& r) = 0;

    /**
     * 返回当前buffer大小
     */
    virtual int64_t GetDataSize() const = 0;

    /**
     * 返回已经写入buffer的记录数
     */
    virtual int64_t GetRecordCount() const = 0;

    /**
     *	@brief 将当前缓存中的数据flush到server端
     */
    virtual std::string Flush() = 0;

    /**
     *	@brief 将当前缓存中的数据flush到server端
     *
     *	@param option flush参数
     *	@return flush结果
     */
    virtual FlushResult Flush(const FlushOption& option) = 0;

    /**
     *	@brief 获取写入数据的 metrics
     */
    virtual std::string GetMetrics() = 0;

    /**
     * 清理pack对象内存
     */
    virtual void Reset() = 0;
};

typedef std::shared_ptr<IRecordPack> IRecordPackPtr;

/**
 *	@brief 用于下载Odps数据
 */
class IRecordReader
{
public:
    virtual ~IRecordReader() {}

    /**
     *	@brief 读取一个Record对象
     *
     *	@param ODPSTableRecord 一个Record对象的引用
     *	@return 返回是否读取成功
     */
    virtual bool Read(ODPSTableRecord& r) = 0;

    /**
     *	@brief 用来释放相关资源，在使用完后必须调用。
     */
    virtual void Close() = 0;

    /**
     *	@brief 获得当前 Reader 的正确 Schema
     */
    virtual IODPSTableSchema* GetSchema() = 0;

    /**
     *	@brief 创建用于上传的 buffer record
     */
    virtual ODPSTableRecordPtr CreateBufferRecord() = 0;

    /**
     *	@brief 获取读取数据的 metrics
     */
    virtual std::string GetMetrics() = 0;

};

typedef std::shared_ptr<IRecordReader> IRecordReaderPtr;

class IBufferRecordReader
{
public:
    virtual ~IBufferRecordReader() {}
    virtual ODPSTableRecordPtr Read() = 0;
    virtual ODPSTableRecordPtr ReadWithRetry(uint64_t retryTimes) = 0;
    virtual void Close() = 0;
};

typedef std::shared_ptr<IBufferRecordReader> IBufferRecordReaderPtr;

#ifdef ODPS_SDK_ENABLE_ARROW

/**
 *	@brief Arrow 格式从 ODPS 下载数据。
 */
class IArrowRecordReader
{
public:
    virtual ~IArrowRecordReader(){}
    virtual bool Read(std::shared_ptr<arrow::RecordBatch>& r) = 0;
    /**
     *	@brief 用来释放相关资源，在使用完后必须调用。
     */
    virtual void Close() = 0;
};

typedef std::shared_ptr<IArrowRecordReader> IArrowRecordReaderPtr;

/**
 *	@brief Arrow 格式缓冲读取器接口，用于批量读取 Arrow RecordBatch。
 */
class IBufferArrowRecordReader
{
public:
    virtual ~IBufferArrowRecordReader() {}
    virtual std::shared_ptr<arrow::RecordBatch> Read() = 0;
    virtual std::shared_ptr<arrow::RecordBatch> ReadWithRetry(uint64_t retryTimes) = 0;
};

typedef std::shared_ptr<IBufferArrowRecordReader> IBufferArrowRecordReaderPtr;

/**
 *	@brief Arrow 格式向 ODPS 上传数据。
 */
class IArrowRecordWriter
{
public:
    virtual ~IArrowRecordWriter(){}
    virtual bool Write(const arrow::RecordBatch& r) = 0;
    /**
     *	@brief 用来释放相关资源，在使用完后必须调用。
     */
    virtual void Close() = 0;
};

typedef std::shared_ptr<IArrowRecordWriter> IArrowRecordWriterPtr;

struct EOSInfo
{
    int64_t mSequenceId = 0;
    int64_t mSequenceOffset = 0;

    EOSInfo() {}
    EOSInfo(int64_t seqId, int64_t seqOff): mSequenceId(seqId), mSequenceOffset(seqOff) {}

    bool operator==(const EOSInfo& another) const
    {
        return mSequenceId == another.mSequenceId && mSequenceOffset == another.mSequenceOffset;
    }

    operator std::string()
    {
        return std::to_string(mSequenceId) + ":" + std::to_string(mSequenceOffset);
    }
};

class IEOSStream
{
public:
    virtual ~IEOSStream(){}

    virtual EOSInfo Tell() = 0;

    virtual FlushResult Write(const arrow::RecordBatch& r, const EOSInfo& offset) = 0;

    virtual void Release() = 0;
};

typedef std::shared_ptr<IEOSStream> IEOSStreamPtr;
#endif


/**
 *	@brief 文件的输出流
 */
class IVolumeOutputStream
{
public:
    virtual ~IVolumeOutputStream() {}

    /**
     *	@brief 写入字节流
     *
     *	@param buf 写入的数据块的起始地址
     *	@param size 需要写入的字节数量
     */
    virtual void Write(const char* buf, int size) = 0;

    /**
     *	@brief 用来释放相关资源，在使用完后必须调用。
     */
    virtual void Close() = 0;
};

typedef std::shared_ptr<IVolumeOutputStream> IVolumeOutputStreamPtr;

/**
 *	@brief 文件的输入流
 */
class IVolumeInputStream
{
public:
    virtual ~IVolumeInputStream() {}

    /**
     *	@brief 读取字节流
     *
     *	@param buf 存放数据的缓冲区的起始地址
     *	@param size 需要读取的字节数量
     */
    virtual int Read(char* buf, int size) = 0;

    /**
     *	@brief 用来释放相关资源，在使用完后必须调用。
     */
    virtual void Close() = 0;
};

typedef std::shared_ptr<IVolumeInputStream> IVolumeInputStreamPtr;

/**
 *	@brief 上传数据的入口类
 */
class IUpload
{
public:

    virtual ~IUpload() {}

    /**
     *	@brief 获取当前Upload的唯一标识符
     *
     *	@return 获取Upload的ID
     */
    virtual std::string GetUploadId() = 0;

    /**
     *	@brief 获取当前Upload使用的Tunnel配额组
     *
     *	@return 配额组名
     */
    virtual std::string GetQuotaName() = 0;

    /**
     *	@brief 获取当前Upload操作的ODPS表的RecordSchema
     *
     *	@return 返回IODPSTableSchema类型指针
     */
    virtual IODPSTableSchema* GetSchema() = 0;

    #ifdef ODPS_SDK_ENABLE_ARROW
    virtual std::shared_ptr<arrow::Schema> GetArrowSchema() = 0;
    /**
     *	@brief 创建IArrowRecordWriter用来将Record写入到指定表中
     *
     *	@param blockId 所写入表文件的blockId blockId用于标识此次上传的数据
     *	取值范围：[0,20000]， 当数据上传失败，可以根据blockId重新上传。
     *	@return 返回IArrowRecordWriter的share_ptr类
     */
    virtual IArrowRecordWriterPtr OpenArrowWriter(const uint32_t blockId, const CompressOption& compress = CompressOption::NO_COMPRESS) = 0;
    #endif

    /**
     *	@brief 获取当前Upload的状态
     *
     *	Upload主要有7种状态，状态说明如下
     *	UNKNOWN server端刚创建一个session时设置的初始值
     *	NORMAL 创建upload对象成功
     *	CLOSING 当调用commit方法(结束上传)时，服务端会先把状态置为CLOSING。
     *	CLOSED 完成结束上传(即把数据移动到结果表所在目录)后
     *	CANCELED 调用abort取消上传(已上传数据无效，不可用)
     *	EXPIRED 上传超时
     *	CRITICAL 服务出错
     *
     *	@return 返回Upload的状态
     */
    virtual std::string GetStatus() = 0;

    /**
     *	@brief 创建IRecordWriter用来将Record写入到指定表中
     *
     *	@param blockId 所写入表文件的blockId blockId用于标识此次上传的数据
     *	取值范围：[0,20000]， 当数据上传失败，可以根据blockId重新上传。
     *	@return 返回IRecordWriter的share_ptr类
     */
    virtual IRecordWriterPtr OpenWriter(const uint32_t blockId, const bool compress = false) = 0;

    virtual IRecordWriterPtr OpenWriter(const uint32_t blockId, const CompressOption& compress) = 0;

    /**
     *	@brief 提交本次上传的所有block，并结束本次上传
     *	必须调用Commit，数据才会进入表中并对用户可见
     *
     *	@param 已经成功上传的block id列表
     */
    virtual void Commit(const std::vector<uint32_t>& blocks) = 0;
    virtual void Commit() = 0;
    virtual std::vector<int64_t> GetBlockList() = 0;

    /**
     *	@brief 创建用于上传的 buffer record
     */
    virtual ODPSTableRecordPtr CreateBufferRecord() = 0;
};

typedef std::shared_ptr<IUpload> IUploadPtr;

class IStreamUpload
{
public:
    virtual ~IStreamUpload() {}
    virtual std::string GetUploadId() = 0;
    virtual IODPSTableSchema* GetSchema() = 0;
    virtual IRecordPackPtr CreateRecordPack() = 0;
    virtual IRecordPackPtr CreateRecordPack(const CompressOption& option) = 0;
    virtual IRecordPackPtr CreateRecordPack(const CompressOption& option, size_t reserveSize) = 0;
    virtual ODPSTableRecordPtr CreateBufferRecord() = 0;
    virtual std::string GetQuotaName() = 0;
    #ifdef ODPS_SDK_ENABLE_ARROW
    virtual std::shared_ptr<arrow::Schema> GetArrowSchema() = 0;
    virtual IEOSStreamPtr CreateEOSStream(const std::string& clientId, const CompressOption& opt) = 0;
    #endif
};

typedef std::shared_ptr<IStreamUpload> IStreamUploadPtr;

class IListener
{
public:
    FlushResult flushResult;

    virtual void OnFlush(const FlushResult& result) = 0;
    virtual bool OnFlushFail(const std::string& error, int retry) = 0;
};

typedef std::shared_ptr<IListener> IListenerPtr;

class IUpsertStream
{
public:
    virtual ~IUpsertStream() {}
    virtual void Upsert(ODPSTableRecord& r) = 0;
    virtual void Upsert(ODPSTableRecord& r, std::vector<std::string> upsertCols) = 0;
    virtual void Delete(ODPSTableRecord& r) = 0;
    virtual void Flush(bool flushAll = true) = 0;
    virtual void Close() = 0;
    virtual void Reset() = 0;

    virtual void setListener(IListenerPtr listener) = 0;
    virtual int64_t getSlotBufferSize() = 0;
    virtual int64_t getMaxBufferSize() = 0;
    virtual void setSlotBufferSize(int64_t slotBufferSize) = 0;
    virtual void setMaxBufferSize(int64_t maxBufferSize) = 0;
};

typedef std::shared_ptr<IUpsertStream> IUpsertStreamPtr;

class IUpsert
{
public:
    virtual ~IUpsert() {}
    virtual std::string GetUpsertId() = 0;
    virtual std::string GetStatus() = 0;
    virtual int64_t GetCommitTimeout() = 0;
    virtual void SetCommmitTimeout(int64_t commitTimeout) = 0;
    virtual IODPSTableSchema* GetSchema() = 0;
    virtual bool supportPartialUpdate() = 0;
    virtual void Commit(bool async) = 0;
    virtual void Abort() = 0;
    virtual ODPSTableRecordPtr CreateUpsertRecord() = 0;
    virtual IUpsertStreamPtr CreateUpsertStream(const CompressOption& option = CompressOption::ZLIB_COMPRESS) = 0;
};

typedef std::shared_ptr<IUpsert> IUpsertPtr;

/**
 *	@brief 下载数据操作的入口类
 */
class IDownload
{
public:
    virtual ~IDownload() {}

    /**
     *	@brief 获取当前Download的唯一标识符
     *
     *	@return 返回Download的ID
     */
    virtual std::string GetDownloadId() = 0;

    /**
     *	@brief 获取当前Download使用的Tunnel配额组
     *
     *	@return 配额组名
     */
    virtual std::string GetQuotaName() = 0;

    /**
     *	@brief 获取当前Download操作的ODPS表的RecordSchema
     *
     *	@return 返回IODPSTableSchema类型指针
     */
    virtual IODPSTableSchema* GetSchema() = 0;

    /**
     *	@brief 获取当前Download的状态
     *
     *	Download有四种状态，状态说明如下
     *	UNKNOWN server端刚创建一个session时设置的初始值
     *	NORMAL 创建Download对象成功
     *	CLOSED 下载结束后
     *	EXPIRED 下载超时
     *
     *	@return 返回Download的状态
     */
    virtual std::string GetStatus() = 0;

    /**
     *	@brief 获取当前Download操作的表的Record数
     *
     *	@return 返回表的Record数
     */
    virtual uint64_t GetRecordCount() = 0;

    /**
     *	@brief 创建IRecordReader用来读取指定表
     *
     *	@param start 本次要读的Record的起始位置
     *	@param count 本次要读的Record的数量
     *	@return 返回IRecordReaderPtr的share_ptr类
     */
    virtual IRecordReaderPtr OpenReader(const uint64_t start, const uint64_t count, const bool compress = false) = 0;

    /**
     *	@brief 创建IRecordReader用来读取指定表
     *
     *	@param start 本次要读的Record的起始位置
     *	@param count 本次要读的Record的数量
     *	@param colNames 需要下载的数据列
     *	@return 返回IRecordReaderPtr类
     */
    virtual IRecordReaderPtr OpenReader(const uint64_t start, const uint64_t count, const std::vector<std::string>& colNames, const bool compress = false) = 0;

    /**
     *	@brief 创建IRecordReader用来读取指定表
     *
     *	@param start 本次要读的Record的起始位置
     *	@param count 本次要读的Record的数量
     *	@param colNames 需要下载的数据列
     *	@return 返回IRecordReaderPtr类
     */
    virtual IRecordReaderPtr OpenReader(const uint64_t start, const uint64_t count, const std::vector<std::string>& colNames, const CompressOption& compress, bool disableModifiedCheck) = 0;

    virtual IBufferRecordReaderPtr OpenBufferReader(const uint64_t start, const uint64_t count, const uint64_t bufferRecordCount, const std::vector<std::string>& colNames, const CompressOption& compress, bool disableModifiedCheck = false) = 0;

    #ifdef ODPS_SDK_ENABLE_ARROW
    virtual std::shared_ptr<arrow::Schema> GetArrowSchema() = 0;
    /**
     *	@brief 创建IArrowRecordReader用来读取指定表
     *
     *	@param start 本次要读的Record的起始位置
     *	@param count 本次要读的Record的数量
     *	@return 返回IArrowRecordReaderPtr的share_ptr类
     */
    virtual IArrowRecordReaderPtr OpenArrowReader(const uint64_t start, const uint64_t count, const std::vector<std::string>& colNames = {}, const CompressOption& compress = CompressOption::NO_COMPRESS, bool disableModifiedCheck = false, const uint64_t rawSize = 0) = 0;
    /**
     *	@brief 创建IBufferArrowRecordReader用来批量读取指定表
     *
     *	@param start 本次要读的Record的起始位置
     *	@param count 本次要读的Record的总数量
     *	@param bufferRecordCount 每次缓冲读取的最大Record数量，服务端返回的RecordBatch记录数不超过此值
     *	@param colNames 需要下载的数据列
     *	@param compress 压缩选项
     *	@param disableModifiedCheck 是否禁用修改检查
     *	@return 返回IBufferArrowRecordReaderPtr的share_ptr类
     */
    virtual IBufferArrowRecordReaderPtr OpenBufferArrowReader(const uint64_t start, const uint64_t count, const uint64_t bufferRecordCount, const std::vector<std::string>& colNames = {}, const CompressOption& compress = CompressOption::NO_COMPRESS, bool disableModifiedCheck = false) = 0;
    /**
     *	@brief 创建IBufferArrowRecordReader用来批量读取指定表
     *
     *	@param start 本次要读的Record的起始位置
     *	@param count 本次要读的Record的总数量
     *	@param bufferRecordCount 每次缓冲读取的最大Record数量，服务端返回的RecordBatch记录数不超过此值
     *	@param rawSize 期望一次读取的buffer的最大size（字节数），服务端返回的RecordBatch大小不超过此值, 0 表示不限制
     *	@param colNames 需要下载的数据列
     *	@param compress 压缩选项
     *	@param disableModifiedCheck 是否禁用修改检查
     *	@return 返回IBufferArrowRecordReaderPtr的share_ptr类
     *	@note 服务端会同时满足两个条件：记录数不超过bufferRecordCount，且数据大小不超过rawSize
     */
    virtual IBufferArrowRecordReaderPtr OpenBufferArrowReader(const uint64_t start, const uint64_t count, const uint64_t bufferRecordCount, const uint64_t rawSize, const std::vector<std::string>& colNames = {}, const CompressOption& compress = CompressOption::NO_COMPRESS, bool disableModifiedCheck = false) = 0;
    #endif

    /**
     *	@brief 完成本次Download，在使用完后必须调用。
     */
    virtual void Complete() = 0;
};

typedef std::shared_ptr<IDownload> IDownloadPtr;

/**
 *	@brief 文件上传操作的入口类
 */
class IVolumeUpload
{
public:

    virtual ~IVolumeUpload() {}

    /**
     *	@brief 获取当前VolumeUpload的唯一标识符
     *
     *	@return 返回Upload的ID
     */
    virtual std::string GetUploadId() = 0;

    /**
     *	@brief 获取当前Upload使用的Tunnel配额组
     *
     *	@return 配额组名
     */
    virtual std::string GetQuotaName() = 0;

    /**
     *	@brief 获取当前Upload的状态
     *
     *	@return 返回Upload的状态
     */
    virtual std::string GetStatus() = 0;

    /**
     *	@brief 创建IVolumeOutputStreamPtr用来将数据流写入到指定文件中
     *
     *	@param filename 所写入文件的名称
     *	@return 返回IVolumeOutputStreamPtr类
     */
    virtual IVolumeOutputStreamPtr OpenOutputStream(const std::string& fileName, const bool compress = false) = 0;

    /**
     *	@brief 提交本次上传的所有文件，并结束本次上传
     *	必须调用Commit，数据才会进入表中并对用户可见
     *
     *	@param 已经成功上传的文件列表
     */
    virtual void Commit(const std::vector<std::string>& fileNames) = 0;
};

typedef std::shared_ptr<IVolumeUpload> IVolumeUploadPtr;

/**
 *	@brief 下载文件操作的入口类
 */
class IVolumeDownload
{
public:
    virtual ~IVolumeDownload() {}

    /**
     *	@brief 获取当前Download的唯一标识符
     *
     *	@return 返回Download的ID
     */
    virtual std::string GetDownloadId() = 0;

    /**
     *	@brief 获取当前Download使用的Tunnel配额组
     *
     *	@return 配额组名
     */
    virtual std::string GetQuotaName() = 0;

    /**
     *	@brief 获取当前Download的状态
     *
     *	Download有四种状态，状态说明如下
     *	UNKNOWN server端刚创建一个session时设置的初始值
     *	NORMAL 创建Download对象成功
     *	CLOSED 下载结束后
     *	EXPIRED 下载超时
     *
     *	@return 返回Download的状态
     */
    virtual std::string GetStatus() = 0;

    /**
     *	@brief 获取当前VolumeDownload操作的File的长度
     *
     *	@return 返回当前操作文件的长度
     */
    virtual uint64_t GetFileLength() = 0;

    /**
     *	@brief 创建IVolumeInputStreamPtr用来读取指定文件
     *
     *	@param start 本次要读取文件的起始位置
     *	@param length 本次要读取文件的字节数量
     *	@return 返回IVolumeInputStreamPtr类
     */
    virtual IVolumeInputStreamPtr OpenInputStream(const uint64_t start = 0, const uint64_t length = std::numeric_limits<uint64_t>::max() / 2, const bool compress = false) = 0;

    /**
     *	@brief 完成本次VolumeDownload，在使用完后必须调用。
     */
    virtual void Complete() = 0;
};

typedef std::shared_ptr<IVolumeDownload> IVolumeDownloadPtr;

/**
 *	@brief 访问ODPS DataTunnel服务的入口类
 */
class OdpsTunnel
{
public:
    /**
     *	@brief 构造函数
     */
    OdpsTunnel();

    /**
     *	@brief 使用Configuration类来初始化Tunnel.
     *
     *	@param conf 配置信息
     */
    void Init(const Configuration& conf) { this->conf = conf; }

    /**
     *	@brief 构造一个新的Upload对象.
     *
     *	@param project 上传数据表所在的project名称
     *	@param table 上传数据表名称
     *	@param partition 上传数据表的partition描述，格式如下: pt=xxx,dt=xxx 如果不是分区表,partition值用null代替
     *	@param uploadId Upload的唯一标识符，如果没有UploadId则使用默认值
     */
    IUploadPtr CreateUpload(const std::string& project, const std::string& table, const std::string& partition = "",
                            const std::string& uploadId = "", bool overwrite = false, const std::string& schemaName = "");

    /**
     *	@brief 构造一个新的StreamUpload对象.
     *
     *	@param project 上传数据表所在的project名称
     *	@param table 上传数据表名称
     *	@param partition 上传数据表的partition描述，格式如下: pt=xxx,dt=xxx 如果不是分区表,partition值用null代替
     *	@param createPartition 自动创建分区，partition参数非空时生效，默认不创建
     *  @param slotNum 服务端slot数，默认值0，由服务端自动分配
     *  @param compress 压缩算法，默认zlib压缩
     */
    IStreamUploadPtr CreateStreamUpload(const std::string& project, const std::string& table);
    IStreamUploadPtr CreateStreamUpload(const std::string& project, const std::string& table, int32_t slotNum, const CompressOption& compress);
    IStreamUploadPtr CreateStreamUpload(const std::string& project, const std::string& table, int32_t slotNum, const CompressOption& compress, const std::string& schemaName);
    IStreamUploadPtr CreateStreamUpload(const std::string& project, const std::string& table, const std::string& partition);
    IStreamUploadPtr CreateStreamUpload(const std::string& project, const std::string& table, const std::string& partition, bool createPartition, int32_t slotNum, const CompressOption& compress, const std::string& schemaName = "");

    IUpsertPtr CreateUpsert(const std::string& project, const std::string& table, const std::string& partition = "",
                            const std::string& upsertId = "", const std::string& schemaName = "");

    /**
     *	@brief 构造一个新的Download对象.
     *
     *	@param project 下载数据表所在的project名称
     *	@param table 下载数据表名称
     *	@param partition 下载数据表的partition描述，格式如下: pt=xxx,dt=xxx 如果不是分区表,partition值用null代替
     *	@param downloadId Download的唯一标识符，如果没有DownloadId则使用默认值
     */
    IDownloadPtr CreateDownload(
            const std::string& project,
            const std::string& table,
            const std::string& partition = "",
            const std::string& downloadId = "",
            const std::string& schemaName = "");

    /**
     *	@brief 构造一个新的VolumeUpload对象.
     *
     *	@param project 上传文件所在的project名称
     *	@param volume 上传文件所在Volume的名称
     *	@param partition 上传文件的partition描述，由字母,数字,下划线组成，3-32个字符，举例如下: my_pt_001
     *	@param uploadId VolumeUpload的唯一标识符，如果没有UploadId则使用默认值
     */
    IVolumeUploadPtr CreateVolumeUpload(const std::string& project, const std::string& volume
            , const std::string& partition, const std::string& uploadId = "");

    /**
     *	@brief 构造一个新的VolumeDownload对象.
     *
     *	@param project 下载文件所在的project名称
     *	@param volume 下载文件所在Volume的名称
     *	@param partition 下载文件的partition描述，由字母,数字,下划线组成，3-32个字符，举例如下: my_pt_001
     *	@param downloaddId VolumeDownload的唯一标识符，如果没有DownloadId则使用默认值
     */
    IVolumeDownloadPtr CreateVolumeDownload(const std::string& project, const std::string& volume
            , const std::string& partition, const std::string& fileName, const std::string& downloadId = "");

protected:
    Configuration conf;	/**< tunnel配置信息*/
};

}}}
#endif
