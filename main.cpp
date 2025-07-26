#include <iostream>
#include <string>
#include <vector>
#include <expected>
#include <variant>
#include <fstream>
#include <sstream>
#include <stdexcept> // For std::runtime_error in main for unhandled cases

#include <cassert>
#include <typeinfo>

// Step 1: Define Custom Error Types
struct ConfigReadError {
    std::string filename;
};

struct ConfigParseError {
    std::string line_content;
    int line_number;
};

struct ValidationError {
    std::string field_name;
    std::string invalid_value;
};

struct ProcessingError {
    std::string task_name;
    std::string details;
};

// Step 2: Define a Global Error Variant for the entire pipeline
using PipelineError = std::variant<ConfigReadError, ConfigParseError, ValidationError, ProcessingError>;

// Helper for overloaded lambdas (C++17 style)
template<class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

template<class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

// Placeholder structs for data flowing through the pipeline
struct Config {
    std::string data;
};

struct ValidatedData {
    std::string processed_data;
};

struct Result {
    int final_result_code;
};

// Step 3: Implement Functions Returning std::expected with PipelineError
[[nodiscard]] std::expected<Config, PipelineError> LoadConfig(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "DEBUG: LoadConfig failed to open " << filename << std::endl;
        return std::unexpected(ConfigReadError{filename});
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // Simulate a parse error for empty config or specific content
    if (content.empty() || content.find("malformed") != std::string::npos) {
        std::cerr << "DEBUG: LoadConfig detected malformed config in " << filename << std::endl;
        return std::unexpected(ConfigParseError{"malformed", 1});
    }
    std::cout << "DEBUG: Config loaded successfully from " << filename << std::endl;
    return Config{content};
}

[[nodiscard]] std::expected<ValidatedData, PipelineError> ValidateData(const Config& config) {
    // Simulate a validation error
    if (config.data.find("invalid_field") != std::string::npos) {
        std::cerr << "DEBUG: ValidateData detected invalid field." << std::endl;
        return std::unexpected(ValidationError{"invalid_field", "contains disallowed value"});
    }
    std::cout << "DEBUG: Data validated successfully." << std::endl;
    return ValidatedData{"Validated: " + config.data};
}

[[nodiscard]] std::expected<Result, PipelineError> ProcessData(const ValidatedData& data) {
    // Simulate a processing error
    if (data.processed_data.length() < 10) {
        std::cerr << "DEBUG: ProcessData detected data too short." << std::endl;
        return std::unexpected(ProcessingError{"Data Processing", "Input data too short for task"});
    }
    std::cout << "DEBUG: Data processed successfully." << std::endl;
    return Result{static_cast<int>(data.processed_data.length())};
}

// Step 5: Handling the Final Result with std::visit
void handle_pipeline_result(const std::expected<Result, PipelineError>& final_result) {
    if (final_result) {
        std::cout << "\nPipeline Succeeded! Final Result Code: " << final_result->final_result_code << std::endl;
    } else {
        std::cerr << "\nPipeline Failed! Error details: ";
        std::visit(Overloaded {
            [](const ConfigReadError& e) {
                std::cerr << "Configuration Read Error: Could not open file '" << e.filename << "'" << std::endl;
            },
            [](const ConfigParseError& e) {
                std::cerr << "Configuration Parse Error: Malformed content at line "
                          << e.line_number << " (Context: '" << e.line_content << "')" << std::endl;
            },
            [](const ValidationError& e) {
                std::cerr << "Data Validation Error: Field '" << e.field_name
                          << "' has invalid value '" << e.invalid_value << "'" << std::endl;
            },
            [](const ProcessingError& e) {
                std::cerr << "Data Processing Error: Task '" << e.task_name
                          << "' failed. Details: " << e.details << std::endl;
            },
            // This generic lambda serves as a fallback for any unhandled types.
            // For strict compile-time enforcement of exhaustiveness, a static_assert(false,...)
            // could be used here if all types are expected to be handled.
            [](auto&& arg) {
                // This branch should ideally not be reachable if all specific error types are handled.
                // In a production system, this might log an unexpected error type.
                std::cerr << "An unexpected error type was encountered." << std::endl;
            }
        }, final_result.error());
    }
}

// A helper function for unit tests calling pipeline.
[[nodiscard]] std::expected<Result, PipelineError> call_pipeline(const std::string &configfile) {
    return LoadConfig(configfile)
       .and_then([](const Config& cfg) { return ValidateData(cfg); })
       .and_then([](const ValidatedData& vd) { return ProcessData(vd); });
}

void test_read_nonexisted_config_file() {
    const std::string filename = "this_file_should_not_exist.txt";
    ConfigReadError expected = {
        .filename = filename,
    };
    auto ret = call_pipeline(filename);

    assert(ret.has_value() == false);
    auto target = ret.error();
    ConfigReadError *c = std::get_if<ConfigReadError>(&target);
    assert(c != nullptr);
    assert(c->filename == expected.filename);

    std::cout << "test_read_nonexisted_config_file() passes" << std::endl;
}

int main() {
    // Scenario 1: Successful pipeline execution
    std::cout << "--- Scenario 1: Successful Execution ---" << std::endl;
    // Create a dummy file for successful config load
    std::ofstream("valid_config.txt") << "valid_data_content";
    auto success_pipeline = LoadConfig("valid_config.txt")
       .and_then([](const Config& cfg) { return ValidateData(cfg); })
       .and_then([](const ValidatedData& vd) { return ProcessData(vd); });
    handle_pipeline_result(success_pipeline);

    // Scenario 2: Config Read Error
    std::cout << "\n--- Scenario 2: Config Read Error ---" << std::endl;
    auto read_error_pipeline = LoadConfig("non_existent_config.txt")
       .and_then([](const Config& cfg) { return ValidateData(cfg); })
       .and_then([](const ValidatedData& vd) { return ProcessData(vd); });
    handle_pipeline_result(read_error_pipeline);

    // Scenario 3: Config Parse Error
    std::cout << "\n--- Scenario 3: Config Parse Error ---" << std::endl;
    // Simulate a file with "malformed" content
    std::ofstream("malformed_config.txt") << "malformed content";
    auto parse_error_pipeline = LoadConfig("malformed_config.txt")
       .and_then([](const Config& cfg) { return ValidateData(cfg); })
       .and_then([](const ValidatedData& vd) { return ProcessData(vd); });
    handle_pipeline_result(parse_error_pipeline);

    // Scenario 4: Validation Error
    std::cout << "\n--- Scenario 4: Validation Error ---" << std::endl;
    // Simulate a file with "invalid_field"
    std::ofstream("invalid_data_config.txt") << "valid_data\ninvalid_field";
    auto validation_error_pipeline = LoadConfig("invalid_data_config.txt")
       .and_then([](const Config& cfg) { return ValidateData(cfg); })
       .and_then([](const ValidatedData& vd) { return ProcessData(vd); });
    handle_pipeline_result(validation_error_pipeline);

    // Scenario 5: Processing Error
    std::cout << "\n--- Scenario 5: Processing Error ---" << std::endl;
    // Simulate a file with short content
    std::ofstream("short_data_config.txt") << "short";
    auto processing_error_pipeline = LoadConfig("short_data_config.txt")
       .and_then([](const Config& cfg) { return ValidateData(cfg); })
       .and_then([](const ValidatedData& vd) { return ProcessData(vd); });
    handle_pipeline_result(processing_error_pipeline);

    // Cleanup created files
    std::remove("valid_config.txt");
    std::remove("malformed_config.txt");
    std::remove("invalid_data_config.txt");
    std::remove("short_data_config.txt");

    // Conduct unit tests
    std::cout << "\n--- Start unit testing. ---" << std::endl;
    test_read_nonexisted_config_file();
    std::cout << "\n--- All unit tests pass. ---" << std::endl;

    return 0;
}
