#include <iostream>
#include <cstdlib>

#include "example/common/example_config.h"
#include "include/odps_tunnel.h"

using namespace std;
using namespace apsara::odps::sdk;

void GenericPrintComplexType(IODPSProtoSerializablePtr complex);
void GenericPrintRecord(ODPSTableRecord& r);

int main(int argc, char* argv[])
{
    ExampleConfig config;
    if (!LoadExampleConfig(config))
        return 1;

    string tunnelEp = config.mTunnelEndpoint;
    string project = config.mProjectName;
    string table = "complex_type_test";
    Account account(ACCOUNT_ALIYUN, config.mAccessId, config.mAccessKey);
    Configuration conf;
    conf.SetAccount(account);
    conf.SetTunnelEndpoint(tunnelEp);
    conf.SetUserAgent(UserAgent("COMPLEX_EXAMPLE", "1.0.0.0"));

    bool doUpload = false;
    if (argc > 1 && string(argv[1]) == "upload") doUpload = true;
    bool doNullValuesTest = false;
    if (argc > 2 && string(argv[2]) == "nulltest") doNullValuesTest = true;

    OdpsTunnel tunnel;
    if(!doUpload)
    {
        try
        {
            tunnel.Init(conf);
            IDownloadPtr download = tunnel.CreateDownload(project, table);
            string downloadId = download->GetDownloadId();
            std::cout << "downloadId:" << downloadId << " --> ";
            std::cout << download->GetStatus() << std::endl;
            IRecordReaderPtr rr = download->OpenReader(0, 100000);
            ODPSTableRecordPtr _r = rr->CreateBufferRecord();
            ODPSTableRecord& buffer = *_r;
            while (rr->Read(buffer))
            {
                GenericPrintRecord(buffer);
            }
            rr->Close();
            download->Complete();

            std::cout << "download completed." << std::endl;
        }
        catch (const OdpsTunnelException& e)
        {
            std::cerr << "OdpsTunnelException: " << e.ToString() << endl;
        }
        catch (const exception& e)
        {
            cerr << "std::exception: " << e.what() << endl;
        }
        return 0;
    }


    cout << "starting upload..." << endl;

    // know schema beforehand..
    /*
    biarray     | array<bigint>
    bimap       | map<string,bigint>
    bistruct    | struct<a:string,b:bigint,c:double>
    nsarray     | array<map<string,bigint>>
    nsmap       | map<string,struct<a:bigint,b:bigint>>
    nsstruct    | struct<a:array<array<string>>,b:struct<b1:string,b2:bigint>,c:map<string,string>>
    */

    try
    {
        tunnel.Init(conf);
        IUploadPtr upload = tunnel.CreateUpload(project, table);
        string uploadId = upload->GetUploadId();
        cout << "uploadId: " << uploadId << " --> ";
        cout << upload->GetStatus() << endl;
        IRecordWriterPtr wr = upload->OpenWriter(0);
        IODPSTableSchema* schema = upload->GetSchema();
        // setup a normal record
        if (!doNullValuesTest)
        {
            ODPSTableRecordPtr _r = upload->CreateBufferRecord();
            ODPSTableRecord& buffer = *_r;
            shared_ptr<ODPSArray> biarray = make_shared<ODPSArray>(schema->GetTableColumn(0).GetTypeInfo());
            shared_ptr<ODPSMap> bimap = make_shared<ODPSMap>(schema->GetTableColumn(1).GetTypeInfo());
            shared_ptr<ODPSStruct> bistruct = make_shared<ODPSStruct>(schema->GetTableColumn(2).GetTypeInfo());
            shared_ptr<ODPSArray> nsarray = make_shared<ODPSArray>(schema->GetTableColumn(3).GetTypeInfo());
            shared_ptr<ODPSMap> nsmap = make_shared<ODPSMap>(schema->GetTableColumn(4).GetTypeInfo());
            shared_ptr<ODPSStruct> nsstruct = make_shared<ODPSStruct>(schema->GetTableColumn(5).GetTypeInfo());
            biarray->AppendBigIntValue(4);
            biarray->AppendBigIntValue(5);
            biarray->AppendBigIntValue(6);
            bimap->SetBigIntValue<string>("e", 5);
            bimap->SetBigIntValue<string>("f", 6);
            bistruct->SetStringValue(bistruct->GetMemberIndex("a"), "sss");
            bistruct->SetBigIntValue(bistruct->GetMemberIndex("b"), 5);
            bistruct->SetDoubleValue(bistruct->GetMemberIndex("c"), 6.0);
            shared_ptr<ODPSMap> nsarray_map1 = make_shared<ODPSMap>(nsarray->GetTypeInfo().mSubTypes[0]);
            nsarray_map1->SetBigIntValue<string>("e", 5);
            nsarray_map1->SetBigIntValue<string>("f", 6);
            shared_ptr<ODPSMap> nsarray_map2 = make_shared<ODPSMap>(nsarray->GetTypeInfo().mSubTypes[0]);
            nsarray_map2->SetBigIntValue<string>("g", 7);
            nsarray_map2->SetBigIntValue<string>("h", 8);
            nsarray->AppendMapValue(nsarray_map1);
            nsarray->AppendMapValue(nsarray_map2);
            shared_ptr<ODPSStruct> nsmap_struct1 = make_shared<ODPSStruct>(nsmap->GetValueTypeInfo());
            nsmap_struct1->SetBigIntValue(nsmap_struct1->GetMemberIndex("a"), 5);
            nsmap_struct1->SetBigIntValue(nsmap_struct1->GetMemberIndex("b"), 6);
            shared_ptr<ODPSStruct> nsmap_struct2 = make_shared<ODPSStruct>(nsmap->GetValueTypeInfo());
            nsmap_struct2->SetBigIntValue(nsmap_struct2->GetMemberIndex("a"), 7);
            nsmap_struct2->SetBigIntValue(nsmap_struct2->GetMemberIndex("b"), 8);
            nsmap->SetStructValue<string>("e", nsmap_struct1);
            nsmap->SetStructValue<string>("f", nsmap_struct2);
            shared_ptr<ODPSArray> nsstruct_array_array1 = make_shared<ODPSArray>(nsstruct->GetMemberType(nsstruct->GetMemberIndex("a")).mSubTypes[0]);
            nsstruct_array_array1->AppendStringValue("4");
            nsstruct_array_array1->AppendStringValue("5");
            shared_ptr<ODPSArray> nsstruct_array_array2 = make_shared<ODPSArray>(nsstruct->GetMemberType(nsstruct->GetMemberIndex("a")).mSubTypes[0]);
            nsstruct_array_array2->AppendStringValue("6");
            nsstruct_array_array2->AppendStringValue("7");
            shared_ptr<ODPSArray> nsstruct_array = make_shared<ODPSArray>(nsstruct->GetMemberType(nsstruct->GetMemberIndex("a")));
            nsstruct_array->AppendArrayValue(nsstruct_array_array1);
            nsstruct_array->AppendArrayValue(nsstruct_array_array2);
            nsstruct->SetArrayValue(nsstruct->GetMemberIndex("a"), nsstruct_array);
            shared_ptr<ODPSStruct> nsstruct_struct = make_shared<ODPSStruct>(nsstruct->GetMemberType(nsstruct->GetMemberIndex("b")));
            nsstruct_struct->SetStringValue(nsstruct_struct->GetMemberIndex("b1"), "testf");
            nsstruct_struct->SetBigIntValue(nsstruct_struct->GetMemberIndex("b2"), 9);
            nsstruct->SetStructValue(nsstruct->GetMemberIndex("b"), nsstruct_struct);
            shared_ptr<ODPSMap> nsstruct_map = make_shared<ODPSMap>(nsstruct->GetMemberType(nsstruct->GetMemberIndex("c")));
            nsstruct_map->SetStringValue<string>("testf1", "hello");
            nsstruct_map->SetStringValue<string>("testf2", "world");
            nsstruct->SetMapValue(nsstruct->GetMemberIndex("c"), nsstruct_map);
            buffer.SetArrayValue(0, biarray);
            buffer.SetMapValue(1, bimap);
            buffer.SetStructValue(2, bistruct);
            buffer.SetArrayValue(3, nsarray);
            buffer.SetMapValue(4, nsmap);
            buffer.SetStructValue(5, nsstruct);
            cout << "TO WRITE:" << std::endl;
            GenericPrintRecord(buffer);
            wr->Write(buffer);
        }
        else
        {
            // write some nulls.
            ODPSTableRecordPtr _r = upload->CreateBufferRecord();
            ODPSTableRecord& buffer = *_r;
            shared_ptr<ODPSArray> biarray = make_shared<ODPSArray>(schema->GetTableColumn(0).GetTypeInfo());
            shared_ptr<ODPSMap> bimap = make_shared<ODPSMap>(schema->GetTableColumn(1).GetTypeInfo());
            shared_ptr<ODPSStruct> bistruct = make_shared<ODPSStruct>(schema->GetTableColumn(2).GetTypeInfo());
            shared_ptr<ODPSArray> nsarray = make_shared<ODPSArray>(schema->GetTableColumn(3).GetTypeInfo());
            shared_ptr<ODPSMap> nsmap = make_shared<ODPSMap>(schema->GetTableColumn(4).GetTypeInfo());
            shared_ptr<ODPSStruct> nsstruct = make_shared<ODPSStruct>(schema->GetTableColumn(5).GetTypeInfo());
            biarray->AppendBigIntValue(4);
            biarray->AppendBigIntValue(5);
            biarray->AppendNull();
            bimap->SetBigIntValue<string>("e", 5);
            bimap->SetNullValue(string("f"));
            bistruct->SetStringValue(bistruct->GetMemberIndex("a"), "sss");
            bistruct->SetNullValue(bistruct->GetMemberIndex("b"));
            bistruct->SetDoubleValue(bistruct->GetMemberIndex("c"), 6.0);
            shared_ptr<ODPSMap> nsarray_map2 = make_shared<ODPSMap>(nsarray->GetTypeInfo().mSubTypes[0]);
            nsarray_map2->SetBigIntValue<string>("g", 7);
            nsarray_map2->SetBigIntValue<string>("h", 8);
            nsarray->AppendNull();
            nsarray->AppendMapValue(nsarray_map2);
            shared_ptr<ODPSStruct> nsmap_struct2 = make_shared<ODPSStruct>(nsmap->GetValueTypeInfo());
            nsmap_struct2->SetBigIntValue(nsmap_struct2->GetMemberIndex("a"), 7);
            nsmap_struct2->SetBigIntValue(nsmap_struct2->GetMemberIndex("b"), 8);
            nsmap->SetNullValue(string("e"));
            nsmap->SetStructValue<string>("f", nsmap_struct2);
            shared_ptr<ODPSArray> nsstruct_array_array1 = make_shared<ODPSArray>(nsstruct->GetMemberType(nsstruct->GetMemberIndex("a")).mSubTypes[0]);
            nsstruct_array_array1->AppendStringValue("4");
            nsstruct_array_array1->AppendNull();
            shared_ptr<ODPSArray> nsstruct_array_array2 = make_shared<ODPSArray>(nsstruct->GetMemberType(nsstruct->GetMemberIndex("a")).mSubTypes[0]);
            nsstruct_array_array2->AppendStringValue("6");
            nsstruct_array_array2->AppendStringValue("7");
            shared_ptr<ODPSArray> nsstruct_array = make_shared<ODPSArray>(nsstruct->GetMemberType(nsstruct->GetMemberIndex("a")));
            nsstruct_array->AppendArrayValue(nsstruct_array_array1);
            nsstruct_array->AppendArrayValue(nsstruct_array_array2);
            nsstruct->SetArrayValue(nsstruct->GetMemberIndex("a"), nsstruct_array);
            shared_ptr<ODPSStruct> nsstruct_struct = make_shared<ODPSStruct>(nsstruct->GetMemberType(nsstruct->GetMemberIndex("b")));
            nsstruct_struct->SetStringValue(nsstruct_struct->GetMemberIndex("b1"), "testf");
            nsstruct_struct->SetBigIntValue(nsstruct_struct->GetMemberIndex("b2"), 9);
            nsstruct->SetStructValue(nsstruct->GetMemberIndex("b"), nsstruct_struct);
            nsstruct->SetNullValue(nsstruct->GetMemberIndex("c"));
            buffer.SetArrayValue(0, biarray);
            buffer.SetMapValue(1, bimap);
            buffer.SetStructValue(2, bistruct);
            buffer.SetArrayValue(3, nsarray);
            buffer.SetMapValue(4, nsmap);
            buffer.SetStructValue(5, nsstruct);
            cout << "TO WRITE:" << std::endl;
            GenericPrintRecord(buffer);
            wr->Write(buffer);
        }

        wr->Close();
        std::cout << "Status --> " << upload->GetStatus() << std::endl;
        upload->Commit({0});
        std::cout << "Commit --> " << upload->GetStatus() << std::endl;
    }
    catch (const OdpsTunnelException& e)
    {
        std::cerr << "OdpsTunnelException: " << e.ToString() << endl;
    }
    catch (const exception& e)
    {
        cerr << "std::exception: " << e.what() << endl;
    }
    cout << "Upload completed." << endl;
    return 0;
}

void GenericPrintRecord(ODPSTableRecord& r)
{
    IODPSTableSchema* sch = r.GetSchema();
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
            case ODPS_ARRAY:
            {
                shared_ptr<ODPSArray> v = r.GetArrayValue(i);
                if(!v)
                {
                    cout << "NULL" << "|";
                }
                else
                {
                    GenericPrintComplexType(v);
                    cout << "|";
                }
                break;
            }
            case ODPS_MAP:
            {
                shared_ptr<ODPSMap> v = r.GetMapValue(i);
                if(!v)
                {
                    cout << "NULL" << "|";
                }
                else
                {
                    GenericPrintComplexType(v);
                    cout << "|";
                }
                break;
            }
            case ODPS_STRUCT:
            {
                shared_ptr<ODPSStruct> v = r.GetStructValue(i);
                if(!v)
                {
                    cout << "NULL" << "|";
                }
                else
                {
                    GenericPrintComplexType(v);
                    cout << "|";
                }
                break;
            }
            default:
            {
                std::cout << "##" << GetTypeName(type) << "##|";
            }
        }
    }
    std::cout << std::endl;
}

#define ARRAY_VALUECASE(ODPSTYPE, SIGNTYPE) \
case ODPSTYPE: \
{ \
    for(uint32_t i = 0; i < arrayValue->Size(); i++) \
    { \
        if (i != 0) cout << ","; \
        if (arrayValue->IsNull(i)) cout << "NULL"; \
        else cout << arrayValue->Get##SIGNTYPE(i); \
    } \
} \
break

#define ARRAY_COMPLEXCASE(ODPSTYPE, SIGNTYPE) \
case ODPSTYPE: \
{ \
    for(uint32_t i = 0; i < arrayValue->Size(); i++) \
    { \
        if (i != 0) cout << ","; \
        if (arrayValue->IsNull(i)) cout << "NULL"; \
        else GenericPrintComplexType(arrayValue->Get##SIGNTYPE(i)); \
    } \
} \
break

#define MAP_PRINT_VAL(ODPSTYPE, SIGNTYPE) \
case ODPSTYPE: \
{ \
    if(!(mapValue->ContainsAndNotNull(keyvec[i]))) cout << "NULL"; \
    else cout << mapValue->Get##SIGNTYPE(keyvec[i]); \
} \
break

#define MAP_PRINT_VAL_COMPLEX(ODPSTYPE, SIGNTYPE) \
case ODPSTYPE: \
{ \
    if(!(mapValue->ContainsAndNotNull(keyvec[i]))) cout << "NULL"; \
    else GenericPrintComplexType(mapValue->Get##SIGNTYPE(keyvec[i])); \
} \
break

#define STRUCT_PRINT_VAL(ODPSTYPE, SIGNTYPE, INDEX) \
case ODPSTYPE: \
{ \
    if (structValue->IsNull(INDEX)) cout << "NULL"; \
    else cout << structValue->Get##SIGNTYPE(INDEX); \
} \
break

#define STRUCT_PRINT_VAL_COMPLEX(ODPSTYPE, SIGNTYPE, INDEX) \
case ODPSTYPE: \
{ \
    if (structValue->IsNull(INDEX)) cout << "NULL"; \
    else GenericPrintComplexType(structValue->Get##SIGNTYPE(INDEX)); \
} \
break;

template<typename T>
void MapPrintImpl(shared_ptr<ODPSMap> mapValue)
{
    vector<T> keyvec = mapValue->Keys<T>();
    ODPSColumnTypeInfo valueTypeInfo = mapValue->GetValueTypeInfo();
    for(size_t i = 0; i < keyvec.size(); i++)
    {
        if (i != 0) cout << ",";
        cout << keyvec[i] << ":";
        switch(valueTypeInfo.mType)
        {
            MAP_PRINT_VAL(ODPS_BIGINT, BigInt);
            MAP_PRINT_VAL(ODPS_STRING, String);
            MAP_PRINT_VAL(ODPS_DOUBLE, Double);
            MAP_PRINT_VAL(ODPS_FLOAT, Float);
            MAP_PRINT_VAL_COMPLEX(ODPS_ARRAY, Array);
            MAP_PRINT_VAL_COMPLEX(ODPS_MAP, Map);
            MAP_PRINT_VAL_COMPLEX(ODPS_STRUCT, Struct);
            default:
            cout << "##" << GetTypeName(valueTypeInfo.mType) << "##";
        }
    }
}

void MapPrintGeneric(shared_ptr<ODPSMap> mapValue)
{
    switch(mapValue->GetKeyCType())
    {
        case apsara::odps::sdk::ODPS_STRING:
            MapPrintImpl<string>(mapValue);
            break;
        case apsara::odps::sdk::ODPS_BIGINT:
            MapPrintImpl<int64_t>(mapValue);
            break;
        case apsara::odps::sdk::ODPS_FLOAT:
            MapPrintImpl<float>(mapValue);
            break;
        case apsara::odps::sdk::ODPS_DOUBLE:
            MapPrintImpl<double>(mapValue);
            break;
        default:
            cout << "##UNKNOWN##";
    }
}

void GenericPrintComplexType(IODPSProtoSerializablePtr complex)
{
    if (!complex)
    {
        cout << "NULL";
        return;
    }
    ODPSColumnTypeInfo typeinfo = complex->GetTypeInfo();
    switch(typeinfo.mType)
    {
        case ODPS_ARRAY:
        {
            shared_ptr<ODPSArray> arrayValue = dynamic_pointer_cast<ODPSArray>(complex);
            cout << "array<";
            ODPSColumnType elementType = arrayValue->GetElementType();
            switch(elementType)
            {
                ARRAY_VALUECASE(ODPS_BIGINT, BigInt);
                ARRAY_VALUECASE(ODPS_DOUBLE, Double);
                ARRAY_VALUECASE(ODPS_STRING, String);
                ARRAY_COMPLEXCASE(ODPS_ARRAY, Array);
                ARRAY_COMPLEXCASE(ODPS_MAP, Map);
                ARRAY_COMPLEXCASE(ODPS_STRUCT, Struct);
                default:
                {
                    std::cout << "##" << GetTypeName(elementType) << "##";
                }
            }
            std::cout << ">";
        }
        break;
        case ODPS_MAP:
        {
            shared_ptr<ODPSMap> mapValue = dynamic_pointer_cast<ODPSMap>(complex);
            cout << "map<";
            MapPrintGeneric(mapValue);
            cout << ">";
        }
        break;
        case ODPS_STRUCT:
        {
            shared_ptr<ODPSStruct> structValue = dynamic_pointer_cast<ODPSStruct>(complex);
            cout << "struct<";
            for(uint32_t i = 0; i < structValue->Size(); i++)
            {
                if (i != 0) cout << ",";
                ODPSColumnTypeInfo memberType = structValue->GetMemberType(i);
                cout << memberType.mMemberName << ":";
                switch(memberType.mType)
                {
                    STRUCT_PRINT_VAL(ODPS_BIGINT, BigInt, i);
                    STRUCT_PRINT_VAL(ODPS_FLOAT, Float, i);
                    STRUCT_PRINT_VAL(ODPS_DOUBLE, Double, i);
                    STRUCT_PRINT_VAL(ODPS_STRING, String, i);
                    STRUCT_PRINT_VAL_COMPLEX(ODPS_ARRAY, Array, i);
                    STRUCT_PRINT_VAL_COMPLEX(ODPS_MAP, Map, i);
                    STRUCT_PRINT_VAL_COMPLEX(ODPS_STRUCT, Struct, i);
                    default:
                    {
                        std::cout << "##" << GetTypeName(memberType.mType) << "##";
                    }
                }
            }
            cout << ">";
        }
        break;
        default:
        {
            std::cout << "**" << GetTypeName(typeinfo.mType) << "**";
        }
    }
}