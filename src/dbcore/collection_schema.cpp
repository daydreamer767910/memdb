#include "collection_schema.hpp"

// 验证整个文档是否符合 schema
void CollectionSchema::validateDocument(const std::shared_ptr<Document> document, uint8_t depth) const {
	for (const auto& [fieldName, schema] : fieldSchemas_) {
		auto it = document->getFields().find(fieldName);
		if (it == document->getFields().end()) {
			// 检查必需字段是否缺失

			if (schema.getConstraints().required && 
				((0x0000000000000001<<depth)&schema.getConstraints().depth) != 0) {
				std::cerr << "Missing required field: " << fieldName << " in depth:" << depth << " \n";
				throw std::invalid_argument("Missing required field: " + fieldName + " in depth:" + std::to_string(depth));
				//return false;
			}
			continue;
		}

		auto field = document->getFields().at(fieldName);
		FieldValue value = field->getValue();

		if (!schema.validate(value, depth)) {
			std::cerr << "Invalid field: " << fieldName << " value: " << value << "\n";
			throw std::invalid_argument("Invalid field: " + fieldName);
			//return false;
		}

		if (schema.getType() == FieldType::DOCUMENT) {
			// For DOCUMENT type, recursively validate nested fields
			if (auto doc = std::get_if<std::shared_ptr<Document>>(&value)) {
				validateDocument(*doc, depth+1);
			} else {
				throw std::invalid_argument("Invalid nested document for field: " + fieldName);
			}
		}
	}
	//return true;
}
