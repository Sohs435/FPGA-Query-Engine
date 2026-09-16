#include "fqe/parser.hpp"

#include <stdexcept>
#include <utility>

namespace fqe {

    namespace {
        // token is a comparison token (=!=<<= and so on)
        bool is_comparison_token (TokenType type) noexcept {

            switch (type) {

                case TokenType::Equal:
                case TokenType::NotEqual:
                case TokenType::LessThan:
                case TokenType::LessEqual:
                case TokenType::GreaterThan:
                case TokenType::GreaterEqual:
                    return true; 
                
                default: 
                    return false; 
            }
        }

        //comparison token is a Sub Set of continuous scalar expression 

        //token is a scalar expresion (+-*/=!= and so on)
        bool continuous_scalar_expression (TokenType type) noexcept {

            switch (type) {

                case TokenType::Plus:
                case TokenType::Minus:
                case TokenType::Star:
                case TokenType::Slash:
                case TokenType::Equal:
                case TokenType::NotEqual:
                case TokenType::LessThan:
                case TokenType::LessEqual:
                case TokenType::GreaterThan:
                case TokenType::GreaterEqual:
                case TokenType::Between:
                case TokenType::In:
                    return true;

                default:
                    return false;
            }
        }
    }

    // Move ownership of the token storage into tokens_.
    // The source vector remains valid but has an unspecified value.
    Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)), current_(0) {

        // Query is empty or doesnt end with a semicolon
        if (tokens_.empty() || tokens_.back().type != TokenType::End){
            throw std::invalid_argument ("Parser requires a token sequence ending with End");
        }

        //; somewhere in between Query Statement and not at end 
        for (std::size_t i = 0; i < tokens_.size() - 1; i++){

            if (tokens_[i].type == TokenType::End){
                throw std::invalid_argument("End token found in middle of token sequence");
            }
        }
    }

    //current token is the end token (;)
    bool Parser::is_at_end() const noexcept {
        return peek().type == TokenType::End; 
    }

    //.peek() returns token at current index
    // it does not consume it and hence, current_ doesn't advance 
    const Token& Parser::peek() const {
        return tokens_[current_];
    }

    //can return token at previous index as long as current index is not starting
    //index (0)
    const Token& Parser::previous() const {
        if (current_ == 0) {
           throw std::invalid_argument("No previous index before starting index of 0");
        }

        return tokens_[current_ - 1];
    }

    //return token at index current_ and consume by advancing current_ if 
    //its not reached the end of tokens_ 
    const Token& Parser::advance() {

        const Token& current_token = peek();

        if (!is_at_end()){
            current_++; 
        }

        return current_token; 
    }

    //return True if token type input is the same as the token type at current index
    bool Parser::check(TokenType type) const noexcept {
        return peek().type == type; 
    }

    // Check whether the current token has the requested type.
    // If it matches, consume it and return true.
    // Otherwise, leave it untouched and return false.
    bool Parser::match(TokenType type) {
        
        if (!check(type)){
            return false;
        }

        advance();
        return true; 
    }
    
    //message is only used to produce a useful error when the expected token is
    //missing. Without it we would essentially have Unexpected token
    //when ever there is error/misordering of tokens in query (i.e something
    // like calling FROM before SELECT in a query)
    const Token& Parser::consume(TokenType type, const std::string& message){

        if (check(type)){
            return advance();
        }

        throw std::invalid_argument(message + " at position " 
            + std::to_string(peek().position) + ", received '" + peek().text + "'");
    }
    // essentially the parsing order is 
    // SELECT + ... 
    // ... -> parse_select_list()
    // FROM + ...
    // ... -> table_name = consume(...)
    // WHERE + predicate
    // predicate -> parse_predicate -> handles predicate via recursive descent parser 
    ParsedQuery Parser::parse_query() {

        consume(TokenType::Select, "Expected SELECT at the beginning of the Query");

        ParsedQuery query;

        query.select_items = parse_select_list();

        consume(TokenType::From, "Expected FROM after the SELECT list");

        const Token& table_name = consume(TokenType::Identifier, "Expected table name after From");
        
        query.table_name = table_name.text; 

        if (match(TokenType::Where)) {
            query.where_expression = parse_predicate(); //everything post where expression
        }

        // GROUP BY ... -> BY always after GROUP so they should be one after the
        // other in the token order in tokens_ 
        if (match(TokenType::Group)){
            consume(TokenType::By, "Expected BY after GROUP");

            query.group_by_columns = parse_group_by_list(); //everything post GROUP BY
        }

        match(TokenType::Semicolon);
        //semi colon has to be at end - kinda redundant since constructor already checks 
        // but like not that hard to implemnent and we still have to reach semicolon regardless
        // so its just one extra check 
        if (!is_at_end()) {
            throw std::invalid_argument("Unexpected Token '" + peek().text + 
            "' at position" + std::to_string(peek().position));
        }

        return query;

    }

    //Ex. SELECT SUM (Price), COUNT(*) FROM ...
    //Parse a select item and the current token becomes a comma
    //so everytime we have a comma after the while loop runs 
    // we still have a another item that needs to be parsed by parse_select_item()
    std::vector<SelectItem> Parser::parse_select_list() {
        std::vector<SelectItem> select_items;
        //result vector 
        select_items.push_back(parse_select_item());
        //parse select item works by parsing tokens seperated by a comma, 
        // or is the last item in the SELECT line -> every time current_
        // is on a comma we know that there is still another item to 
        // parse from the SELECT line 
        while (match(TokenType::Comma)) {
            select_items.push_back(parse_select_item());
        }

        return select_items; 
    }

    // SelectItem -> aggregate, expression, is_star, alias
    // COUNT (*) -> aggregate = Count, is_star = True, alias = "" (no as)
    // expression = nullptr
    // SUM(price) -> aggregate = Sum, is_star = False, alias = "",
    // expression-> (price)
    SelectItem Parser::parse_select_item () {

        SelectItem select_item;
        
        //Token type is an aggregate function (Sum, Count, max, min)
        if (is_aggregate_function(peek().type)) {

            select_item.aggregate = parse_aggregate_function();

            // SUM (... or COUNT(... -> aggregate function always followed
            // by left bracket 
            consume(TokenType::LeftParenthesis, "Expected ( after aggregate call"); 

            //in case of COUNT (*)
            if (select_item.aggregate.value() == AggregateFunction::Count
             && match(TokenType::Star)) {
                select_item.is_star = true; 
             }

             else {
                if (check(TokenType::Star)) {
                    throw std::invalid_argument ("Only COUNT can use * at position"
                     + std::to_string(peek().position));
                }
                // something has to be inside brackets is item is
                // not COUNT
                select_item.expression = parse_scalar_expression();
             }

             consume(TokenType::RightParenthesis, "Expected ) after aggregate argument");

        }
        
        //SELECT price, SELECT quantity, etc -> no aggregate function
        else {
            select_item.expression = parse_scalar_expression();
        }
        // SELECT SUM(price * quantity) AS total_value
        if (match(TokenType::As)) { // check if current token is AS
            const Token& alias = consume(TokenType::Identifier,
             "expected alias name after as"); // next token 
             // should be an identifier like total_value is in the
             //example

             select_item.alias = alias.text; // "total_value"
        }

        return select_item;
    }
    
    std::vector<std::string> Parser::parse_group_by_list() {

        std::vector<std::string> columns;

        const Token& first_column = consume(TokenType::Identifier,
         "Expected column name after GROUP BY"); //GROUP BY price 
         // or size or whatever else 

         columns.push_back(first_column.text);

         while (match(TokenType::Comma)) {
            //GROUP BY price, size, quantity
            // check if comma and consume if it is after which consume identifier 
            const Token& column = consume (TokenType::Identifier, 
             "Expected column name after, in GROUP BY"); // Cannot have
             // GROUP BY price, size, 
             // and just leave it like that, another identifier needs to come after
             // the comma 

             columns.push_back(column.text);
         }

         return columns;
    }

    //just checks if Token is one of sum, count, max, min 
    bool Parser::is_aggregate_function(TokenType type) const noexcept {

        switch (type){
            case TokenType::Sum:
            case TokenType::Count:
            case TokenType::Min:
            case TokenType::Max:
                return true; 
            
            default:
                return false; 
        }
    }

    AggregateFunction Parser::parse_aggregate_function() {

        TokenType type = advance().type; //advances & extracts aggregate
        //type 

        //we use AggregateFunction since it allows for easier logical decomp
        //later on -> can just use TokenType but it would make code less readable
        switch (type) {
            case TokenType::Sum:
                return AggregateFunction::Sum;

            case TokenType::Count:
                return AggregateFunction::Count;

            case TokenType::Min:
                return AggregateFunction::Min;

            case TokenType::Max:
                return AggregateFunction::Max;

            default:
                throw std::invalid_argument("Expected aggregate function");
        }

        
    }

    // Or has lowest precedence
    // The order essentially is OR -> AND -> NOT -> primary -> comparison
    // -> scalar -> additive -> multiplicative -> unary -> scalar primary
    PredicateExpressionPtr Parser::parse_predicate() {
        return parse_or_expression();
    }
    // (X OR Y) OR Z AND A -> LHS = X OR Y, RHS = Z AND A -> brackets
    // have higher precedence so they get dealt with later down the recursion
    PredicateExpressionPtr Parser::parse_or_expression() {

        PredicateExpressionPtr left = parse_and_expression();

        while (match(TokenType::Or)) {
            
            PredicateExpressionPtr right = parse_and_expression();

            left = make_or(std::move(left), std::move(right));
        }

        return left; 
    }


    PredicateExpressionPtr Parser::parse_and_expression() {

        PredicateExpressionPtr left = parse_not_expression(); //NOT follows AND 

        while (match(TokenType::And)) {

            PredicateExpressionPtr right = parse_not_expression();

            left = make_and(std::move(left), std::move(right));
        }

        return left;
    }


    PredicateExpressionPtr Parser::parse_not_expression() {

        if (match(TokenType::Not)) {
            return make_not(parse_not_expression());
        }

        return parse_predicate_primary(); //Primary follows NOT
    }

    //FED into AND, OR, NOT blocks 
    //builds predicate abstract syntax tree that will later be evaluated into bitmasks
    PredicateExpressionPtr Parser::parse_predicate_primary() {

        if (match(TokenType::True)){
            return make_boolean(true); 
        }

        if (match(TokenType::False)){
            return make_boolean(false);
        }
        
        //Enter paranthesis block 
        if (check(TokenType::LeftParenthesis) && parenthesis_starts_predicate()) {
            advance();

            PredicateExpressionPtr expression = parse_predicate();

            consume(TokenType::RightParenthesis, "Expected ) afer predicate expression"); 

            return expression; 
        }

        return parse_comparison_expression();
    }

    PredicateExpressionPtr Parser::parse_comparison_expression() {
        ScalarExpressionPtr left = parse_scalar_expression(); // x > y -> x 

        if (is_comparison_token(peek().type)) {

            ComparisonOperator comparison = parse_comparison_operator(); // x > y -> >

            ScalarExpressionPtr right = parse_scalar_expression(); // x > y -> y 

            return make_comparison(std::move(left), comparison, std::move(right));
        }

        bool negated = false;

        //check for NOT IN and NOT BETWEEN 
        // tokens_[current_] is NOT and tokens_[current_ + 1] is BETWEEN or IN
        //other instances of not are handled by parse_not_expression()
        if (check(TokenType::Not) && current_ + 1 < tokens_.size()
         && (tokens_[current_ + 1].type == TokenType::Between ||
         tokens_[current_ + 1].type == TokenType::In)) {
            
            advance();
            negated = true;
         }

        if (match(TokenType::Between)) {
            std::int64_t lower_bound = parse_integer_value();

            consume(TokenType::And, "Expected AND inside BETWEEN expression");

            std::int64_t upper_bound = parse_integer_value();

            PredicateExpressionPtr between = make_between(std::move(left), lower_bound,
             upper_bound, true, true); //between means inclusive
             // so lower and upper inclusive are both true 

            if (negated) {
                return make_not(std::move(between));
            }

            return between; 

        }

        if (match(TokenType::In)) {
            std::vector<std::int64_t> values = parse_integer_list();

            return make_in(std::move(left), std::move(values), negated);
        }

        throw std::invalid_argument ("Expected comparison operator at position "
          + std::to_string(peek().position));
    }

    //Okay so this is jus the public internal entry point for any numerical expression
    // think bout it like this:
    // if we have parse + quantity*2 -> Add will be the root of the overal
    // tree structure represented by the expression given it has lowest precedence
    ScalarExpressionPtr Parser::parse_scalar_expression() {
        return parse_additive_expression();
    }

    // A + B + C -> left = A + B
    // left = left_prev + C = (A + B) + C
    // A * B + C + D
    // A * B -> parse_multiplicative expresssion 
    // A * B = left
    // right = C
    // new_left = left + right = A * B + C, new_right = D
    // new_new_left = new_left + new_right = (A * B + C )+ D **// 
    ScalarExpressionPtr Parser::parse_additive_expression() {
        ScalarExpressionPtr left = parse_multiplicative_expression();

        while (check(TokenType::Plus) || check(TokenType::Minus)) {
            
            TokenType operation_token = advance().type;

            ScalarExpressionPtr right = parse_multiplicative_expression();

            ArithmeticOperator operation;

            if (operation_token == TokenType::Plus) {
                operation = ArithmeticOperator::Add; 
            }
            else {
                operation = ArithmeticOperator::Subtract;
            }

            left = make_arithmetic(operation, std::move(left), std::move(right));
        }

        return left; 
    }

    ScalarExpressionPtr Parser::parse_multiplicative_expression() {
        ScalarExpressionPtr left = parse_unary_expression();

        while (check(TokenType::Star) || check(TokenType::Slash)) {
            TokenType operation_token = advance().type;

            ScalarExpressionPtr right = parse_unary_expression();

            ArithmeticOperator operation;

            if (operation_token == TokenType::Star) {
                operation = ArithmeticOperator::Multiply;
            }
            else {
                operation = ArithmeticOperator::Divide;
            }

            left = make_arithmetic(operation, std::move(left), std::move(right));
        }

        return left; 
    }

    ScalarExpressionPtr Parser::parse_unary_expression() {
        // can have +- for example in front of some obj
        // so consume plus
        // then call same func recursively and enter minus branch -> 
        // return scalar expression akin to 0 - obj_val = -obj_val
        if (match(TokenType::Plus)) {
            return parse_unary_expression();
        }

        if (match(TokenType::Minus)) {
            
            ScalarExpressionPtr operand = parse_unary_expression(); //cosume minus
            // and enter parse_scalar_primary on next recursion 

            // return 0 - value = -value
            return make_arithmetic(ArithmeticOperator::Subtract, make_integer_literal(0),
            std::move(operand));

        }

        return parse_scalar_primary();


    }

    ScalarExpressionPtr Parser::parse_scalar_primary() {

        if (match(TokenType::Integer)) {

            const Token& integer = previous(); //match consumes integer so we need to look
            //1 index backward in tokens_ 
    

            if (!integer.integer_value.has_value()) { //integer has no value 
                throw std::invalid_argument("Integer token has no value at position " +
                    std::to_string(integer.position));
            }

            return make_integer_literal(integer.integer_value.value()); //return integer value
            // in 64bit form 
        }

        if (match(TokenType::Identifier)) {
            return make_column_reference(previous().text);
        }//figure out header 

        // (price + quantity) * 5
        if (match(TokenType::LeftParenthesis)) { // consume ( and current_ points to price

            ScalarExpressionPtr expression = parse_scalar_expression();//parse everything inside bracket

            consume(TokenType::RightParenthesis, "Expected ) after scalar expression"); //consume )

            return expression; //return completed inner expression -> for the above an add subtree shld be returned
        }

        throw std::invalid_argument("Expected column, integer or ( at position " +
            std::to_string(peek().position));
    }
    
    ComparisonOperator Parser::parse_comparison_operator() {

        TokenType type = advance().type;

        switch (type) {

            case TokenType::Equal:
                return ComparisonOperator::Equal;

            case TokenType::NotEqual:
                return ComparisonOperator::NotEqual;

            case TokenType::LessThan:
                return ComparisonOperator::LessThan;

            case TokenType::LessEqual:
                return ComparisonOperator::LessEqual;

            case TokenType::GreaterThan:
                return ComparisonOperator::GreaterThan;

            case TokenType::GreaterEqual:
                return ComparisonOperator::GreaterEqual;

            default:
                throw std::invalid_argument("Expected comparison operator");
        }
    }

    std::int64_t Parser::parse_integer_value() {

        bool negative = false;

        if (match(TokenType::Minus)) {
            negative = true;
        }

        else {
            match(TokenType::Plus);
        }

        const Token& integer = consume(TokenType::Integer, "Expected integer value");

        if (!integer.integer_value.has_value()) {
            throw std::invalid_argument("Integer token has no value at position " +
                std::to_string(integer.position));
        }

        std::int64_t value = integer.integer_value.value();

        return negative ? -value : value;
    }

    std::vector<std::int64_t> Parser::parse_integer_list() {

        consume(TokenType::LeftParenthesis, "Expected ( at the beginning of IN list");

        if (check(TokenType::RightParenthesis)) {
            throw std::invalid_argument("IN list cannot be empty at position " +
                std::to_string(peek().position));
        }

        std::vector<std::int64_t> values;

        values.push_back(parse_integer_value());

        while (match(TokenType::Comma)) {
            values.push_back(parse_integer_value());
        }

        consume(TokenType::RightParenthesis, "Expected ) after IN list");

        return values;
    }

    bool Parser::parenthesis_starts_predicate() const {

        if (!check(TokenType::LeftParenthesis)) {
            return false;
        }

        std::size_t depth = 0;
        std::size_t closing_position = tokens_.size();

        for (std::size_t i = current_; i < tokens_.size(); i++) {

            if (tokens_[i].type == TokenType::LeftParenthesis) {
                depth++;
            }

            else if (tokens_[i].type == TokenType::RightParenthesis) {

                depth--;

                if (depth == 0) {
                    closing_position = i;
                    break;
                }
            }

            else if (tokens_[i].type == TokenType::End) {
                break;
            }
        }

        if (closing_position == tokens_.size()) {
            throw std::invalid_argument("Unterminated ( at position " +
                std::to_string(peek().position));
        }

        if (closing_position + 1 >= tokens_.size()) {
            return true;
        }

        TokenType following_type = tokens_[closing_position + 1].type;

        if (continuous_scalar_expression(following_type)) {
        return false;
        }

        if (following_type == TokenType::Not && closing_position + 2 < tokens_.size()) {

            TokenType after_not = tokens_[closing_position + 2].type;

            if (after_not == TokenType::Between || after_not == TokenType::In) {
                return false;
            }
        }

        return true;
    }


}