#include <mm/mutators/json_rule_based_mutator.hpp>
#include <mm/config_reader.hpp>
#include <byteswap.h>

static void swap_bytes(void *object, size_t size)
{
	// Swap the byte order of a given data unit and size to transmit / receive across a network.

	// Note: This function could be greatly optimized for speed later.
	unsigned char *start, *end;
	for ( start = (unsigned char*)object, end = start + size - 1; start < end; ++start, --end )
	{
		unsigned char swap = *start;
		*start = *end;
		*end = swap;
	}
}

static int parse_packets_data_field(const json& data, int offset, std::string field_name_prefix, std::vector<packet_description::field>& fields);
static packet_types packet_description_from_json(json j);


namespace mm::mutators {


json_rule_based_mutator::json_rule_based_mutator(const std::string& typesfile, const std::string& rulefile, bool to_big_endian) {

    spdlog::info("Parsing types file: " + typesfile);
    packet_types_list = packet_description_from_json(read_configuration(typesfile));
    for (auto& ptl : packet_types_list) {
        spdlog::info(ptl.dump());
    }

    if (!rulefile.empty()) {
        spdlog::info("Parsing rules file: " + rulefile);
        rules = parse_rules(packet_types_list, read_configuration(rulefile));
    }
    next_mutation = nullptr;

    to_network_byte_order = to_big_endian;
}

static const packet_description::field* get_field_ptr(const std::string& field_name, const packet_types& types_list) {
    for (const auto& packet_type : types_list) {
        // for (const auto& f : packet_type.fields) {
        //     if (f.name == field_name) {
        //         return &f;
        //     }
        // }
        auto iter = packet_type.fields_map.find(field_name);
        if (iter != packet_type.fields_map.end()) {
            return iter->second;
        }
    }
    spdlog::error("could not find field " + field_name);
    return nullptr;
}

std::vector<Rule> json_rule_based_mutator::parse_rules(const packet_types& packet_types, json data) {
    spdlog::info(data.dump(2));
    std::vector<Rule> rules;

    if (!data.contains("rules")) {
        spdlog::error("rules file does not contain a 'rules' object");
        return rules;
    }

    for (auto& rule_json : data["rules"]) {
        if (!rule_json.contains("conditions")) {
            spdlog::error("rule does not contain a 'conditions' object");
            continue;
        }
        if (!rule_json.contains("mutations")) {
            spdlog::error("rule does not contain a 'mutation' object");
            continue;
        }

        Rule rule = {};
        const json& conditions_json = rule_json["conditions"];
        if (!conditions_json.is_array()) {
            spdlog::error("conditions field is not an array");
            continue;
        }

        for (const json& condition_json : conditions_json) {
            try {
                std::string condition_field = condition_json["field"].get<std::string>();
                std::string operator_type = condition_json["operator"].get<std::string>();
                const packet_description::field* condition_field_ptr = get_field_ptr(condition_field, packet_types);
                if (!condition_field_ptr) {
                    spdlog::error("Failed to find field {} for condition", condition_field);
                    continue;
                }
                int data_size = data_size_from_type(condition_field_ptr->type);
                if (data_size <= 0) {
                    spdlog::error("Failed to find data size for condition field {}", condition_field); 
                    continue;
                }
                Condition cd{
                    .data_offset = condition_field_ptr->offset,
                        .data_size = data_size,
                        .type = condition_field_ptr->type,
                        .operation = condition_operation_from_string(operator_type),
                        .value_d = condition_json["value"].get<double>(),
                        .value_u = condition_json["value"].get<uint64_t>(),
                        .value_i = condition_json["value"].get<int64_t>(),
                };

                if (cd.operation == OP_INVALID) {
                    spdlog::error("Failed to convert " + operator_type + " to a valid cond_operation ");
                    continue;
                }

                if (cd.type == INVALID_DATA_TYPE) {
                    spdlog::error("Could not find valid data type for condition field " + condition_field);
                    continue;
                }
                rule.conditions.push_back(cd);
            }
            catch(...) {
                spdlog::error("Failed to parse condition");
            }
        }

        for (const auto& mutation_json : rule_json["mutations"]) {
            try {
                std::string field_name = mutation_json["field"].get<std::string>();
                const packet_description::field* field = get_field_ptr(field_name, packet_types);
                if (!field) {
                    spdlog::error("Failed to find field {} for mutation", field_name);
                    continue;
                }
                rule.mutations.push_back(Mutation{
                        .data_offset = field->offset,
                        .data_size = data_size_from_type(field->type),
                        .type = field->type,
                        .new_value_d = mutation_json["new_value"].get<double>(),
                        .new_value_u = mutation_json["new_value"].get<uint64_t>(),
                        .new_value_i = mutation_json["new_value"].get<int64_t>(),
                        });
            }
            catch(...) {
                spdlog::error("Failed to parse mutation");
            }
        }
        rules.push_back(rule);
    }

    return rules;
}


template<typename T>
static bool evaluate_operation(const void* field, int operation, T value, int size, bool byteswap) {
    if (byteswap) {
        swap_bytes(&value, size);
    }
    switch(operation) {
        case OP_EQUAL | OP_LESS_THAN:
            return (*static_cast<const T*>(field) <= value);
        case OP_EQUAL | OP_GREATER_THAN:
            return (*static_cast<const T*>(field) >= value);
        case OP_LESS_THAN:
            return (*static_cast<const T*>(field) < value);
        case OP_GREATER_THAN:
            return (*static_cast<const T*>(field) > value);
        case OP_EQUAL:
            return (*static_cast<const T*>(field) == value);
        case OP_NOT_EQUAL:
            return (*static_cast<const T*>(field) != value);
        case OP_INVALID:
            spdlog::error("Tried to evaluate a condition with an invalid operation");
            return false;
        default:
            spdlog::error("Tried to evaluate an unknown condition");
            return false;
    }
}

static bool evaluate_condition(const Condition& condition, mm::network::BufferPtr buffer, bool to_network_byte_order = false) {
    const void* data_ptr = static_cast<const void*>(&buffer->data()[condition.data_offset]);
    switch(condition.type) {
        case FLOAT_TYPE:
            return evaluate_operation<float>(data_ptr, condition.operation, condition.value_d, condition.data_size, to_network_byte_order);
        case DOUBLE_TYPE:
            return evaluate_operation<double>(data_ptr, condition.operation, condition.value_d,  condition.data_size, to_network_byte_order);
        case CHAR_TYPE:
            return evaluate_operation<int8_t>(data_ptr, condition.operation, condition.value_i,  condition.data_size, to_network_byte_order);
        case SHORT_TYPE:
            return evaluate_operation<int16_t>(data_ptr, condition.operation, condition.value_i,  condition.data_size, to_network_byte_order);
        case INT_TYPE:
            return evaluate_operation<int32_t>(data_ptr, condition.operation, condition.value_i,  condition.data_size, to_network_byte_order);
        case LONG_TYPE:
            return evaluate_operation<int64_t>(data_ptr, condition.operation, condition.value_i,  condition.data_size, to_network_byte_order);
        case UCHAR_TYPE:
            return evaluate_operation<uint8_t>(data_ptr, condition.operation, condition.value_u,  condition.data_size, to_network_byte_order);
        case USHORT_TYPE:
            return evaluate_operation<uint16_t>(data_ptr, condition.operation, condition.value_u,  condition.data_size, to_network_byte_order);
        case UINT_TYPE:
            return evaluate_operation<uint32_t>(data_ptr, condition.operation, condition.value_u,  condition.data_size, to_network_byte_order);
        case ULONG_TYPE:
            return evaluate_operation<uint64_t>(data_ptr, condition.operation, condition.value_u,  condition.data_size, to_network_byte_order);
        case ARRAY_TYPE:
            // TODO:
            return false;
            // return evaluate_operation<uint64_t>(data_ptr, condition.operation, condition.value_u,  condition.data_size, to_network_byte_order);
        default:
            break;
    }
    return false;
}

template<typename T>
static void set_field(void* field, const T value, int size, bool byteswap = false) {
    std::memcpy(field, &value, size);
    // TODO: Optimize later bitches
    if (byteswap) {
        swap_bytes(field, size);
    }
}

bool json_rule_based_mutator::mutate_packet(mm::network::BufferPtr readBuf,
                   mm::network::EndpointPtr sender,
                   std::size_t bytes) {

    bool mutated = false;
    for (const auto& rule : rules) {
        bool passed = true;
        for (const auto& condition : rule.conditions) {
            passed &= evaluate_condition(condition, readBuf, to_network_byte_order);
        }

        if (passed) {
            for (const auto& mutation : rule.mutations) {
                void* data_ptr = static_cast<void*>(&readBuf->data()[mutation.data_offset]);

                switch(mutation.type) {
                    case FLOAT_TYPE:
                        set_field<float>(data_ptr, mutation.new_value_d, mutation.data_size, to_network_byte_order);
                        break;
                    case DOUBLE_TYPE:
                        set_field<double>(data_ptr, mutation.new_value_d,  mutation.data_size, to_network_byte_order);
                        break;
                    case CHAR_TYPE:
                        set_field<int8_t>(data_ptr, mutation.new_value_i,  mutation.data_size, to_network_byte_order);
                        break;
                    case SHORT_TYPE:
                        set_field<int16_t>(data_ptr, mutation.new_value_i,  mutation.data_size, to_network_byte_order);
                        break;
                    case INT_TYPE:
                        set_field<int32_t>(data_ptr, mutation.new_value_i,  mutation.data_size, to_network_byte_order);
                        break;
                    case LONG_TYPE:
                        set_field<int64_t>(data_ptr, mutation.new_value_i,  mutation.data_size, to_network_byte_order);
                        break;
                    case UCHAR_TYPE:
                        set_field<uint8_t>(data_ptr, mutation.new_value_u,  mutation.data_size, to_network_byte_order);
                        break;
                    case USHORT_TYPE:
                        set_field<uint16_t>(data_ptr, mutation.new_value_u,  mutation.data_size, to_network_byte_order);
                        break;
                    case UINT_TYPE:
                        set_field<uint32_t>(data_ptr, mutation.new_value_u,  mutation.data_size, to_network_byte_order);
                        break;
                    case ULONG_TYPE:
                        set_field<uint64_t>(data_ptr, mutation.new_value_u,  mutation.data_size, to_network_byte_order);
                        break;
                    case ARRAY_TYPE:
                        // TODO:
                        // set_field<uint64_t>(data_ptr, mutation.new_value_u,  mutation.data_size, to_network_byte_order);
                        break;
                    default:
                        spdlog::error("Could not execute mutation: invalid type %d ", +mutation.type) ;
                        break;
                }
                mutated = true;
            }
        }
    }
    
    return mutated;
}

} // end namespace mm::mutators

static packet_types packet_description_from_json(json j) {
    packet_types pds;
    int cur_data_offset = 0;
    for (auto& packet_json : j["packets"]) {
        std::vector<packet_description::field> fields;
        std::string name = packet_json["name"].get<std::string>();
        std::string opcode_field = packet_json["opcode_field"].get<std::string>();
        int opcode = packet_json["opcode"].get<int>();
        const json& data = packet_json["data"];

        std::string field_name_prefix = "";
        cur_data_offset += parse_packets_data_field(data, cur_data_offset, field_name_prefix, fields);
        pds.push_back(packet_description(name, opcode_field, opcode, fields));
    }
    return pds;
}


// returns the new offset after parsing the data array
static int parse_packets_data_field(const json& data, int offset, std::string field_name_prefix, std::vector<packet_description::field>& fields) {
    for (auto& d : data)  {
        if (d.contains("struct")) {
            std::string struct_name = d["struct"].get<std::string>();
            spdlog::info("struct=" + struct_name);

            if (d.contains("data")) {
                const json& nested_data = d["data"];
                offset = parse_packets_data_field(nested_data, offset, struct_name + ".", fields);
            }
            else {
                spdlog::error("data entry is marked as a struct but is missing a data field");
            }
        }
        else {
            std::string field_name = d["value"].get<std::string>();

            if (d.contains("type")) {
                std::string field_type = d["type"].get<std::string>();

                fields.push_back({
                    .name = field_name_prefix + field_name,
                    .offset = offset,
                    .type = data_type_from_string(field_type),
                    .type_str = field_type,
                });
                // TODO: already string comped once, just use enum
                offset += data_size_from_type_string(field_type);
            }
            else if (d.contains("size")) {
                int field_size = d["size"].get<int>();

                fields.push_back({
                    .name = field_name_prefix + field_name,
                    .offset = offset,
                    .type = ARRAY_TYPE,
                    .type_str = "",
                });
                offset += field_size;
            }
            else {
                spdlog::error("Failed to parse field {} in types file", field_name);
            }
        }
    }
    return offset;
}

std::shared_ptr<mm::mutators::json_rule_based_mutator> mm::mutators::json_rule_based_mutator::fromJsonString(const std::string& typesFile, const std::string& jsonStr, bool to_big_endian){
    auto mutator = std::make_shared<mm::mutators::json_rule_based_mutator>(typesFile, "", to_big_endian);
    mutator->rules = parse_rules(mutator->packet_types_list, json::parse(jsonStr));
    return mutator;
}

