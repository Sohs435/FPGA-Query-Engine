#include "fqe/csv_loader.hpp"

#include <charconv> // std::from_chars()
#include <cstdint> // int32_t, int64_t
#include <fstream> // std::ifstream
#include <stdexcept> //std::runtime_error, invalid_assignment
#include <string>
#include <system_error> // std::errc
#include <utility> //std::move
#include <vector>

namespace fqe {

    namespace {
        // no quoted field support yet 
        std::vector<std::string> split_csv_line (const std::string& line) {
        
            std::vector <std::string> values;

            std::size_t start =  0; 

            while (true){

                std::size_t comma = line.find(',', start);

                if (comma == std::string::npos) {// no comma -> convert full string to vector and brk
                    values.push_back(line.substr(start));
                    break; 
                }

                values.push_back(line.substr(start, comma - start));

                start = comma + 1; 
            }

            return values; //vector of strings returned
        }
        // "1200,600,1,100000001" -> {"1200", "600", "1", "100000001"}
        // string -> vector of substrings of string 

        // function that can parse into any integer type using template
        template <typename IntegerType> IntegerType parse_integer (const std::string& text, 
            std::size_t line_number, const std::string& column_name){ 

            // initial zero state for value incase parsing fails partway
            IntegerType value{};
        
            const char* start = text.data(); // pointer to string's first character 
            const char* end = start + text.size();  // end is one position after final character
            
            // from chars essentially works like stoi but is locale-indepent, non-allocating and
            // noexception. Parses character sequences into numeric values

            auto result = std::from_chars(start, end, value); // akin to
            // value = stoi(string.substr(start, end - start + 1)) 

            // ptr should point to the end character after result is generated
            
            // parse error occurs (empty/non-numeric/overflow) || parse succeded but stopped before
            // reaching end ex. "123 "/ "123abc" yeilds " " and "abc" after parsing which should still
            // throw an error 
            if (result.ec != std::errc{} || result.ptr != end){
                
                throw std::invalid_argument ("Invalid integer '" + text + "' at line " + 
                    std::to_string(line_number) + " in column '" + column_name + "'");
            }


            return value; 

        }


        // Remove a trailing carriage return so CRLF files parse correctly.
        // windows text files commonly use \r\n -> std::getline removes \n so we need to manually 
        // remove \r.
        void remove_carriage_return (std::string& line){

            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
        }

    }

    // convert csv to table
    Table load_csv (const std::string& file_path, Schema schema) {
        
        // opens file for reading (InputFileSTREAM)
        // no destructor needed cuz ifstream handles it 
        std::ifstream input(file_path);

        // runtime_error if input file is not open
        if (!input.is_open()){
            throw std::runtime_error ("Could not run CSV: " + file_path);
        }

        std::string line;

        //store input in line and return True if succeeds or False if reading fails 
        // will get header since everything in the top line is a header seperated by commas
        // at \n this stops and line is extracted 
        if (!std::getline(input, line)){
            throw std::invalid_argument("CSV is empty: " + file_path);
        }

        // remove \r from line 
        remove_carriage_return(line);

        // convert line into header vector 
        std::vector<std::string> header = split_csv_line(line);

        if (header.size() != schema.size()){
            throw std::invalid_argument ("CSV header count does not match schema");
        }

        for (std::size_t i = 0; i < header.size(); i++){

            if (header[i] != schema.field(i).name){
                throw std::invalid_argument ("CSV header mismatch at column " + std::to_string(i)
                    + ": expected '" + schema.field(i).name + "', received '" + header[i] + "'");
            }
        }

        std::vector<Column> columns;

        columns.reserve(schema.size()); //reserve enough memory for column objs 

        for (std::size_t i = 0; i < schema.size(); i++){
            columns.emplace_back(schema.field(i).type); 
            // Empty Column(DataType) placed at each index
        }

        std::size_t line_number = 1; 

        while (std::getline(input, line)){// while we can actually extract a line from input
            // i.e stop when we have reached the end

            line_number++; 

            remove_carriage_return(line);
            
            // blank line -> skip 
            if (line.empty()){
                continue;
            }
            
            // convert each line into a vector
            std::vector<std::string> values = split_csv_line(line);

            if (values.size() != schema.size()){
                throw std::invalid_argument("Incorrect number of values at line: " + 
                    std::to_string(line_number ));
            }

            for (std::size_t i = 0; i < values.size(); i++){

                switch (schema.field(i).type) {
                        
                    case DataType::Int32:

                        columns[i].append(parse_integer<std::int32_t> (values[i], line_number,
                            schema.field(i).name));// other 2 inputs are j for more useful err msg 

                        break;

                    case DataType::Int64:

                        columns[i].append(parse_integer<std::int64_t>(values[i], line_number,
                            schema.field(i).name));
                            
                            break;
                }
            }
        }
        // move doesnt do anything apart from telling compiler that resources have been transfered and
        // not copied
        return Table(std::move(schema), std::move(columns));
    }
    

}
