#ifndef APSARA_ODPS_SDK_EXCEPTION_H
#define APSARA_ODPS_SDK_EXCEPTION_H

#include "error_code.h"
#include <map>
#include <string>
#include <exception>

namespace apsara{ namespace odps{ namespace sdk{

/**
 *	@brief ODPS 异常类
 */
class OdpsException : public std::exception
{
public:
    /**
     *	@brief 构造函数
     *
     *	@param error 错误码
     *	@param message 错误信息
     *	@param requestID 请求标识
     */
    OdpsException(const std::string& error, const std::string& message, const std::string& requestid = "", const std::map<std::string, std::string>& extraInfo = {})
        :mErrorCode(error),
         mErrorMsg(message),
         mRequestId(requestid),
         mExtraInfo(extraInfo)
    {
        mMessage = GetMessage();
    }

    OdpsException(const std::string& message)
        :mMessage(message)
    {
    }

    ~OdpsException() throw() {}

    /**
     *	@brief 获取错误码
     *
     *	@return 错误码
     */
    std::string GetErrorCode() const
    {
        return mErrorCode;
    }

    /**
     *	@brief 获取错误信息
     *
     *	@return 错误信息
     */
    std::string GetErrorMsg() const
    {
        return mErrorMsg;
    }

    /**
     *	@brief 获取请求标识
     *
     *	@return 请求标识
     */
    std::string GetRequestId() const
    {
        return mRequestId;
    }

    const char* what() const throw()
    {
        return  mMessage.c_str();
    }

    std::string ToString() const
    {
        return mMessage;
    }

    std::string GetExtraInfo(const std::string& key) const
    {
        if (mExtraInfo.count(key))
        {
            return mExtraInfo.at(key);
        }
        else
        {
            return "";
        }
    }

private:

    /**
     *	@brief 获取异常信息
     */
    std::string GetMessage()
    {
        std::string message;

        if (!mRequestId.empty())
            message.append("RequestId=").append(mRequestId).append(", ");

        message.append("ErrorCode=").append(mErrorCode);
        message.append(", ErrorMessage=").append(mErrorMsg);

        for (const auto& it : mExtraInfo)
        {
            message.append(", ").append(it.first).append("=").append(it.second);
        }

        return message;
    }

private:
    std::string mErrorCode = LOCAL_ERROR;	/**< 错误码*/
    std::string mErrorMsg;	/**< 错误信息*/
    std::string mRequestId;	/**< 请求标识*/
    std::string mMessage;	/**< 异常消息*/

    std::map<std::string, std::string> mExtraInfo;
};

class OdpsTunnelException: public OdpsException
{
public:
    OdpsTunnelException(const std::string& error, const std::string& message, const std::string& requestid = "", const std::map<std::string, std::string>& extraInfo = {})
        :OdpsException(error, message, requestid, extraInfo)
    {}

    OdpsTunnelException(const std::string& message)
        :OdpsException(message)
    {}
};

#ifdef ODPS_SDK_ENABLE_ARROW
class ExactlyOnceAlreadyWrittenException: public OdpsTunnelException
{
public:
    ExactlyOnceAlreadyWrittenException(const std::string& error, const std::string& message, const std::string& requestid, const std::map<std::string, std::string>& extraInfo);
    int64_t GetSequenceId() const { return mSequenceId; }
    int64_t GetSequenceOffset() const { return mSequenceOffset; }
private:
    int64_t mSequenceId = 0;
    int64_t mSequenceOffset = 0;
};
#endif

}}}
#endif
