#include "tool.h"

using namespace tool;

int main(int argc, char* argv[])
{
    if (argc < 2 || std::string(argv[1]) == "help")
    {
        Println("Usage:", argv[0], "<tool-name> [tool-options]");
        Println("available tools are:");
        for(auto name: Tool::Registry::GetInstance().NameAll())
        {
            std::shared_ptr<Tool> t = Tool::Registry::GetInstance().Create(name);
            Println(name);
            Println(t->GetHelpMessage());
        }
        return -1;
    }
    std::shared_ptr<Tool> t = Tool::Registry::GetInstance().Create(argv[1]);
    return t->Run(argc - 1, &(argv[1]));
}