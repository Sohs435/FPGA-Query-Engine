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

    //Parser Constructor: set tokens_ to tokens and move the array so that tokens
    // object no longer contains anything. current index set to the start of the
    // vector so at index 0
    Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)), current_(0) {

        // Query is empty or doesnt end with a semicolon
        if (tokens_.empty() || tokens_.back().type != TokenType::End){
            throw std::invalid_argument ("Empty Query statement or Query doesn't end in a semicolon");
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

    // if we have type GreaterEqual (>=) {..., >, =, ...} should be {..., >=, ...}
    // current type is >
    // advance to next index
    // where current type is =
    // so overall we consume >= at the same time 
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

}