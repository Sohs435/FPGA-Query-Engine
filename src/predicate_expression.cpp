#include "fqe/predicate_expression.hpp"

#include <stdexcept>
#include <utility>

namespace fqe {

    PredicateExpression::PredicateExpression (PredicateNode node): node(std::move(node)) {}

    PredicateExpressionPtr make_comparison (ComparisonPredicate predicate) {

        return std::make_unique<PredicateExpression>(PredicateNode{std::move(predicate)});
    }

    PredicateExpressionPtr make_and (PredicateExpressionPtr left,
        PredicateExpressionPtr right) {

        if (left == nullptr || right == nullptr){
            throw std::invalid_argument("AND expression requires two children");
        }

        AndExpression expression{std::move(left), std::move(right)};

        return std::make_unique<PredicateExpression>(
            PredicateNode{std::move(expression)}
        );
    }

    PredicateExpressionPtr make_or (PredicateExpressionPtr left,
        PredicateExpressionPtr right) {

        if (left == nullptr || right == nullptr){
            throw std::invalid_argument("OR expression requires two children");
        }

        OrExpression expression{std::move(left), std::move(right)};

        return std::make_unique<PredicateExpression>(PredicateNode{std::move(expression)});
    }

    PredicateExpressionPtr make_not (PredicateExpressionPtr child) {

        if (child == nullptr){
            throw std::invalid_argument("NOT expression requires one child");
        }

        NotExpression expression{std::move(child)};

        return std::make_unique<PredicateExpression>(PredicateNode{std::move(expression)});
    }

}