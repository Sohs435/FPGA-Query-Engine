#pragma once

#include <memory> 
#include <variant> 

#include "predicate.hpp"

namespace fqe {

    struct PredicateExpression; 

    using PredicateExpressionPtr = std::unique_ptr<PredicateExpression>; 

    struct AndExpression {
        PredicateExpressionPtr left;
        PredicateExpressionPtr right; 
    };

    struct OrExpression{
        PredicateExpressionPtr left;
        PredicateExpressionPtr right; 
    };

    struct NotExpression {
        PredicateExpressionPtr child;
    };

    using PredicateNode = std::variant<ComparisonPredicate, AndExpression,
     OrExpression, NotExpression>; 

    struct PredicateExpression {
        PredicateNode node; 

        explicit PredicateExpression(PredicateNode Node);
    };

    PredicateExpressionPtr make_comparison (ComparisonPredicate predicate);

    PredicateExpressionPtr make_and (PredicateExpressionPtr left, 
     PredicateExpressionPtr right);
    
    PredicateExpressionPtr make_or (PredicateExpressionPtr left, 
     PredicateExpressionPtr right);
    
    PredicateExpressionPtr make_not (PredicateExpressionPtr child);

}