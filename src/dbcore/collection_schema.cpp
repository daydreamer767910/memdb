#include "collection_schema.hpp"

// 验证整个文档是否符合 schema
void CollectionSchema::validateDocument(std::shared_ptr<const Document> document, uint8_t depth) const {
	for (const auto& [fieldName, schema] : fieldSchemas_) {
		const Field* field = document->getField(fieldName);
		if (field == nullptr) {
			// 检查必需字段是否缺失
			if (schema.getConstraints().required && 
				((0x0000000000000001<<depth)&schema.getConstraints().depth) != 0) {
				std::cerr << "Missing required field: " << fieldName << " in depth:" << depth << " \n";
				throw std::invalid_argument("Missing required field: " + fieldName + " in depth:" + std::to_string(depth));
				//return false;
			}
			continue;
		}

		if (!schema.validate(*field, depth)) {
			std::cerr << "Invalid field: " << fieldName << " value: " << *field << "\n";
			throw std::invalid_argument("Invalid field: " + fieldName);
			//return false;
		}

		if (schema.getType() == FieldType::DOCUMENT) {
			// For DOCUMENT type, recursively validate nested fields
			FieldValue value = field->getValue();
			if (auto doc = std::get_if<std::shared_ptr<Document>>(&value)) {
				validateDocument(*doc, depth+1);
			} else {
				throw std::invalid_argument("Invalid nested document for field: " + fieldName);
			}
		}
	}
	//return true;
}
//指定路径验证field
void CollectionSchema::validateField(const std::string& path, const Field& field) const {
	uint8_t depth = static_cast<uint8_t>(std::count(path.begin(), path.end(), '.'));
	size_t pos = path.find_last_of('.');
	std::string fieldName;
    if (pos == std::string::npos) {
        fieldName = path;  // 没有 `.`，说明是顶层字段，直接返回整个路径
    } else {
		fieldName = path.substr(pos + 1);
	}
	
	auto it = fieldSchemas_.find(fieldName);
    if (it != fieldSchemas_.end()) {
        if (!it->second.validate(field, depth)) {
			std::cerr << "Invalid field: " << path << " value: " << field << "\n";
			throw std::invalid_argument("Invalid field: " + path);
		}
    }
}
