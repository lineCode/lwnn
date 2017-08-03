
#pragma once

#include "../include/lwnn/front/ast.h"
#include "../include/lwnn/front/error.h"

#include <deque>

namespace lwnn {
    namespace ast_passes {

        /** Runs all AST passes that are part of the compilation process, except for code generation. */
        void runAllPasses(ast::Module *module, error::ErrorStream &errorStream);
    }
}