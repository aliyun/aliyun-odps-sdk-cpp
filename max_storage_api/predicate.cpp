#include "include/max_storage_api.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace apsara {
namespace odps {
namespace sdk {
namespace max_storage_api {

// 谓词实现

const IPredicatePtr IPredicate::NO_PREDICATE = IRawPredicate::Of("");

class PredicateImpl : public IPredicate
{
public:
    PredicateImpl(PredicateType type) : IPredicate(type) {}

    PredicateType GetType() const override {
        return mType;
    }

    std::string ToString() const override {
        return "";
    }
};

class AttributeImpl : public IAttribute
{
public:
    AttributeImpl(const std::string& value) : mValue(value) {}

    std::string GetValue() const override {
        return mValue;
    }

    std::string ToString() const override {
        const std::string BACK_TICK = "`";
        const std::string ESCAPE_BACK_TICK = "``";

        if (mValue.length() > 1 &&
            mValue.substr(0, 1) == BACK_TICK &&
            mValue.substr(mValue.length() - 1, 1) == BACK_TICK) {
            // 已经正确加引号
            return mValue;
        } else {
            // 转义反引号并添加引号
            std::string escaped = mValue;
            size_t pos = 0;
            while ((pos = escaped.find(BACK_TICK, pos)) != std::string::npos) {
                escaped.replace(pos, 1, ESCAPE_BACK_TICK);
                pos += ESCAPE_BACK_TICK.length();
            }
            return BACK_TICK + escaped + BACK_TICK;
        }
    }

private:
    std::string mValue;
};

class ConstantImpl : public IConstant
{
public:
    ConstantImpl(const std::string& value) : mValue(value) {}

    std::string GetValue() const override {
        return mValue;
    }

    std::string ToString() const override {
        return mValue;
    }

private:
    std::string mValue;
};

class RawPredicateImpl : public IRawPredicate
{
public:
    RawPredicateImpl(const std::string& rawExpr) : mRawExpr(rawExpr) {}

    std::string GetRawExpr() const override {
        return mRawExpr;
    }

    std::string ToString() const override {
        return mRawExpr;
    }

private:
    std::string mRawExpr;
};

class BinaryPredicateImpl : public IBinaryPredicate
{
public:
    BinaryPredicateImpl(Operator op, const std::string& leftOperand, const std::string& rightOperand)
        : mOperator(op), mLeftOperand(leftOperand), mRightOperand(rightOperand) {}

    Operator GetOperator() const override {
        return mOperator;
    }

    std::string GetLeftOperand() const override {
        return mLeftOperand;
    }

    std::string GetRightOperand() const override {
        return mRightOperand;
    }

    std::string ToString() const override {
        std::string opStr;
        switch (mOperator) {
            case Operator::EQUALS:
                opStr = "=";
                break;
            case Operator::NOT_EQUALS:
                opStr = "!=";
                break;
            case Operator::GREATER_THAN:
                opStr = ">";
                break;
            case Operator::LESS_THAN:
                opStr = "<";
                break;
            case Operator::GREATER_THAN_OR_EQUAL:
                opStr = ">=";
                break;
            case Operator::LESS_THAN_OR_EQUAL:
                opStr = "<=";
                break;
        }
        return mLeftOperand + " " + opStr + " " + mRightOperand;
    }

private:
    Operator mOperator;
    std::string mLeftOperand;
    std::string mRightOperand;
};

class UnaryPredicateImpl : public IUnaryPredicate
{
public:
    UnaryPredicateImpl(Operator op, const std::string& operand)
        : mOperator(op), mOperand(operand) {}

    Operator GetOperator() const override {
        return mOperator;
    }

    std::string GetOperand() const override {
        return mOperand;
    }

    std::string ToString() const override {
        std::string opStr;
        switch (mOperator) {
            case Operator::IS_NULL:
                opStr = "is null";
                break;
            case Operator::NOT_NULL:
                opStr = "is not null";
                break;
        }
        return mOperand + " " + opStr;
    }

private:
    Operator mOperator;
    std::string mOperand;
};

class CompoundPredicateImpl : public ICompoundPredicate
{
public:
    CompoundPredicateImpl(Operator op) : mOperator(op) {}

    CompoundPredicateImpl(Operator op, const std::vector<IPredicatePtr>& predicates)
        : mOperator(op), mPredicates(predicates) {
        if (mOperator == Operator::NOT && predicates.size() > 1) {
            throw std::invalid_argument("NOT 操作符应该只有一个操作数");
        }
    }

    Operator GetOperator() const override {
        return mOperator;
    }

    const std::vector<IPredicatePtr>& GetPredicates() const override {
        return mPredicates;
    }

    void AddPredicate(const IPredicatePtr& predicate) override {
        mPredicates.push_back(predicate);
    }

    std::string ToString() const override {
        if (mPredicates.empty()) {
            return IPredicate::NO_PREDICATE->ToString();
        }

        std::string opStr;
        switch (mOperator) {
            case Operator::AND:
                opStr = "and";
                break;
            case Operator::OR:
                opStr = "or";
                break;
            case Operator::NOT:
                opStr = "not";
                break;
        }

        // 对于 NOT 操作符，我们确保只有一个操作数
        if (mOperator == Operator::NOT) {
            IPredicatePtr predicate = mPredicates[0];
            if (!predicate || predicate->ToString().empty()) {
                return IPredicate::NO_PREDICATE->ToString();
            }

            std::stringstream sb;
            sb << opStr << " ";
            sb << "(" << predicate->ToString() << ")";
            return sb.str();
        }

        std::stringstream sb;
        bool first = true;
        for (const auto& predicate : mPredicates) {
            if (!predicate || predicate->ToString().empty()) {
                if (mOperator == Operator::OR) {
                    // A or true = true
                    // 对于 OR 谓词，如果有任意谓词为 true，则结果为 true
                    return IPredicate::NO_PREDICATE->ToString();
                } else {
                    // A and true = A
                    // 对于 AND 谓词，跳过为 true 的谓词
                    continue;
                }
            }

            if (!first) {
                sb << " " << opStr << " ";
            }

            sb << "(" << predicate->ToString() << ")";

            first = false;
        }

        return sb.str();
    }

private:
    Operator mOperator;
    std::vector<IPredicatePtr> mPredicates;
};

class InPredicateImpl : public IInPredicate
{
public:
    InPredicateImpl(Operator op, const std::string& operand, const std::vector<std::string>& set)
        : mOperator(op), mOperand(operand), mSet(set) {}

    Operator GetOperator() const override {
        return mOperator;
    }

    std::string GetOperand() const override {
        return mOperand;
    }

    const std::vector<std::string>& GetSet() const override {
        return mSet;
    }

    std::string ToString() const override {
        std::string opStr;
        switch (mOperator) {
            case Operator::IN:
                opStr = "in";
                break;
            case Operator::NOT_IN:
                opStr = "not in";
                break;
        }

        std::stringstream sb;
        sb << mOperand << " " << opStr << " (";
        for (size_t i = 0; i < mSet.size(); i++) {
            sb << mSet[i];
            if (i < mSet.size() - 1) {
                sb << ", ";
            }
        }
        sb << ")";
        return sb.str();
    }

private:
    Operator mOperator;
    std::string mOperand;
    std::vector<std::string> mSet;
};

// 静态方法实现

std::shared_ptr<IAttribute> IAttribute::Of(const std::string& value) {
    return std::make_shared<AttributeImpl>(value);
}

std::shared_ptr<IConstant> IConstant::Of(const std::string& value) {
    return std::make_shared<ConstantImpl>(value);
}

std::shared_ptr<IRawPredicate> IRawPredicate::Of(const std::string& rawExpr) {
    return std::make_shared<RawPredicateImpl>(rawExpr);
}

std::shared_ptr<IBinaryPredicate> IBinaryPredicate::Equals(const std::string& leftOperand, const std::string& rightOperand) {
    return std::make_shared<BinaryPredicateImpl>(Operator::EQUALS, leftOperand, rightOperand);
}

std::shared_ptr<IBinaryPredicate> IBinaryPredicate::NotEquals(const std::string& leftOperand, const std::string& rightOperand) {
    return std::make_shared<BinaryPredicateImpl>(Operator::NOT_EQUALS, leftOperand, rightOperand);
}

std::shared_ptr<IBinaryPredicate> IBinaryPredicate::GreaterThan(const std::string& leftOperand, const std::string& rightOperand) {
    return std::make_shared<BinaryPredicateImpl>(Operator::GREATER_THAN, leftOperand, rightOperand);
}

std::shared_ptr<IBinaryPredicate> IBinaryPredicate::LessThan(const std::string& leftOperand, const std::string& rightOperand) {
    return std::make_shared<BinaryPredicateImpl>(Operator::LESS_THAN, leftOperand, rightOperand);
}

std::shared_ptr<IBinaryPredicate> IBinaryPredicate::GreaterThanOrEqual(const std::string& leftOperand, const std::string& rightOperand) {
    return std::make_shared<BinaryPredicateImpl>(Operator::GREATER_THAN_OR_EQUAL, leftOperand, rightOperand);
}

std::shared_ptr<IBinaryPredicate> IBinaryPredicate::LessThanOrEqual(const std::string& leftOperand, const std::string& rightOperand) {
    return std::make_shared<BinaryPredicateImpl>(Operator::LESS_THAN_OR_EQUAL, leftOperand, rightOperand);
}

std::shared_ptr<IUnaryPredicate> IUnaryPredicate::IsNull(const std::string& operand) {
    return std::make_shared<UnaryPredicateImpl>(Operator::IS_NULL, operand);
}

std::shared_ptr<IUnaryPredicate> IUnaryPredicate::NotNull(const std::string& operand) {
    return std::make_shared<UnaryPredicateImpl>(Operator::NOT_NULL, operand);
}

std::shared_ptr<ICompoundPredicate> ICompoundPredicate::And(const std::vector<IPredicatePtr>& predicates) {
    return std::make_shared<CompoundPredicateImpl>(Operator::AND, predicates);
}

std::shared_ptr<ICompoundPredicate> ICompoundPredicate::Or(const std::vector<IPredicatePtr>& predicates) {
    return std::make_shared<CompoundPredicateImpl>(Operator::OR, predicates);
}

std::shared_ptr<ICompoundPredicate> ICompoundPredicate::Not(const IPredicatePtr& predicate) {
    std::vector<IPredicatePtr> predicates = {predicate};
    return std::make_shared<CompoundPredicateImpl>(Operator::NOT, predicates);
}

std::shared_ptr<IInPredicate> IInPredicate::In(const std::string& operand, const std::vector<std::string>& set) {
    return std::make_shared<InPredicateImpl>(Operator::IN, operand, set);
}

std::shared_ptr<IInPredicate> IInPredicate::NotIn(const std::string& operand, const std::vector<std::string>& set) {
    return std::make_shared<InPredicateImpl>(Operator::NOT_IN, operand, set);
}

}  // namespace max_storage_api
}  // namespace sdk
}  // namespace odps
}  // namespace apsara