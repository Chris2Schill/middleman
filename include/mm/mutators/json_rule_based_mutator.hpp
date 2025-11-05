#pragma once

#include "packet_mutator.hpp"
#include <nlohmann/json.hpp>

#include <vector>

using json = nlohmann::json;



enum cond_operation {
    OP_INVALID      = -1,
    OP_EQUAL        = 0x00000001,
    OP_NOT_EQUAL    = 0x00000002,
    OP_LESS_THAN    = 0x00000004,
    OP_GREATER_THAN = 0x00000008,
};

static int condition_operation_from_string(const std::string& str) {
    if (str == "==") return OP_EQUAL;
    if (str == "!=") return OP_NOT_EQUAL;
    if (str == "<")  return OP_LESS_THAN;
    if (str == "<=") return (OP_LESS_THAN | OP_EQUAL);
    if (str == ">")  return OP_GREATER_THAN;
    if (str == ">=") return (OP_GREATER_THAN | OP_EQUAL);
    return OP_INVALID;
}

enum data_type {
    INVALID_DATA_TYPE,
    CHAR_TYPE,
    SHORT_TYPE,
    INT_TYPE,
    LONG_TYPE,
    UCHAR_TYPE,
    USHORT_TYPE,
    UINT_TYPE,
    ULONG_TYPE,
    FLOAT_TYPE,
    DOUBLE_TYPE,

    ARRAY_TYPE, // used for generic size paddings
};

inline data_type data_type_from_string(const std::string& str) {
    if (str == "int8") { return CHAR_TYPE; }
    if (str == "int16") { return SHORT_TYPE; }
    if (str == "int32") { return INT_TYPE; }
    if (str == "int64") { return LONG_TYPE; }
    if (str == "uint8") { return UCHAR_TYPE; }
    if (str == "uint16") { return USHORT_TYPE; }
    if (str == "uint32") { return UINT_TYPE; }
    if (str == "uint64") { return ULONG_TYPE; }
    if (str == "float") { return FLOAT_TYPE; }
    if (str == "double") { return DOUBLE_TYPE; }

    spdlog::error("Failed to convert " + str + " to data_type");
    return INVALID_DATA_TYPE;
}

// Returns 0 if type not found
inline int data_size_from_type(data_type type) {
    switch(type) {
        case CHAR_TYPE:   { return 1; }
        case SHORT_TYPE:  { return 2; }
        case INT_TYPE:    { return 4; }
        case LONG_TYPE:   { return 8; }
        case UCHAR_TYPE:   { return 1; }
        case USHORT_TYPE:  { return 2; }
        case UINT_TYPE:    { return 4; }
        case ULONG_TYPE:   { return 8; }
        case FLOAT_TYPE:  { return 4; }
        case DOUBLE_TYPE: { return 8; }
        default:
            break;
    }

    spdlog::error("type {} not found", (int)type);
    return 0;
}

inline int data_size_from_type_string(const std::string& type);

struct Condition {
    int data_offset;
    int data_size;
    data_type type;
    int operation; // cond_operation bit mask
    double value_d;
    uint64_t value_u;
    int64_t value_i;
};

struct Mutation 
{
    int data_offset;
    int data_size;
    data_type type;

    // jank af baby but it works. store all 3 types
    double new_value_d;
    uint64_t new_value_u;
    int64_t new_value_i;
};

using Mutations = std::vector<Mutation>;
using Conditions = std::vector<Condition>;

struct Rule {
    Conditions conditions;
    Mutations mutations;
};

struct packet_description {
    struct field {
        std::string name;
        int offset;
        data_type type;
        std::string type_str;
    };

    std::string name;
    std::string opcode_field;
    int opcode;
    std::vector<field> fields;
    std::unordered_map<std::string, const field*> fields_map;

    packet_description(std::string name,
                       std::string opcode_field,
                       int opcode,
                       const std::vector<field>& fields)
        :name(name)
        ,opcode_field(opcode_field)
        ,opcode(opcode)
        ,fields(fields)  {

        for(const auto& f: this->fields)  {
            fields_map.insert({f.name, &f});
            spdlog::debug("inserted {} @{} to {} fields map", f.name, (void*)&f, name);
        }
    }

    std::string dump() {
        std::stringstream ss;
        ss << name << ":\n"
           << "  opcode_field: " << opcode_field << "\n"
           << "  opcode: " << opcode << "\n"
           << "  fields:{" << '\n';
        for (auto& f : fields) {
            ss << "    " << f.name << ": " << f.type_str << " @ data[" << f.offset << "]" << "\n";
        }
        ss << "  }\n";
        ss << "}";
        return ss.str();
    }
};
using packet_types = std::vector<packet_description>;


namespace mm::mutators {

class json_rule_based_mutator : public packet_mutator {
    packet_types packet_types_list;
    std::vector<Rule> rules;

public:
    json_rule_based_mutator(const std::string& typesfile, const std::string& rulefile, bool to_big_endian = false);

    static std::shared_ptr<json_rule_based_mutator> fromJsonString(const std::string& typesFile, const std::string& jsonStr, bool bigEndian);

    bool mutate_packet(mm::network::BufferPtr readBuf,
                       mm::network::EndpointPtr sender,
                       std::size_t bytes) override;

private:
    static std::vector<Rule> parse_rules(const packet_types& packet_types, json data);
    const Mutations* next_mutation;
    bool to_network_byte_order;
};

}

inline int data_size_from_type_string(const std::string& type) {
    data_type dt = data_type_from_string(type);
    int size = data_size_from_type(dt);
    if (size == 0) {
        spdlog::error("Failed to get data size for type string {}",type);
    }
    return size;
}

