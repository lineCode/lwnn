/**
 * This file defines the public interface used by all parts of LWNN to convert a series of characters to an AST.
 */

#pragma once

#include "ast.h"
#include <fstream>

#include <string>

namespace lwnn { namespace front {

ast::Module *parseModule(const std::string &filename);
ast::Module *parseModule(std::istream &inputStream, const std::string &name);
ast::Module *parseModule(const std::string &lineOfCode, const std::string &inputName);

/** Thrown when the parser has determined that it is unable to continue parsing.*/
class ParseAbortedException : exception::Exception {
public:
    ParseAbortedException(const std::string &message = std::string("")) : Exception(message) {
        std::cerr << "Parse aborted!";
    }
};


}}