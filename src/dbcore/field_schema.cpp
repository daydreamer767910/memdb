#include "field_schema.hpp"

bool FieldSchema::validate(const FieldValue& value, uint8_t depth) const {
	if ((0x0000000000000001<<depth)&constraints_.depth == 0) return true;
	// 检查类型匹配
	if (!typeMatches(value,type_)) {
		std::cerr << "Field type mismatch, required type: " << type_ << std::endl;
		return false;
	}

	// 如果字段是空值且必需字段未提供值，则不合法
	if (std::holds_alternative<std::monostate>(value) && constraints_.required) {
		std::cerr << "Required field is missing.\n";
		return false;
	}

	// 其他约束可以扩展（如范围、正则表达式等）
	return true;
}