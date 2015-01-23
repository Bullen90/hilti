
#include "cg-operator-common.h"
#include <binpac/autogen/operators/enum.h>

using namespace binpac;
using namespace binpac::codegen;

void CodeBuilder::visit(expression::operator_::enum_::Equal* i)
{
    auto result = builder()->addTmp("equal", hilti::builder::boolean::type());
    auto op1 = cg()->hiltiExpression(i->op1());
    auto op2 = cg()->hiltiExpression(i->op2());
    cg()->builder()->addInstruction(result, hilti::instruction::operator_::Equal, op1, op2);
    setResult(result);
}

void CodeBuilder::visit(constant::Enum* e)
{
    assert(e->type()->id());

    auto expr =  e->firstParent<Expression>();
    assert(expr);

    ID::component_list path = { expr->scope(), e->value()->name() };

    if ( expr->scope().find("::") == std::string::npos ) {
        // TODO: This is a bit hackish to get the right result depending on
        // whether the scope is already qualified with a module name or not.
        // We'll eventually need to clean up how scopes/modules are
        // associated with IDs/expressions.
        auto module = e->firstParent<Module>();
        assert(module);
        path.push_front(module->id()->name());
    }

    auto fq = std::make_shared<ID>(path, e->location());
    auto result = hilti::builder::id::create(cg()->hiltiID(fq));
    setResult(result);
}

void CodeBuilder::visit(expression::operator_::enum_::Call* i)
{
    auto type = ast::checkedCast<type::TypeType>(i->op1()->type())->typeType();
    auto etype = ast::checkedCast<type::Enum>(type);
    auto value = cg()->hiltiExpression(callParameter(i->op2(), 0));

    auto result = cg()->builder()->addTmp("enum", cg()->hiltiType(etype));
    cg()->builder()->addInstruction(result, hilti::instruction::enum_::FromInt, value);

    setResult(result);
}

void CodeBuilder::visit(expression::operator_::enum_::CastInteger* i)
{
    auto itype = ast::checkedCast<type::TypeType>(i->op2()->type())->typeType();
    auto result = builder()->addTmp("i", cg()->hiltiType(itype));
    auto op1 = cg()->hiltiExpression(i->op1());
    cg()->builder()->addInstruction(result, hilti::instruction::enum_::ToInt, op1);
    setResult(result);
}

void CodeBuilder::visit(expression::operator_::enum_::CoerceBool* i)
{
    auto result = builder()->addTmp("known", hilti::builder::boolean::type());
    auto op1 = cg()->hiltiExpression(i->op1());
    auto undef = hilti::builder::enum_::create(hilti::builder::id::node("Undef"), cg()->hiltiType(i->op1()->type()));
    cg()->builder()->addInstruction(result, hilti::instruction::operator_::Unequal, op1, undef);
    setResult(result);
}

