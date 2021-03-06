#include "lwnn.h"
#include "front/parse.h"
#include "front/ast_passes.h"
#include "front/visualize.h"
#include "front/error.h"
#include "common/string_format.h"
#include "execute/execute.h"


#include "gc/gc.h"

#include <unistd.h>

#include <linenoise.h>
#include <cstring>
#include <fstream>

//static const char* examples[] = {
//        "db", "hello", "hallo", "hans", "hansekogge", "seamann", "quetzalcoatl", "quit", "power", NULL
//};
//
//void completionHook (char const* prefix, linenoiseCompletions* lc) {
//    size_t i;
//
//    for (i = 0;  examples[i] != NULL; ++i) {
//        if (strncmp(prefix, examples[i], strlen(prefix)) == 0) {
//            linenoiseAddCompletion(lc, examples[i]);
//        }
//    }
//}

namespace lwnn {

void executeLine(std::shared_ptr<execute::ExecutionContext> executionContext, std::string lineOfCode, std::string moduleName, bool shouldExecute);
bool executeScript(const std::string &startScriptFilename);

void runInteractive();
void runScript(std::string startScriptFilename);


std::string getHistoryFilePath() {
    std::string home{getenv("HOME")};
    return home + "/.lwnn_history";
}

std::string readLineOfCode() {
    char const* prompt = "\x1b[1;32mlwnn\x1b[0m> ";
    char* lineOfCodeChar = linenoise(prompt);
    if (lineOfCodeChar == NULL) {
        return "";
    }
    std::string lineOfCode{lineOfCodeChar};
    free(lineOfCodeChar);
    return lineOfCode;
}

void help() {
    std::cout << "Meta-Command      Description\n";
    std::cout << "/help             Displays this text.\n";
    std::cout << "/compile          Toggles compilation.  When disabled, the LLVM IR will not be generated.\n";
    std::cout << "/history          Displays command history.\n";
    std::cout << "/exit             Exits the lwnn REPL.\n\n";
    std::cout << "Valid lwnn statements may also be entered.\n";
}

void resultCallback(execute::ExecutionContext*, type::PrimitiveType primitiveType, void *valuePtr) {
    const char *resultPrefix = "result: ";
    switch(primitiveType) {
        case type::PrimitiveType::NotAPrimitive:
            std::cout << "<result was not a primitive>";
            break;
        case type::PrimitiveType::Void:
            break;
        case type::PrimitiveType::Bool:
            std::cout << resultPrefix << (*reinterpret_cast<bool*>(valuePtr) ? "true" : "false") << std::endl;
            break;
        case type::PrimitiveType::Int32:
            std::cout << resultPrefix << *reinterpret_cast<int*>(valuePtr) << std::endl;
            break;
        case type::PrimitiveType::Float:
            std::cout << resultPrefix << *reinterpret_cast<float*>(valuePtr) << std::endl;
            break;
        case type::PrimitiveType::Double:
            std::cout << resultPrefix << *reinterpret_cast<double*>(valuePtr) << std::endl;
            break;
    }
}

void runInteractive() {
    const std::__cxx11::string historyFilename = getHistoryFilePath();

    linenoiseHistoryLoad(historyFilename.c_str());

    //linenoiseSetCompletionCallback(completionHook);

    std::shared_ptr<execute::ExecutionContext> executionContext = execute::createExecutionContext();

    executionContext->setResultCallback(resultCallback);


    const char* NUDGE = "Type '/help' for help or '/exit' to exit.";
    std::cout << "Welcome to the lwnn REPL. " << NUDGE << "\n";

    bool keepGoing = true;
    bool shouldCompile = true;
    int commandCount = 0;

    while (keepGoing) {
        std::string lineOfCode{readLineOfCode()};
        linenoiseHistoryAdd(lineOfCode.c_str());
        if(lineOfCode[0] == '/') {
            std::string command = lineOfCode.substr(1);
            if (command == "compile") {
                shouldCompile = !shouldCompile;
            } else if (command == "help") {
                help();
            } else if (command == "exit") {
                keepGoing = false;
            } else if (command == "history") {
                /* Display the current history. */
                for (int index = 0;; ++index) {
                    char *hist = linenoiseHistoryLine(index);
                    if (hist == NULL) break;
                    printf("%4d: %s\n", index, hist);
                    free(hist);
                }
            } else {
                std::cerr << "Unknown meta-command \"" << command << "\". " << NUDGE << "\n";
            }
        }
        else {
            std::string moduleName = string::format("repl_line_%d", ++commandCount);
            executeLine(executionContext, lineOfCode, moduleName, shouldCompile);
        }

        linenoiseHistorySave(historyFilename.c_str());
    }
}

bool runModule(std::shared_ptr<execute::ExecutionContext> executionContext, ast::Module *lwnnModule) {
    ASSERT(executionContext);
    ASSERT(lwnnModule);

//    executionContext->setPrettyPrintAst(true);
//    executionContext->setDumpIROnLoad(true);

    try {
        executionContext->prepareModule(lwnnModule);
    } catch (execute::ExecutionException &e) {
        return true; //Don't try to compile a module that doesn't even pass semantics checks.
    }
    executionContext->executeModule(lwnnModule);
    return false;
}

void executeLine(std::shared_ptr<execute::ExecutionContext> executionContext, std::string lineOfCode, std::string moduleName,
                 bool shouldExecute) {
    lwnn::ast::Module *module = lwnn::front::parseModule(lineOfCode, moduleName);

    //If no Module returned, parsing failed.
    if(!module) {
        return;
    }

    if(shouldExecute) {
        runModule(executionContext, module);
    }
}

bool executeScript(const std::string &startScriptFilename) {
    lwnn::ast::Module *module;
    try {
        module = lwnn::front::parseModule(startScriptFilename);

        //If no Module returned, parsing failed.
        if(!module) {
            return true;
        }
    } catch(std::runtime_error &err) {
        std::cerr << err.what();
        return true;
    }

    std::shared_ptr<execute::ExecutionContext> executionContext = execute::createExecutionContext();
    executionContext->setResultCallback(resultCallback);
    return runModule(executionContext, module);
}

} //namespace lwnn

volatile int destructionCount = 0;
class SomeGarbage : public gc_cleanup
{
public:
    ~SomeGarbage() {
        destructionCount++;
    }
    int randomWahteverIdoncare;

    void foo() { randomWahteverIdoncare++; }
};


void generateSomeGarbage() {

    for(int i = 0; i < 100000; ++i) {
        SomeGarbage *garbage = new SomeGarbage();
        garbage->foo();
    }
}

int main(int argc, char **argv) {
    GC_set_all_interior_pointers(1);
    //GC_enable_incremental();
    GC_INIT();

    generateSomeGarbage();
    while (GC_collect_a_little());

    //I'm just gonna leave this here for a while until we're confident that libgc is in fact working reliably.
    std::cout << "destructionCount: " << destructionCount << std::endl;
    if (destructionCount == 0) {
        std::cout << "**************************************************************************\n";
        std::cout << "WARNING: the libgc appears doesn't appear to be collecting anything.\n";
        std::cout << "**************************************************************************\n";
    }

    //lwnn::backtrace::initBacktraceDumper();
    linenoiseInstallWindowChangeHandler();

    char *startScriptFilename = nullptr;

    while (argc > 1) {
        argc--;
        argv++;
        if (!strcmp(*argv, "--keycodes")) {
            linenoisePrintKeyCodes();
            exit(0);
        } else {
            startScriptFilename = *argv;
        }
    }

    if(startScriptFilename) {
        if(lwnn::executeScript(startScriptFilename)) {
            return -1;
        }
    } else {
        if (!isatty(fileno(stdin))) {
            std::cout << "stdin is not a terminal\n";
            return -1;
        }
        lwnn::runInteractive();
    }

    linenoiseHistoryFree();
    return 0;
}
