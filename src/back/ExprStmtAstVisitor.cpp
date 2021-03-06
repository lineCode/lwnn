#include "emit.h"
#include "CompileAstVisitor.h"
#include "common/containers.h"

namespace lwnn {
    namespace back {

        class ExprStmtAstVisitor : public CompileAstVisitor {
            std::stack<llvm::Value*> valueStack_;

        public:
            ExprStmtAstVisitor(CompileContext &cc) : CompileAstVisitor(cc) { }

            bool hasValue() {
                return valueStack_.size() > 0;
            }

            llvm::Value *llvmValue() {
                ASSERT(valueStack_.size() == 1);
                return valueStack_.top();
            }

            virtual void visitedFuncCallExpr(ast::FuncCallExpr *expr) override {
                //Pop arguments off first since they are evaulated before the function ptr.
                std::vector<llvm::Value*> args;
                for(size_t i = 0; i < expr->argCount(); ++i) {
                    args.push_back(valueStack_.top());
                    valueStack_.pop();
                }
                //Arguments are popped off in reverse order.
                std::reverse(args.begin(), args.end());

                //Pop the function pointer.
                llvm::Value *value = valueStack_.top();
                valueStack_.pop();

                llvm::Function *llvmFunc = llvm::cast<llvm::Function>(value);


                llvm::CallInst *result = cc().irBuilder().CreateCall(llvmFunc, args);

                valueStack_.push(result);
            }


            virtual void visitedCastExpr(ast::CastExpr *expr) {
                llvm::Value *value = valueStack_.top();
                valueStack_.pop();

                ASSERT(expr->type()->isPrimitive());
                ASSERT(expr->valueExpr()->type()->isPrimitive());

                llvm::Value *castedValue = nullptr;
                llvm::Type *destLlvmType = cc().typeMap().toLlvmType(expr->type());
                switch (expr->valueExpr()->type()->primitiveType()) {
                    //From int32
                    case type::PrimitiveType::Int32:
                        switch (expr->type()->primitiveType()) {
                            //To bool
                            case type::PrimitiveType::Bool:
                                castedValue = cc().irBuilder().CreateICmpNE(value, cc().getDefaultValueForType(expr->valueExpr()->type()));
                                break;
                            //To int
                            case type::PrimitiveType::Int32:
                                castedValue = value;
                                break;
                            //To float
                            case type::PrimitiveType::Float:
                                castedValue = cc().irBuilder().CreateSIToFP(value, destLlvmType);
                                break;
                            default:
                                ASSERT_FAIL("Unknown cast from int32");
                        }
                        break;
                        //From float
                    case type::PrimitiveType::Float:
                        switch (expr->type()->primitiveType()) {
                            //To to int32 or bool
                            case type::PrimitiveType::Bool:
                                castedValue = cc().irBuilder().CreateFCmpUNE(value, cc().getDefaultValueForType(expr->valueExpr()->type()));
                                break;
                            case type::PrimitiveType::Int32:
                                castedValue = cc().irBuilder().CreateFPToSI(value, destLlvmType);
                                break;
                            case type::PrimitiveType::Float:
                                castedValue = value;
                                break;
                            default:
                                ASSERT_FAIL("Unknown cast from float");
                        }
                        break;
                        //From bool
                    case type::PrimitiveType::Bool:
                        ASSERT_FAIL("cannot cast from bool to anything");
                        break;
                    default:
                        ASSERT_FAIL("Unhandled type::PrimitiveType")
                }
                ASSERT(castedValue && "Cannot fail to create casted value...");
                valueStack_.push(castedValue);
            }

            virtual void visitedVariableDeclExpr(ast::VariableDeclExpr *expr) override {
                switch(expr->symbol()->storageKind()) {
                    case scope::StorageKind::Global: {
                        llvm::GlobalVariable *globalVariable = cc().llvmModule().getNamedGlobal(expr->name());
                        ASSERT(globalVariable);
                        globalVariable->setAlignment(ALIGNMENT);
                        if (expr->type()->isPrimitive()) {
                            globalVariable->setInitializer(cc().getDefaultValueForType(expr->type()));
                        }
                        break;
                    }
                    case scope::StorageKind::Local: {
                        llvm::Type *localVariableType = cc().typeMap().toLlvmType(expr->type());
                        llvm::Value *localVariable = cc().irBuilder().CreateAlloca(localVariableType, nullptr, expr->name());
                        cc().mapSymbolToValue(expr->symbol(), localVariable);
                        break;
                    }
                    default:
                        ASSERT_FAIL("Unhandled StorageKind");
                }
                visitVariableRefExpr(expr);
            }

            virtual void visitVariableRefExpr(ast::VariableRefExpr *expr) override {
                llvm::Value *pointer = cc().getMappedValue(expr->symbol());

                ASSERT(pointer);

                if (expr->variableAccess() == ast::VariableAccess::Write) {
                    valueStack_.push(pointer);
                    return;
                }

                if(expr->type()->isPrimitive()) {
                    llvm::LoadInst *loadInst = cc().irBuilder().CreateLoad(pointer);
                    loadInst->setAlignment(ALIGNMENT);
                    valueStack_.push(loadInst);
                } else {
                    valueStack_.push(pointer);
                }
            }

            virtual void visitLiteralInt32Expr(ast::LiteralInt32Expr *expr) override {
                valueStack_.push(getLiteralIntLlvmValue(expr->value()));
            }

            llvm::Value *getLiteralIntLlvmValue(int value) {
                return llvm::ConstantInt::get(cc().llvmContext(), llvm::APInt(32, (uint64_t) value, true));
            }

            llvm::Value *getLiteralUIntLLvmValue(unsigned value) {
                return llvm::ConstantInt::get(cc().llvmContext(), llvm::APInt(32, (uint64_t) value, false));
            }

            virtual void visitLiteralFloatExpr(ast::LiteralFloatExpr *expr) override {
                valueStack_.push(llvm::ConstantFP::get(cc().llvmContext(), llvm::APFloat(expr->value())));
            }

            virtual void visitLiteralBoolExpr(ast::LiteralBoolExpr *expr) {
                valueStack_.push(llvm::ConstantInt::get(cc().llvmContext(), llvm::APInt(1, (uint64_t) expr->value(), true)));
            }

            virtual bool visitingBinaryExpr(ast::BinaryExpr *expr) override {
                if(expr->binaryExprKind() != ast::BinaryExprKind::Logical) {
                    return true;
                }

                //Prepare the basic blocks
                llvm::BasicBlock *lValueBlock = cc().irBuilder().GetInsertBlock();
                llvm::Function *currentFunc = lValueBlock->getParent();
                llvm::BasicBlock *rValueBlock = llvm::BasicBlock::Create(cc().llvmContext(), "rValueBlock");
                llvm::BasicBlock *endBlock = llvm::BasicBlock::Create(cc().llvmContext(), "endBlock");

                //Emit the lValue, which is always the first operand of a logical operation to be evaluated
                llvm::Value *lValue = emitExpr(expr->lValue(), cc());

                bool lValueConst;
                switch(expr->operation()) {
                    //For the && operator, only evaluate the lValue if the rValue is true
                    case ast::BinaryOperationKind::LogicalAnd:
                        cc().irBuilder().CreateCondBr(lValue, rValueBlock, endBlock);
                        lValueConst = false;
                        break;
                    case ast::BinaryOperationKind::LogicalOr:
                        cc().irBuilder().CreateCondBr(lValue, endBlock, rValueBlock);
                        lValueConst = true;
                        break;
                    default:
                        ASSERT_FAIL("Unhandled BinaryOperationKind (supposed to be a logical operator)")
                }
                currentFunc->getBasicBlockList().push_back(rValueBlock);
                cc().irBuilder().SetInsertPoint(rValueBlock);
                llvm::Value *rValue = emitExpr(expr->rValue(), cc());
                cc().irBuilder().CreateBr(endBlock);

                currentFunc->getBasicBlockList().push_back(endBlock);
                cc().irBuilder().SetInsertPoint(endBlock);
                llvm::PHINode *phi = cc().irBuilder().CreatePHI(cc().typeMap().toLlvmType(&type::Primitives::Bool), 2, "logical_tmp");

                llvm::ConstantInt *shortCircuitLlvmValue
                    = llvm::ConstantInt::get(cc().llvmContext(), llvm::APInt(1, (uint64_t) lValueConst, false));
                phi->addIncoming(shortCircuitLlvmValue, lValueBlock);
                phi->addIncoming(rValue, rValueBlock);

                valueStack_.push(phi);
                return false;
            }

            virtual void visitedBinaryExpr(ast::BinaryExpr *expr) override {
                if(expr->binaryExprKind() != ast::BinaryExprKind::Arithmetic) {
                    return;
                }

                ASSERT(expr->lValue()->type()->isSameType(expr->rValue()->type()) && "data types must match");

                llvm::Value *rValue = valueStack_.top();
                valueStack_.pop();

                llvm::Value *lValue = valueStack_.top();
                valueStack_.pop();

                ASSERT(expr->type()->isPrimitive() && "Only primitive types currently supported here");

                if (expr->operation() == ast::BinaryOperationKind::Assign) {
                    cc().irBuilder().CreateStore(rValue, lValue); //(args swapped on purpose)
                    //Note:  I think this will call rValue a second time if rValue is a call site.
                    valueStack_.push(rValue);
                    return;
                }
                llvm::Value *resultValue = nullptr;
                switch (expr->operandsType()->primitiveType()) {
                    case type::PrimitiveType::Bool:
                        switch (expr->operation()) {
                            case ast::BinaryOperationKind::Eq:
                                resultValue = cc().irBuilder().CreateICmpEQ(lValue, rValue);
                                break;
                            case ast::BinaryOperationKind::NotEq:
                                resultValue = cc().irBuilder().CreateICmpNE(lValue, rValue);
                                break;
                            default:
                                ASSERT_FAIL("Unsupported BinaryOperationKind for bool primitive type")
                        }
                        break;
                    case type::PrimitiveType::Int32:
                        switch (expr->operation()) {
                            case ast::BinaryOperationKind::Eq:
                                resultValue = cc().irBuilder().CreateICmpEQ(lValue, rValue);
                                break;
                            case ast::BinaryOperationKind::NotEq:
                                resultValue = cc().irBuilder().CreateICmpNE(lValue, rValue);
                                break;
                            case ast::BinaryOperationKind::Add:
                                resultValue = cc().irBuilder().CreateAdd(lValue, rValue);
                                break;
                            case ast::BinaryOperationKind::Sub:
                                resultValue = cc().irBuilder().CreateSub(lValue, rValue);
                                break;
                            case ast::BinaryOperationKind::Mul:
                                resultValue = cc().irBuilder().CreateMul(lValue, rValue);
                                break;
                            case ast::BinaryOperationKind::Div:
                                resultValue = cc().irBuilder().CreateSDiv(lValue, rValue);
                                break;
                            case ast::BinaryOperationKind::GreaterThan:
                                resultValue = cc().irBuilder().CreateICmpSGT(lValue, rValue);
                                break;
                            case ast::BinaryOperationKind::GreaterThanOrEqual:
                                resultValue = cc().irBuilder().CreateICmpSGE(lValue, rValue);
                                break;
                            case ast::BinaryOperationKind::LessThan:
                                resultValue = cc().irBuilder().CreateICmpSLT(lValue, rValue);
                                break;
                            case ast::BinaryOperationKind::LessThanOrEqual:
                                resultValue = cc().irBuilder().CreateICmpSLE(lValue, rValue);
                                break;
                            default:
                                ASSERT_FAIL("Unhandled BinaryOperationKind (supposed to be an arithmetic operator)");
                        }
                        break;
                    case type::PrimitiveType::Float:
                        switch (expr->operation()) {
                            case ast::BinaryOperationKind::Eq:
                                resultValue = cc().irBuilder().CreateFCmpOEQ(lValue, rValue);
                                break;
                            case ast::BinaryOperationKind::NotEq:
                                resultValue = cc().irBuilder().CreateFCmpONE(lValue, rValue);
                                break;
                            case ast::BinaryOperationKind::Add:
                                resultValue = cc().irBuilder().CreateFAdd(lValue, rValue);
                                break;
                            case ast::BinaryOperationKind::Sub:
                                resultValue = cc().irBuilder().CreateFSub(lValue, rValue);
                                break;
                            case ast::BinaryOperationKind::Mul:
                                resultValue = cc().irBuilder().CreateFMul(lValue, rValue);
                                break;
                            case ast::BinaryOperationKind::Div:
                                resultValue = cc().irBuilder().CreateFDiv(lValue, rValue);
                                break;
                            case ast::BinaryOperationKind::GreaterThan:
                                resultValue = cc().irBuilder().CreateFCmpOGT(lValue, rValue);
                                break;
                            case ast::BinaryOperationKind::GreaterThanOrEqual:
                                resultValue = cc().irBuilder().CreateFCmpOGE(lValue, rValue);
                                break;
                            case ast::BinaryOperationKind::LessThan:
                                resultValue = cc().irBuilder().CreateFCmpOLT(lValue, rValue);
                                break;
                            case ast::BinaryOperationKind::LessThanOrEqual:
                                resultValue = cc().irBuilder().CreateFCmpOLE(lValue, rValue);
                                break;
                            default:
                                ASSERT_FAIL("Unhandled BinaryOperationKind (supposed to be an arithmetic operator)");
                        }
                        break;
                    default:
                        ASSERT_FAIL("Unhandled PrimitiveType");
                }
                ASSERT(resultValue);
                valueStack_.push(resultValue);
            }

            virtual void visitedDotExpr(ast::DotExpr *expr) override {
                llvm::Value *lvalue = valueStack_.top();
                valueStack_.pop();
                auto classType = dynamic_cast<const type::ClassType*>(expr->lValue()->type()->actualType());
                ASSERT(classType != nullptr && " TODO: add semantics check to ensure that lvalues of dot operators are of types that have fields.");
                type::ClassField *classField = classType->findField(expr->memberName());
                unsigned ordinal = classField->ordinal();

                //First emit code to calculate the address of the member field
                llvm::Value *ptrOrValue = cc().irBuilder().CreateStructGEP(nullptr, lvalue, ordinal, classField->name());

                // The result of the DotExpr is a pointer if field is not being assigned to because the store instruction
                // expects a pointer at which to store the value.
                // We also do not attempt to load "values" of any classes because so far we lwnn only operates on class pointers or
                // their fields, not entire classes.
                if(expr->isWrite() || expr->type()->isClass()) {
                    valueStack_.push(ptrOrValue);
                    return;
                }

                ptrOrValue = cc().irBuilder().CreateLoad(ptrOrValue);
                valueStack_.push(ptrOrValue);
                return;
            }

            virtual bool visitingIfExpr(ast::IfExprStmt *ifExpr) override {
                //This function is modeled after: https://llvm.org/docs/tutorial/LangImpl08.html (ctrl-f for "IfExprAST::codegen")
                //Emit the condition
                llvm::Value *condValue = emitExpr(ifExpr->condition(), cc());

                //Prepare the BasicBlocks
                llvm::Function *currentFunc = cc().irBuilder().GetInsertBlock()->getParent();
                llvm::BasicBlock *thenBlock = llvm::BasicBlock::Create(cc().llvmContext(), "thenBlock");
                llvm::BasicBlock *elseBlock = llvm::BasicBlock::Create(cc().llvmContext(), "elseBlock");
                llvm::BasicBlock *endBlock = llvm::BasicBlock::Create(cc().llvmContext(), "endBlock");

                //Branch to then or else blocks, depending on condition.
                cc().irBuilder().CreateCondBr(condValue, thenBlock, elseBlock);

                //Emit the thenBlock
                currentFunc->getBasicBlockList().push_back(thenBlock);
                cc().irBuilder().SetInsertPoint(thenBlock);
                llvm::Value *thenValue = emitExpr(ifExpr->thenExpr(), cc());
                thenBlock = cc().irBuilder().GetInsertBlock();

                //Jump to the endBlock, skipping the elseBlock
                cc().irBuilder().CreateBr(endBlock);

                //Emit the elseBlock
                currentFunc->getBasicBlockList().push_back(elseBlock);
                cc().irBuilder().SetInsertPoint(elseBlock);

                llvm::Value *elseValue = nullptr;
                if (ifExpr->elseExpr()) {
                    elseValue = emitExpr(ifExpr->elseExpr(), cc());
                }
                cc().irBuilder().CreateBr(endBlock);
                elseBlock = cc().irBuilder().GetInsertBlock();

                //Emit the endBlock
                currentFunc->getBasicBlockList().push_back(endBlock);
                cc().irBuilder().SetInsertPoint(endBlock);
                if (ifExpr->type()->primitiveType() != type::PrimitiveType::Void) {
                    llvm::PHINode *phi = cc().irBuilder().CreatePHI(cc().typeMap().toLlvmType(ifExpr->type()),
                                                                    ifExpr->elseExpr() ? 2 : 1, "iftmp");
                    phi->addIncoming(thenValue, thenBlock);
                    if (ifExpr->elseExpr()) {
                        phi->addIncoming(elseValue, elseBlock);
                    }
                    valueStack_.push(phi);
                }
                return false;
            }

            virtual bool visitingWhileExpr(ast::WhileExpr *whileExpr) {

                //Prepare the BasicBlocks
                llvm::Function *currentFunc = cc().irBuilder().GetInsertBlock()->getParent();
                llvm::BasicBlock *conditionBlock = llvm::BasicBlock::Create(cc().llvmContext(), "whileCondition");
                llvm::BasicBlock *bodyBlock = llvm::BasicBlock::Create(cc().llvmContext(), "whileBody");
                llvm::BasicBlock *endBlock = llvm::BasicBlock::Create(cc().llvmContext(), "whileEnd");

                cc().irBuilder().CreateBr(conditionBlock);
                currentFunc->getBasicBlockList().push_back(conditionBlock);
                cc().irBuilder().SetInsertPoint(conditionBlock);

                //Emit the condition
                llvm::Value *condValue = emitExpr(whileExpr->condition(), cc());

                //If the condition is true branch to the body block, otherwise branch to the end block
                cc().irBuilder().CreateCondBr(condValue, bodyBlock, endBlock);

                //Emit the body block
                currentFunc->getBasicBlockList().push_back(bodyBlock);
                cc().irBuilder().SetInsertPoint(bodyBlock);
                emitExpr(whileExpr->body(), cc());

                //Loop back to the condition
                cc().irBuilder().CreateBr(conditionBlock);

                //Emit the end block
                currentFunc->getBasicBlockList().push_back(endBlock);
                cc().irBuilder().SetInsertPoint(endBlock);

                return false;
            }

            virtual bool visitingCompoundExpr(ast::CompoundExpr *expr) override {

                llvm::Value *lastValue = nullptr;
                for (ast::ExprStmt *expr : expr->expressions()) {
                    ExprStmtAstVisitor exprAstVisitor{cc()};
                    expr->accept(&exprAstVisitor);
                    if(exprAstVisitor.hasValue()) {
                        lastValue = exprAstVisitor.llvmValue();
                    }
                }
                valueStack_.push(lastValue);

                return false;
            }

            virtual bool visitingAssertExprStmt(ast::AssertExprStmt *assert) override {
                //Emit the condition
                llvm::Value *condValue = emitExpr(assert->condition(), cc());

                //Prepare the BasicBlocks
                llvm::Function *currentFunc = cc().irBuilder().GetInsertBlock()->getParent();
                llvm::BasicBlock *thenBlock = llvm::BasicBlock::Create(cc().llvmContext(), "assertFailBlock");
                llvm::BasicBlock *endBlock = llvm::BasicBlock::Create(cc().llvmContext(), "assertEndBlock");

                //Branch to then or else blocks, depending on condition.
                cc().irBuilder().CreateCondBr(condValue, endBlock, thenBlock);

                //Emit the thenBlock
                currentFunc->getBasicBlockList().push_back(thenBlock);
                cc().irBuilder().SetInsertPoint(thenBlock);

                //Call the assert_failed function
                std::vector<llvm::Value*> arguments;
                source::SourceSpan span = assert->condition()->sourceSpan();
                arguments.push_back(cc().getDeduplicatedStringConstant(span.name()));
                arguments.push_back(getLiteralUIntLLvmValue((unsigned int)span.start().line()));
                //arguments.push_back(cc().getDeduplicatedStringConstant(assert->expression()));
                cc().irBuilder().CreateCall(cc().assertFunc(), arguments);

                //Jump to the endBlock, skipping the elseBlock
                cc().irBuilder().CreateBr(endBlock);

                //Emit the endBlock
                currentFunc->getBasicBlockList().push_back(endBlock);
                cc().irBuilder().SetInsertPoint(endBlock);
                return false;
            }

        };

        llvm::Value *emitExpr(ast::ExprStmt *exprStmt, CompileContext &cc) {
            ExprStmtAstVisitor visitor{cc};
            exprStmt->accept(&visitor);

            return visitor.hasValue() ? visitor.llvmValue() : nullptr;
        }
    }
}
