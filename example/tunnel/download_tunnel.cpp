#include <stdlib.h>
#include <iostream>

#include "example/common/example_config.h"
#include "include/odps_tunnel.h"

using namespace std;
using namespace apsara::odps::sdk;


int main(int argc, char *argv[])
{
    ExampleConfig config;
    if (!LoadExampleConfig(config))
        return 1;

    string ep = config.mTunnelEndpoint;
    string odpsEndpoint = config.mOdpsEndpoint;
    string project  = config.mProjectName;
    string table  = "<your_table>";
    uint64_t start = 0;
    uint64_t count = 10;

    if (argc > 1)
        table = argv[1];

    if (argc > 2)
        start = atoi(argv[2]);

    if (argc > 3)
        count = atoi(argv[3]);

    Account account(ACCOUNT_ALIYUN, config.mAccessId, config.mAccessKey);

    Configuration conf;
    conf.SetAccount(account);
    conf.SetTunnelEndpoint(ep);
    conf.SetEndpoint(odpsEndpoint);
    conf.SetUserAgent(UserAgent("DOWNLOAD_EXAMPLE", "1.0.0.0"));

    OdpsTunnel dt;

    try
    {
        dt.Init(conf);

        IDownloadPtr download = dt.CreateDownload(project, table, "pt=20130601,dt=abc");

        std::string downloadId = download->GetDownloadId();

        std::cout << "downloadId:" << downloadId << std::endl;
        std::cout << download->GetStatus() << std::endl;

        std::vector<std::string> cols;

        // column selection
        //cols.push_back("id");
        //cols.push_back("name");

        IRecordReaderPtr rr = download->OpenReader(start,count, cols);

        IODPSTableSchema* sch = rr->GetSchema();
        ODPSTableRecordPtr _r = rr->CreateBufferRecord();
        ODPSTableRecord& r = *_r;

        while(rr->Read(r))
        {
            for (uint32_t i = 0; i < sch->GetColumnCount(); i++)
            {
                ODPSColumnType type = sch->GetTableColumn(i).GetType();
                switch(type)
                {
                    case ODPS_BIGINT:
                    {
                        const int64_t* v = r.GetBigIntValue(i);
                        if (v == NULL)
                        {
                            std::cout << "NULL" << "|";
                        }
                        else
                        {
                            std::cout << *v << "|";
                        }
                        break;
                    }
                    case ODPS_DOUBLE:
                    {
                        const double* v = r.GetDoubleValue(i);
                        if (v == NULL)
                        {
                            std::cout << "NULL" << "|";
                        }
                        else
                        {
                            std::cout << *v << "|";
                        }
                        break;
                    }
                    case ODPS_BOOLEAN:
                    {
                        const bool* v = r.GetBoolValue(i);
                        if (v == NULL)
                        {
                            std::cout << "NULL" << "|";
                        }
                        else
                        {
                            std::cout << *v << "|";
                        }
                        break;
                    }
                    case ODPS_DATETIME:
                    {
                        const int64_t* v = r.GetDatetimeValue(i);
                        if (v == NULL)
                        {
                            std::cout << "NULL" << "|";
                        }
                        else
                        {
                            std::cout << *v << "|";
                        }
                        break;
                    }
                    case ODPS_STRING:
                    {
                        uint32_t len;
                        const char* v = r.GetStringValue(i, len);
                        if (v == NULL)
                        {
                            std::cout << "NULL" << "|";
                        }
                        else
                        {
                            std::cout << std::string(v, len) << "|";
                        }
                        break;
                    }
                    case ODPS_JSON:
                    {
                        uint32_t len;
                        const char* v = r.GetJsonValue(i, len);
                        if (v == NULL)
                        {
                            std::cout << "NULL" << "|";
                        }
                        else
                        {
                            std::cout << std::string(v, len) << "|";
                        }
                        break;
                    }
                    default:
                    {
                        std::cerr << "Unknow column type." << std::endl;
                        exit(-1);
                    }
                }
            }
            std::cout << std::endl;
        }

        rr->Close();
        download->Complete();

        std::cout << "download finish" << std::endl;
    }
    catch(OdpsTunnelException& e)
    {
        std::cerr << "OdpsTunnelException:\n" << e.what() << std::endl;
    }
}
